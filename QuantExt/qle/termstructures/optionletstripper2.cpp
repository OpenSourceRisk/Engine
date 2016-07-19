/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.
 
 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org
 
 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.
 
 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ql/instruments/makecapfloor.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <qle/termstructures/optionletstripper2.hpp>
#include <ql/termstructures/volatility/optionlet/spreadedoptionletvol.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

OptionletStripper2::OptionletStripper2(const boost::shared_ptr<OptionletStripper1>& optionletStripper1, 
    const Handle<CapFloorTermVolCurve>& atmCapFloorTermVolCurve, const VolatilityType type, const Real displacement)
    : OptionletStripper(optionletStripper1->termVolSurface(), optionletStripper1->iborIndex(),
                        Handle<YieldTermStructure>(), type, displacement),
      stripper1_(optionletStripper1), atmCapFloorTermVolCurve_(atmCapFloorTermVolCurve),
      dc_(stripper1_->termVolSurface()->dayCounter()), 
      nOptionExpiries_(atmCapFloorTermVolCurve->optionTenors().size()), atmCapFloorStrikes_(nOptionExpiries_), 
      atmCapFloorPrices_(nOptionExpiries_), spreadsVolImplied_(nOptionExpiries_), caps_(nOptionExpiries_), 
      maxEvaluations_(10000), accuracy_(1.e-6) {
    
    registerWith(stripper1_);
    registerWith(atmCapFloorTermVolCurve_);

    QL_REQUIRE(dc_ == atmCapFloorTermVolCurve->dayCounter(), "different day counters provided");
}

void OptionletStripper2::performCalculations() const {

    // optionletStripper data
    optionletDates_ = stripper1_->optionletFixingDates();
    optionletPaymentDates_ = stripper1_->optionletPaymentDates();
    optionletAccrualPeriods_ = stripper1_->optionletAccrualPeriods();
    optionletTimes_ = stripper1_->optionletFixingTimes();
    atmOptionletRate_ = stripper1_->atmOptionletRates();
    for (Size i = 0; i < optionletTimes_.size(); ++i) {
        optionletStrikes_[i] = stripper1_->optionletStrikes(i);
        optionletVolatilities_[i] = stripper1_->optionletVolatilities(i);
    }

    // atmCapFloorTermVolCurve data
    const vector<Period>& optionExpiriesTenors = atmCapFloorTermVolCurve_->optionTenors();
    const vector<Time>& optionExpiriesTimes = atmCapFloorTermVolCurve_->optionTimes();

    // discount curve
    const Handle<YieldTermStructure>& discountCurve =
        discount_.empty() ? iborIndex_->forwardingTermStructure() : discount_;

    for (Size j = 0; j < nOptionExpiries_; ++j) {
        // Dummy strike, doesn't get used for ATM curve
        Volatility atmOptionVol = atmCapFloorTermVolCurve_->volatility(optionExpiriesTimes[j], 33.3333);

        // Create a cap for each pillar point on ATM curve and attach relevant pricing engine i.e. Black if 
        // quotes are shifted lognormal and Bachelier if quotes are normal
        boost::shared_ptr<PricingEngine> engine;
        if (volatilityType_ == ShiftedLognormal) {
            engine = boost::make_shared<BlackCapFloorEngine>(discountCurve, atmOptionVol, dc_, displacement_);
        } else if (volatilityType_ == Normal) {
            engine = boost::make_shared<BachelierCapFloorEngine>(discountCurve, atmOptionVol, dc_);
        } else {
            QL_FAIL("unknown volatility type: " << volatilityType_);
        }
        caps_[j] = MakeCapFloor(CapFloor::Cap, optionExpiriesTenors[j], iborIndex_, Null<Rate>(), 0 * Days)
            .withPricingEngine(engine);

        // Get the ATM rate and the price
        atmCapFloorStrikes_[j] = caps_[j]->atmRate(**discountCurve);
        atmCapFloorPrices_[j] = caps_[j]->NPV();
    }

    spreadsVolImplied_ = spreadsVolImplied(discountCurve);

    StrippedOptionletAdapter adapter(stripper1_);

    Volatility unadjustedVol, adjustedVol;
    for (Size j = 0; j < nOptionExpiries_; ++j) {
        for (Size i = 0; i < optionletVolatilities_.size(); ++i) {
            if (i <= caps_[j]->floatingLeg().size()) {
                unadjustedVol = adapter.volatility(optionletTimes_[i], atmCapFloorStrikes_[j]);
                adjustedVol = unadjustedVol + spreadsVolImplied_[j];

                // insert adjusted volatility
                vector<Rate>::const_iterator previous =
                    lower_bound(optionletStrikes_[i].begin(), optionletStrikes_[i].end(), atmCapFloorStrikes_[j]);
                Size insertIndex = previous - optionletStrikes_[i].begin();

                optionletStrikes_[i].insert(optionletStrikes_[i].begin() + insertIndex, atmCapFloorStrikes_[j]);
                optionletVolatilities_[i].insert(optionletVolatilities_[i].begin() + insertIndex, adjustedVol);
            }
        }
    }
}

vector<Volatility> OptionletStripper2::spreadsVolImplied(const Handle<YieldTermStructure>& discount) const {

    Brent solver;
    vector<Volatility> result(nOptionExpiries_);
    Volatility guess = 0.0001, minSpread = -0.1, maxSpread = 0.1;
    for (Size j = 0; j < nOptionExpiries_; ++j) {
        ObjectiveFunction f(stripper1_, caps_[j], atmCapFloorPrices_[j], discount);
        solver.setMaxEvaluations(maxEvaluations_);
        Volatility root = solver.solve(f, accuracy_, guess, minSpread, maxSpread);
        result[j] = root;
    }
    return result;
}

vector<Volatility> OptionletStripper2::spreadsVol() const {
    calculate();
    return spreadsVolImplied_;
}

vector<Rate> OptionletStripper2::atmCapFloorStrikes() const {
    calculate();
    return atmCapFloorStrikes_;
}

vector<Real> OptionletStripper2::atmCapFloorPrices() const {
    calculate();
    return atmCapFloorPrices_;
}

// OptionletStripper2::ObjectiveFunction
OptionletStripper2::ObjectiveFunction::ObjectiveFunction(
    const boost::shared_ptr<OptionletStripper1>& optionletStripper1, const boost::shared_ptr<CapFloor>& cap,
    Real targetValue, const Handle<YieldTermStructure>& discount)
    : cap_(cap), targetValue_(targetValue), discount_(discount) {
    boost::shared_ptr<OptionletVolatilityStructure> adapter(new StrippedOptionletAdapter(optionletStripper1));

    // set an implausible value, so that calculation is forced
    // at first operator()(Volatility x) call
    spreadQuote_ = boost::shared_ptr<SimpleQuote>(new SimpleQuote(-1.0));

    boost::shared_ptr<OptionletVolatilityStructure> spreadedAdapter(
        new SpreadedOptionletVolatility(Handle<OptionletVolatilityStructure>(adapter), Handle<Quote>(spreadQuote_)));

    // Use the same target volatility type as optionletStripper1
    // Anything else would not make sense
    boost::shared_ptr<PricingEngine> engine;
    if (optionletStripper1->targetVolatilityType() == ShiftedLognormal) {
        engine = boost::make_shared<BlackCapFloorEngine>(discount_, 
            Handle<OptionletVolatilityStructure>(spreadedAdapter), optionletStripper1->targetDisplacement());
    } else if (optionletStripper1->targetVolatilityType() == Normal) {
        engine = boost::make_shared<BachelierCapFloorEngine>(discount_, 
            Handle<OptionletVolatilityStructure>(spreadedAdapter));
    } else {
        QL_FAIL("Unknown volatility type: " << optionletStripper1->targetVolatilityType());
    }

    cap_->setPricingEngine(engine);
}

Real OptionletStripper2::ObjectiveFunction::operator()(Volatility s) const {
    if (s != spreadQuote_->value())
        spreadQuote_->setValue(s);
    return cap_->NPV() - targetValue_;
}
}
