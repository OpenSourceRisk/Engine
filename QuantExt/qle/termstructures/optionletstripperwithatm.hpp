/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/optionletstripperwithatm.hpp
    \brief Optionlet stripper that amends existing stripped optionlets to incorporate ATM cap floor volatilities
    \ingroup termstructures
*/

#ifndef quantext_optionletstripper_with_atm_hpp
#define quantext_optionletstripper_with_atm_hpp

#include <qle/termstructures/capfloortermvolcurve.hpp>
#include <qle/termstructures/optionletstripper.hpp>
#include <qle/termstructures/spreadedoptionletvolatility.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>
#include <qle/instruments/makeoiscapfloor.hpp>

#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>

namespace QuantExt {

/*! Optionlet stripper that amends existing stripped optionlets to incorporate ATM cap floor
    term volatilities
    \ingroup termstructures
*/
template <class TimeInterpolator, class SmileInterpolator>
class OptionletStripperWithAtm : public QuantExt::OptionletStripper {

public:
    //! Constructor
    OptionletStripperWithAtm(const QuantLib::ext::shared_ptr<QuantExt::OptionletStripper>& osBase,
                             const QuantLib::Handle<CapFloorTermVolCurve>& atmCurve,
                             const QuantLib::Handle<QuantLib::YieldTermStructure>& discount =
                                 QuantLib::Handle<QuantLib::YieldTermStructure>(),
                             const QuantLib::VolatilityType atmVolatilityType = QuantLib::ShiftedLognormal,
                             QuantLib::Real atmDisplacement = 0.0, QuantLib::Size maxEvaluations = 10000,
                             QuantLib::Real accuracy = 1.0e-12, const TimeInterpolator& ti = TimeInterpolator(),
                             const SmileInterpolator& si = SmileInterpolator());

    //! \name Inspectors
    std::vector<QuantLib::Rate> atmStrikes() const;
    std::vector<QuantLib::Real> atmPrices() const;
    std::vector<QuantLib::Volatility> volSpreads() const;
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

private:
    //! Return the implied optionlet spreads to retrieve the ATM cap floor term volatilities
    std::vector<QuantLib::Volatility> volSpreads(const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
                                                 const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& ovs,
                                                 const bool isOis) const;

    //! Class that is used to imply the spreads at each tenor such that the ATM cap floor volatilities are retrieved
    class ObjectiveFunction {
    public:
        ObjectiveFunction(const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& ovs,
                          const QuantLib::ext::shared_ptr<QuantLib::CapFloor>& cap, QuantLib::Real targetValue,
                          const QuantLib::Handle<QuantLib::YieldTermStructure>& discount);

        Real operator()(QuantLib::Volatility volSpread) const;

    private:
        QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> spreadQuote_;
        QuantLib::ext::shared_ptr<QuantLib::CapFloor> cap_;
        QuantLib::Real targetValue_;
        QuantLib::Handle<QuantLib::YieldTermStructure> discount_;
    };

    class ObjectiveFunctionOIS {
    public:
        ObjectiveFunctionOIS(const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& ovs, const Leg& cap,
                             QuantLib::Real targetValue,
                             const QuantLib::Handle<QuantLib::YieldTermStructure>& discount);

        Real operator()(QuantLib::Volatility volSpread) const;

    private:
        QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> spreadQuote_;
        Leg cap_;
        QuantLib::Real targetValue_;
        QuantLib::Handle<QuantLib::YieldTermStructure> discount_;
    };

    //! Base optionlet stripper
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> osBase_;

    //! ATM volatility curve
    QuantLib::Handle<CapFloorTermVolCurve> atmCurve_;

    //! ATM volatility type
    QuantLib::VolatilityType atmVolatilityType_;

    //! ATM volatility shift if the ATM volatility type is ShiftedLognormal
    QuantLib::Real atmDisplacement_;

    //! Maximum number of evaluations when searching for spread to match ATM volatilities
    QuantLib::Size maxEvaluations_;

    //! Required accuracy when searching for spread to match ATM volatilities
    QuantLib::Real accuracy_;

    //! The interpolation object in the time direction
    TimeInterpolator ti_;

    //! The interpolation object in the strike direction
    SmileInterpolator si_;

    //! The day counter for atmCurve_ and osBase_
    DayCounter dayCounter_;

    //! The number of ATM instruments in the ATM curve, atmCurve_
    Size nAtmExpiries_;

    mutable std::vector<QuantLib::Rate> atmStrikes_;
    mutable std::vector<QuantLib::Real> atmPrices_;
    mutable std::vector<QuantLib::Volatility> volSpreads_;
    mutable std::vector<QuantLib::ext::shared_ptr<QuantLib::CapFloor> > caps_;
    mutable std::vector<QuantLib::Leg> capsOIS_;
};

template <class TimeInterpolator, class SmileInterpolator>
OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::OptionletStripperWithAtm(
    const QuantLib::ext::shared_ptr<QuantExt::OptionletStripper>& osBase,
    const QuantLib::Handle<CapFloorTermVolCurve>& atmCurve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount, const QuantLib::VolatilityType atmVolatilityType,
    QuantLib::Real atmDisplacement, QuantLib::Size maxEvaluations, QuantLib::Real accuracy, const TimeInterpolator& ti,
    const SmileInterpolator& si)
    : QuantExt::OptionletStripper(osBase->termVolSurface(), osBase->index(), discount, osBase->volatilityType(),
                                  osBase->displacement(), osBase->rateComputationPeriod()),
      osBase_(osBase), atmCurve_(atmCurve), atmVolatilityType_(atmVolatilityType), atmDisplacement_(atmDisplacement),
      maxEvaluations_(maxEvaluations), accuracy_(accuracy), dayCounter_(osBase_->termVolSurface()->dayCounter()),
      nAtmExpiries_(atmCurve_->optionTenors().size()), atmStrikes_(nAtmExpiries_), atmPrices_(nAtmExpiries_),
      volSpreads_(nAtmExpiries_), caps_(nAtmExpiries_), capsOIS_(nAtmExpiries_) {

    registerWith(osBase_);
    registerWith(atmCurve_);

    QL_REQUIRE(dayCounter_ == atmCurve_->dayCounter(),
               "The ATM curve day counter should equal that of the underlying base optionlet stripper");
}

template <class TimeInterpolator, class SmileInterpolator>
inline std::vector<QuantLib::Rate> OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::atmStrikes() const {
    calculate();
    return atmStrikes_;
}

template <class TimeInterpolator, class SmileInterpolator>
inline std::vector<QuantLib::Real> OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::atmPrices() const {
    calculate();
    return atmPrices_;
}

template <class TimeInterpolator, class SmileInterpolator>
inline std::vector<QuantLib::Volatility>
OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::volSpreads() const {
    calculate();
    return volSpreads_;
}

template <class TimeInterpolator, class SmileInterpolator>
void OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::performCalculations() const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::BachelierCapFloorEngine;
    using QuantLib::BlackCapFloorEngine;
    using QuantLib::Handle;
    using QuantLib::MakeCapFloor;
    using QuantLib::OptionletVolatilityStructure;
    using QuantLib::Period;
    using QuantLib::Size;
    using QuantLib::Time;
    using QuantLib::Volatility;
    using QuantLib::YieldTermStructure;
    using std::vector;

    bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index_) != nullptr;

    // Possible update based on underlying optionlet stripper data
    optionletDates_ = osBase_->optionletFixingDates();
    optionletPaymentDates_ = osBase_->optionletPaymentDates();
    optionletAccrualPeriods_ = osBase_->optionletAccrualPeriods();
    optionletTimes_ = osBase_->optionletFixingTimes();
    atmOptionletRate_ = osBase_->atmOptionletRates();
    for (Size i = 0; i < optionletTimes_.size(); ++i) {
        optionletStrikes_[i] = osBase_->optionletStrikes(i);
        optionletVolatilities_[i] = osBase_->optionletVolatilities(i);
    }

    // ATM curve tenors
    const vector<Period>& atmTenors = atmCurve_->optionTenors();
    vector<Time> atmTimes(atmTenors.size());
    for (Size i = 0; i < atmTenors.size(); ++i) {
        Date d = atmCurve_->optionDateFromTenor(atmTenors[i]);
        atmTimes[i] = atmCurve_->timeFromReference(d);
    }

    // discount curve
    const Handle<YieldTermStructure>& discountCurve = discount_.empty() ? index_->forwardingTermStructure() : discount_;

    // Populate ATM strikes and prices
    for (Size j = 0; j < nAtmExpiries_; ++j) {

        if (isOis) {

            Volatility atmVol = atmCurve_->volatility(atmTimes[j], 0.01);
            auto pricer = QuantLib::ext::make_shared<BlackOvernightIndexedCouponPricer>(
                Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
                    0, NullCalendar(), Unadjusted, atmVol, Actual365Fixed(), volatilityType(), false)));
            capsOIS_[j] =
                MakeOISCapFloor(CapFloor::Cap, atmTenors[j], QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index_),
                                osBase_->rateComputationPeriod(), Null<Real>(), discountCurve)
                .withCouponPricer(pricer);
            QL_REQUIRE(!capsOIS_[j].empty(),
                       "OptionletStripperWithAtm: internal error: empty cap for expiry " << atmTenors[j]);
            atmStrikes_[j] = getOisCapFloorStrikes(capsOIS_[j]).front().first;
            atmPrices_[j] = QuantLib::CashFlows::npv(capsOIS_[j], **discountCurve, false);

        } else {

            // Create a cap for each pillar point on ATM curve and attach relevant pricing engine
            Volatility atmVol = atmCurve_->volatility(atmTimes[j], 0.01);
            QuantLib::ext::shared_ptr<PricingEngine> engine;
            if (atmVolatilityType_ == ShiftedLognormal) {
                engine = QuantLib::ext::make_shared<BlackCapFloorEngine>(discountCurve, atmVol, dayCounter_, atmDisplacement_);
            } else if (atmVolatilityType_ == Normal) {
                engine = QuantLib::ext::make_shared<BachelierCapFloorEngine>(discountCurve, atmVol, dayCounter_);
            } else {
                QL_FAIL("Unknown volatility type: " << atmVolatilityType_);
            }

            // Using Null<Rate>() as strike => strike will be set to ATM rate. However, to calculate ATM rate, QL
            // requires a BlackCapFloorEngine to be set (not a BachelierCapFloorEngine)! So, need a temp
            // BlackCapFloorEngine with a dummy vol to calculate ATM rate. Needs to be fixed in QL.
            QuantLib::ext::shared_ptr<PricingEngine> tempEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(discountCurve, 0.01);
            caps_[j] =
                MakeCapFloor(CapFloor::Cap, atmTenors[j], index_, Null<Rate>(), 0 * Days).withPricingEngine(tempEngine);

            // Now set correct engine and get the ATM rate and the price
            caps_[j]->setPricingEngine(engine);
            atmStrikes_[j] = caps_[j]->atmRate(**discountCurve);
            atmPrices_[j] = caps_[j]->NPV();
        }
    }

    // Create an optionlet volatility structure from the underlying stripped optionlet surface
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovs =
        QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator> >(
            atmCurve_->referenceDate(), osBase_);
    ovs->enableExtrapolation();

    // Imply the volatility spreads that match the ATM prices
    volSpreads_ = volSpreads(discountCurve, Handle<OptionletVolatilityStructure>(ovs), isOis);

    // This stripped optionlet surface is the underlying stripped optionlet surface modified by the implied spreads
    for (Size j = 0; j < nAtmExpiries_; ++j) {
        for (Size i = 0; i < optionletTimes_.size(); ++i) {
            if (i < (isOis ? capsOIS_[j].size() : caps_[j]->floatingLeg().size())) {

                Volatility unadjustedVol = ovs->volatility(optionletTimes_[i], atmStrikes_[j]);
                Volatility adjustedVol = unadjustedVol + volSpreads_[j];

                // insert ATM strike and adjusted volatility
                vector<Rate>::const_iterator previous =
                    lower_bound(optionletStrikes_[i].begin(), optionletStrikes_[i].end(), atmStrikes_[j]);
                Size insertIndex = previous - optionletStrikes_[i].begin();
                optionletStrikes_[i].insert(optionletStrikes_[i].begin() + insertIndex, atmStrikes_[j]);
                optionletVolatilities_[i].insert(optionletVolatilities_[i].begin() + insertIndex, adjustedVol);
            }
        }
    }
}

template <class TimeInterpolator, class SmileInterpolator>
inline std::vector<QuantLib::Volatility> OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::volSpreads(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& ovs, const bool isOis) const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Size;
    using QuantLib::Volatility;
    using std::vector;

    QuantLib::Brent solver;
    vector<Volatility> result(nAtmExpiries_);
    Volatility guess = 0.0001;
    Volatility minSpread = -0.1;
    Volatility maxSpread = 0.1;
    solver.setMaxEvaluations(maxEvaluations_);

    for (Size j = 0; j < nAtmExpiries_; ++j) {
        if (isOis) {
            ObjectiveFunctionOIS f(ovs, capsOIS_[j], atmPrices_[j], discount);
            result[j] = solver.solve(f, accuracy_, guess, minSpread, maxSpread);
        } else {
            ObjectiveFunction f(ovs, caps_[j], atmPrices_[j], discount);
            result[j] = solver.solve(f, accuracy_, guess, minSpread, maxSpread);
        }
    }

    return result;
}

template <class TimeInterpolator, class SmileInterpolator>
OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::ObjectiveFunction::ObjectiveFunction(
    const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& ovs,
    const QuantLib::ext::shared_ptr<QuantLib::CapFloor>& cap, QuantLib::Real targetValue,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount)
    : cap_(cap), targetValue_(targetValue), discount_(discount) {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Handle;
    using QuantLib::OptionletVolatilityStructure;
    using QuantLib::Quote;
    using QuantLib::SimpleQuote;

    // set an implausible value, so that calculation is forced at first operator()(Volatility x) call
    spreadQuote_ = QuantLib::ext::make_shared<SimpleQuote>(-1.0);

    // Spreaded optionlet volatility structure that will be used to price the ATM cap
    Handle<OptionletVolatilityStructure> spreadedOvs = Handle<OptionletVolatilityStructure>(
        QuantLib::ext::make_shared<QuantExt::SpreadedOptionletVolatility>(ovs, Handle<Quote>(spreadQuote_)));

    // Attach the relevant engine to the cap with the spreaded optionlet volatility structure
    if (ovs->volatilityType() == ShiftedLognormal) {
        cap_->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(discount_, spreadedOvs, ovs->displacement()));
    } else if (ovs->volatilityType() == Normal) {
        cap_->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(discount_, spreadedOvs));
    } else {
        QL_FAIL("Unknown volatility type: " << ovs->volatilityType());
    }
}

template <class TimeInterpolator, class SmileInterpolator>
OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::ObjectiveFunctionOIS::ObjectiveFunctionOIS(
    const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& ovs, const Leg& cap, QuantLib::Real targetValue,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount)
    : cap_(cap), targetValue_(targetValue), discount_(discount) {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Handle;
    using QuantLib::OptionletVolatilityStructure;
    using QuantLib::Quote;
    using QuantLib::SimpleQuote;

    // set an implausible value, so that calculation is forced at first operator()(Volatility x) call
    spreadQuote_ = QuantLib::ext::make_shared<SimpleQuote>(-1.0);

    // Spreaded optionlet volatility structure that will be used to price the ATM cap
    Handle<OptionletVolatilityStructure> spreadedOvs = Handle<OptionletVolatilityStructure>(
        QuantLib::ext::make_shared<QuantExt::SpreadedOptionletVolatility>(ovs, Handle<Quote>(spreadQuote_)));

    auto pricer = QuantLib::ext::make_shared<BlackOvernightIndexedCouponPricer>(spreadedOvs, false);
    for (auto& c : cap_) {
        if (auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c))
            f->setPricer(pricer);
    }
}

template <class TimeInterpolator, class SmileInterpolator>
QuantLib::Real OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::ObjectiveFunction::operator()(
    QuantLib::Volatility volSpread) const {
    if (volSpread != spreadQuote_->value()) {
        spreadQuote_->setValue(volSpread);
    }
    return cap_->NPV() - targetValue_;
}

template <class TimeInterpolator, class SmileInterpolator>
QuantLib::Real OptionletStripperWithAtm<TimeInterpolator, SmileInterpolator>::ObjectiveFunctionOIS::operator()(
    QuantLib::Volatility volSpread) const {
    if (volSpread != spreadQuote_->value()) {
        spreadQuote_->setValue(volSpread);
    }
    return QuantLib::CashFlows::npv(cap_, **discount_, false) - targetValue_;
}

} // namespace QuantExt

#endif
