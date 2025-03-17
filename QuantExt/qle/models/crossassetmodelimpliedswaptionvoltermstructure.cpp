/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <iostream>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <qle/models/crossassetmodelimpliedswaptionvoltermstructure.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

namespace QuantExt {

CrossAssetModelImpliedSwaptionVolTermStructure::CrossAssetModelImpliedSwaptionVolTermStructure(
    const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
    const ext::shared_ptr<YieldTermStructure>& impliedDiscountCurve,
    const std::vector<QuantLib::ext::shared_ptr<IborIndex>>& impliedIborIndices,
    const ext::shared_ptr<SwapIndex>& swapIndex, const ext::shared_ptr<SwapIndex>& shortSwapIndex,
    BusinessDayConvention bdc, const DayCounter& dc, const bool purelyTimeBased)
    : SwaptionVolatilityStructure(bdc, dc == DayCounter() ? model->irlgm1f(0)->termStructure()->dayCounter() : dc),
      model_(model), ccyIndex_(model_->ccyIndex(swapIndex->currency())), impliedDiscountCurve_(impliedDiscountCurve),
      impliedIborIndices_(impliedIborIndices), swapIndex_(swapIndex), shortSwapIndex_(shortSwapIndex),
      purelyTimeBased_(purelyTimeBased),
      engine_(QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model_, ccyIndex_)),
      referenceDate_(purelyTimeBased ? Null<Date>() : model_->irlgm1f(0)->termStructure()->referenceDate()) {

    registerWith(model_);
    update();
}

void CrossAssetModelImpliedSwaptionVolTermStructure::state(const Real z) {
    state_ = z;
    engine_ =
        ext::make_shared<AnalyticLgmSwaptionEngine>(model_, ccyIndex_, Handle<YieldTermStructure>(),
                                                    AnalyticLgmSwaptionEngine::FloatSpreadMapping::proRata, state_);
}

void CrossAssetModelImpliedSwaptionVolTermStructure::move(const Date& d, const Real z) {
    state(z);
    referenceDate(d);
}

void CrossAssetModelImpliedSwaptionVolTermStructure::move(const Time t, const Real z) {
    state(z);
    referenceTime(t);
}

Real CrossAssetModelImpliedSwaptionVolTermStructure::volatilityImpl(Time optionTime, Time swapLength,
                                                                    Real strike) const {

    /********************************************************************
     * Locate relevant model implied iborIndex, depending on swap term  *
     ********************************************************************/

    Period swapTenor = static_cast<Size>(swapLength + 0.5) * Years;

    //auto swapIndex = swapTenor <= shortSwapIndex_->tenor() ? shortSwapIndex_ : swapIndex_;
    auto swapIndex = swapTenor > shortSwapIndex_->tenor() ? swapIndex_ : shortSwapIndex_;

    Size indexPosition = impliedIborIndices_.size();
    for (Size i = 0; i < impliedIborIndices_.size(); ++i) {
        if (impliedIborIndices_[i]->currency().code() == swapIndex->currency().code() &&
            impliedIborIndices_[i]->tenor() == swapIndex->iborIndex()->tenor()) {
            indexPosition = i;
            break;
        }
    }
    QL_REQUIRE(indexPosition < impliedIborIndices_.size(), "implied index not located");
    ext::shared_ptr<IborIndex> iborIndex = impliedIborIndices_[indexPosition];

    /*********************************************
     * Term structures                           *
     *********************************************/

    // FIXME: Copying the model implied term structures that were passed to the constructor
    // to make sure we decouple and have "proper" date based term structures with a reference date.
    // The model implied discount curve is "purely time-based", the index curve is not.
    std::vector<Period> tenorGrid = {1 * Days,   1 * Weeks,  2 * Weeks,  1 * Months, 2 * Months, 3 * Months, 6 * Months,
                                     9 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,  5 * Years,  6 * Years,
                                     7 * Years,  8 * Years,  9 * Years,  10 * Years, 12 * Years, 15 * Years, 20 * Years,
                                     25 * Years, 30 * Years, 35 * Years, 40 * Years};
    std::vector<Date> dateGrid(1, referenceDate_);
    std::vector<Real> dis(1, 1.0);
    std::vector<Real> fwd(1, 1.0);
    for (Size i = 0; i < tenorGrid.size(); ++i) {
        Date d = referenceDate_ + tenorGrid[i];
        Real t = iborIndex->forwardingTermStructure()->timeFromReference(d);
        dateGrid.push_back(d);
        dis.push_back(impliedDiscountCurve_->discount(t));
        fwd.push_back(iborIndex->forwardingTermStructure()->discount(d));
        // if (tenorGrid[i] == 1 * Days) {
        //     std::cout << tenorGrid[i] << " " << iborIndex->forwardingTermStructure()->referenceDate() << " " << t <<
        //     " "
        //               << dis.back() << " " << fwd.back() << std::endl;
        // }
    }
    auto disc = ext::make_shared<DiscountCurve>(dateGrid, dis, iborIndex->forwardingTermStructure()->dayCounter());
    auto fwdc = ext::make_shared<DiscountCurve>(dateGrid, fwd, iborIndex->forwardingTermStructure()->dayCounter());
    disc->enableExtrapolation(true);
    fwdc->enableExtrapolation(true);
    auto clonedIborIndex = iborIndex->clone(Handle<YieldTermStructure>(fwdc));
    Handle<YieldTermStructure> discountCurve(disc);

    /*************************************************************************
     * Forward starting ATM Swap, assuming exp tenor is a multiple of months *
     * below a year or a multiple of yeats above                             *
     *************************************************************************/

    Size settlementDays = 0;  //iborIndex->fixingDays();
    Period expiryTenor = optionTime < 1.0 ? static_cast<Size>(optionTime * 12 + 0.5) * Months
                                          : static_cast<Size>(optionTime + 0.5) * Years;
    Date expiry = referenceDate_ + expiryTenor + settlementDays * Days;
    Date startDate = expiry;
    Date endDate = startDate + swapTenor;

    Schedule fixedSchedule = MakeSchedule().from(startDate).to(endDate).withTenor(swapIndex->fixedLegTenor());
    Schedule floatSchedule = MakeSchedule().from(startDate).to(endDate).withTenor(iborIndex->tenor());
    Swap::Type type = Swap::Payer;
    Real nominal = 10000.0;
    Real fixedRate = 0.03;

    auto swap = ext::make_shared<VanillaSwap>(type, nominal, fixedSchedule, fixedRate, swapIndex->dayCounter(),
                                              floatSchedule, clonedIborIndex, 0.0, iborIndex->dayCounter());
    auto swapEngine = ext::make_shared<DiscountingSwapEngine>(discountCurve);
    swap->setPricingEngine(swapEngine);

    Real fairRate = swap->fairRate();
    //Real fairRate = 0.0244;
    auto atmSwap = ext::make_shared<VanillaSwap>(type, nominal, fixedSchedule, fairRate, swapIndex->dayCounter(),
                                                 floatSchedule, clonedIborIndex, 0.0, iborIndex->dayCounter());
    atmSwap->setPricingEngine(swapEngine);

    /*********************************************
     * Swaption                                  *
     *********************************************/

    auto exercise = ext::make_shared<EuropeanExercise>(expiry);
    auto swaption = ext::make_shared<Swaption>(atmSwap, exercise);
    swaption->setPricingEngine(engine_);
    Real price = 0.0, impliedVol = 0.0;
    Real accuracy = 1.0e-4; 
    Natural maxEvaluations = 100;
    Volatility minVol = 1.0e-8;
    Volatility maxVol = 4.0;
    VolatilityType volType = Normal;
    try {
        price = swaption->NPV();
        impliedVol =
            swaption->impliedVolatility(price, discountCurve, 0.1, accuracy, maxEvaluations, minVol, maxVol, volType);
    } catch (std::exception& e) {
        QL_FAIL("LGM Swaption pricing or implied vol calculation failed for expiry " << expiry << " and swap term "
                                                                                     << swapTenor << ": " << e.what());
    }

    // std::cout << "referenceDate= " << io::iso_date(referenceDate_)
    // 	    << " referenceDate= " << io::iso_date(iborIndex_->forwardingTermStructure()->referenceDate())
    // 	    << " ccyIndex=" << ccyIndex_
    // 	    << " expiry=" << expiryTenor
    // 	    << " swapTenor=" << swapTenor
    // 	    << " fairRate=" << std::setprecision(4) << swap->fairRate()
    // 	    << " swapNpv=" << std::setprecision(2) << swap->NPV()
    // 	    << " atmSwapNpv=" << atmSwap->NPV()
    // 	    << " swaptionNpv=" << price
    // 	    << " implied=" << impliedVol << std::endl;

    return impliedVol;
}

ext::shared_ptr<SmileSection> CrossAssetModelImpliedSwaptionVolTermStructure::smileSectionImpl(Time optionTime,
                                                                                               Time swapLength) const {
    // TODO
    return nullptr;
}

const Date& CrossAssetModelImpliedSwaptionVolTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure (1)");
    return referenceDate_;
}

void CrossAssetModelImpliedSwaptionVolTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure (2)");
    referenceDate_ = d;
    update();
}

void CrossAssetModelImpliedSwaptionVolTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure (3)");
    relativeTime_ = t;
}

const Period& CrossAssetModelImpliedSwaptionVolTermStructure::maxSwapTenor() const {
    // TODO
    return maxSwapTenor_;
}

void CrossAssetModelImpliedSwaptionVolTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(model_->irlgm1f(0)->termStructure()->referenceDate(), referenceDate_);
    }
    notifyObservers();
}

Date CrossAssetModelImpliedSwaptionVolTermStructure::maxDate() const { return Date::maxDate(); }

Time CrossAssetModelImpliedSwaptionVolTermStructure::maxTime() const { return QL_MAX_REAL; }

Real CrossAssetModelImpliedSwaptionVolTermStructure::minStrike() const { return 0.0; }

Real CrossAssetModelImpliedSwaptionVolTermStructure::maxStrike() const { return QL_MAX_REAL; }

} // namespace QuantExt
