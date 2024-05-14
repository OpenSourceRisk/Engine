/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/makeoiscapfloor.hpp>
#include <qle/termstructures/oiscapfloorhelper.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/utilities/null_deleter.hpp>

#include <boost/bind/bind.hpp>
#include <ql/functional.hpp>

using namespace QuantLib;
using std::ostream;

namespace QuantExt {

OISCapFloorHelper::OISCapFloorHelper(CapFloorHelper::Type type, const Period& tenor,
                                     const Period& rateComputationPeriod, QuantLib::Rate strike,
                                     const Handle<Quote>& quote,
                                     const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& index,
                                     const Handle<YieldTermStructure>& discountingCurve, bool moving,
                                     const QuantLib::Date& effectiveDate, CapFloorHelper::QuoteType quoteType,
                                     QuantLib::VolatilityType quoteVolatilityType, QuantLib::Real quoteDisplacement)
    : RelativeDateBootstrapHelper<OptionletVolatilityStructure>(
          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<QuantLib::ext::function<Real(Real)>>>(
              quote, QuantLib::ext::bind(&OISCapFloorHelper::npv, this, QuantLib::ext::placeholders::_1)))),
      type_(type), tenor_(tenor), rateComputationPeriod_(rateComputationPeriod), strike_(strike), index_(index),
      discountHandle_(discountingCurve), moving_(moving), effectiveDate_(effectiveDate), quoteType_(quoteType),
      quoteVolatilityType_(quoteVolatilityType), quoteDisplacement_(quoteDisplacement), rawQuote_(quote),
      initialised_(false) {

    if (quoteType_ == CapFloorHelper::Premium) {
        QL_REQUIRE(type_ != CapFloorHelper::Automatic,
                   "Cannot have CapFloorHelper type 'Automatic' with quote type of Premium");
    }

    QL_REQUIRE(!(moving_ && effectiveDate_ != Date()),
               "A fixed effective date does not make sense for a moving helper");

    registerWith(index_);
    registerWith(discountHandle_);

    initializeDates();
    initialised_ = true;
}

void OISCapFloorHelper::initializeDates() {

    if (!initialised_ || moving_) {

        Date today = Settings::instance().evaluationDate();

        CapFloor::Type capFloorType = type_ == CapFloorHelper::Cap ? CapFloor::Cap : CapFloor::Floor;

        // Initialise the instrument and a copy
        // The strike can be Null<Real>() to indicate an ATM cap floor helper
        Rate dummyStrike = strike_ == Null<Real>() ? 0.01 : strike_;
        capFloor_ = MakeOISCapFloor(capFloorType, tenor_, index_, rateComputationPeriod_, dummyStrike)
                        .withEffectiveDate(effectiveDate_)
                        .withTelescopicValueDates(true);
        capFloorCopy_ = MakeOISCapFloor(capFloorType, tenor_, index_, rateComputationPeriod_, dummyStrike)
                            .withEffectiveDate(effectiveDate_)
                            .withTelescopicValueDates(true);

        QL_REQUIRE(!capFloor_.empty(), "OISCapFloorHelper: got empty leg.");

        maturityDate_ = CashFlows::maturityDate(capFloor_);

        // Earliest date is the first optionlet fixing date
        auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(capFloor_.front());
        QL_REQUIRE(cfon, "OISCapFloorHelper: Expected the first cashflow on the ois cap floor instrument to be a "
                         "CappedFlooredOvernightIndexedCoupon");
        earliestDate_ = std::max(today, cfon->underlying()->fixingDates().front());

        // Remaining dates are each equal to the fixing date on the final optionlet
        auto cfon2 = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(capFloor_.back());
        QL_REQUIRE(cfon2, "OISCapFloorHelper: Expected the final cashflow on the cap floor instrument to be a "
                          "CappedFlooredOvernightIndexedCoupon");
        pillarDate_ = latestDate_ = latestRelevantDate_ = cfon2->underlying()->fixingDates().back();
    }
}

void OISCapFloorHelper::setTermStructure(OptionletVolatilityStructure* ovts) {

    if (strike_ == Null<Real>()) {
        // If the strike is Null<Real>(), we want an ATM helper
        Rate atm = CashFlows::atmRate(getOisCapFloorUnderlying(capFloor_), **discountHandle_, false);
        CapFloor::Type capFloorType = type_ == CapFloorHelper::Cap ? CapFloor::Cap : CapFloor::Floor;
        capFloor_ = MakeOISCapFloor(capFloorType, tenor_, index_, rateComputationPeriod_, atm)
                        .withTelescopicValueDates(true)
                        .withEffectiveDate(effectiveDate_);
        capFloorCopy_ = MakeOISCapFloor(capFloorType, tenor_, index_, rateComputationPeriod_, atm)
                        .withTelescopicValueDates(true)
                            .withEffectiveDate(effectiveDate_);
    } else if (type_ == CapFloorHelper::Automatic && quoteType_ != CapFloorHelper::Premium) {
        // If the helper is set to automatically choose the underlying instrument type, do it now based on the ATM rate
        Rate atm = CashFlows::atmRate(getOisCapFloorUnderlying(capFloor_), **discountHandle_, false);
        CapFloor::Type capFloorType = atm > strike_ ? CapFloor::Floor : CapFloor::Cap;
        capFloor_ = MakeOISCapFloor(capFloorType, tenor_, index_, rateComputationPeriod_, strike_)
                        .withTelescopicValueDates(true)
                        .withEffectiveDate(effectiveDate_);
        capFloorCopy_ = MakeOISCapFloor(capFloorType, tenor_, index_, rateComputationPeriod_, strike_)
                            .withTelescopicValueDates(true)
                            .withEffectiveDate(effectiveDate_);
        for (auto const& c : capFloor_) {
	    auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c);
        }
    }

    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> temp(ovts, null_deleter());
    ovtsHandle_.linkTo(temp, false);
    RelativeDateBootstrapHelper<OptionletVolatilityStructure>::setTermStructure(ovts);

    auto pricer = QuantLib::ext::make_shared<BlackOvernightIndexedCouponPricer>(ovtsHandle_);
    for (auto c : capFloor_) {
        auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c);
        if (f) {
            f->setPricer(pricer);
        }
    }

    // If the quote type is not a premium, we will need to use capFloorCopy_ to return the premium from the volatility
    // quote
    if (quoteType_ != CapFloorHelper::Premium) {
        Handle<OptionletVolatilityStructure> constOvts;
        if (quoteVolatilityType_ == ShiftedLognormal) {
            constOvts = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
                0, NullCalendar(), Unadjusted, rawQuote_, Actual365Fixed(), ShiftedLognormal, quoteDisplacement_));
        } else {
            constOvts = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
                0, NullCalendar(), Unadjusted, rawQuote_, Actual365Fixed(), Normal));
        }
        auto pricer = QuantLib::ext::make_shared<BlackOvernightIndexedCouponPricer>(constOvts, true);
        for (auto c : capFloorCopy_) {
            auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c);
            if (f) {
                f->setPricer(pricer);
            }
        }
    }
}

Real OISCapFloorHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "CapFloorHelper's optionlet volatility term structure has not been set");
    for (auto c : capFloor_) {
        auto f = QuantLib::ext::dynamic_pointer_cast<LazyObject>(c);
        if (f) {
            f->deepUpdate();
        }
    }
    return CashFlows::npv(capFloor_, **discountHandle_, false);
}

void OISCapFloorHelper::accept(AcyclicVisitor& v) {
    if (Visitor<OISCapFloorHelper>* v1 = dynamic_cast<Visitor<OISCapFloorHelper>*>(&v))
        v1->visit(*this);
    else
        RelativeDateBootstrapHelper<OptionletVolatilityStructure>::accept(v);
}

Real OISCapFloorHelper::npv(Real quoteValue) {
    if (quoteType_ == CapFloorHelper::Premium) {
        return quoteValue;
    } else {
        // If the quote value is a volatility, return the premium
        return CashFlows::npv(capFloorCopy_, **discountHandle_, false);
    }
}

} // namespace QuantExt
