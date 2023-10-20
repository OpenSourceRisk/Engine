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

#include <qle/pricingengines/inflationcapfloorengines.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

namespace QuantExt {

YoYInflationCapFloorEngine::YoYInflationCapFloorEngine(
    const ext::shared_ptr<QuantLib::YoYInflationIndex>& index,
    const Handle<QuantLib::YoYOptionletVolatilitySurface>& volatility, const Handle<YieldTermStructure>& discountCurve)
    : index_(index), volatility_(volatility), discountCurve_(discountCurve) {
    registerWith(index_);
    registerWith(volatility_);
    registerWith(discountCurve_);
}

void YoYInflationCapFloorEngine::setVolatility(const Handle<QuantLib::YoYOptionletVolatilitySurface>& v) {
    if (!volatility_.empty())
        unregisterWith(volatility_);
    volatility_ = v;
    registerWith(volatility_);
    update();
}

void YoYInflationCapFloorEngine::calculate() const {

    // copy black version then adapt to others

    Real value = 0.0, vega = 0.0;
    Size optionlets = arguments_.startDates.size();
    std::vector<Real> values(optionlets, 0.0);
    std::vector<Real> stdDevs(optionlets, 0.0);
    std::vector<Real> forwards(optionlets, 0.0);
    YoYInflationCapFloor::Type type = arguments_.type;

    Handle<YoYInflationTermStructure> yoyTS = index()->yoyInflationTermStructure();
    Handle<YieldTermStructure> discountTS = discountCurve_;
    QL_REQUIRE(!discountTS.empty(), "YoYInflationCapFloorEngine: No discount curve given.");
    Date settlement = discountTS->referenceDate();

    for (Size i = 0; i < optionlets; ++i) {
        Date paymentDate = arguments_.payDates[i];
        if (paymentDate > settlement) { // discard expired caplets
            DiscountFactor d = arguments_.nominals[i] * arguments_.gearings[i] * discountTS->discount(paymentDate) *
                               arguments_.accrualTimes[i];

            // We explicitly have the index and assume that
            // the fixing is natural, i.e. no convexity adjustment.
            // If that was required then we would also need
            // nominal vols in the pricing engine, i.e. a different engine.
            // This also means that we do not need the coupon to have
            // a pricing engine to return the swaplet rate and then
            // the adjusted fixing in the instrument.
            forwards[i] = yoyTS->yoyRate(arguments_.fixingDates[i], Period(0, Days));
            Rate forward = forwards[i];

            Date fixingDate = arguments_.fixingDates[i];
            Time sqrtTime = 0.0;
            if (fixingDate > volatility_->baseDate()) {
                sqrtTime = std::sqrt(volatility_->timeFromBase(fixingDate));
            }

            if (type == YoYInflationCapFloor::Cap || type == YoYInflationCapFloor::Collar) {
                Rate strike = arguments_.capRates[i];
                if (sqrtTime > 0.0) {
                    stdDevs[i] = std::sqrt(volatility_->totalVariance(fixingDate, strike, Period(0, Days)));
                }

                // sttDev=0 for already-fixed dates so everything on forward
                values[i] = optionletImpl(Option::Call, strike, forward, stdDevs[i], d);
                vega += optionletVegaImpl(Option::Call, strike, forward, stdDevs[i], sqrtTime, d);
            }
            if (type == YoYInflationCapFloor::Floor || type == YoYInflationCapFloor::Collar) {
                Rate strike = arguments_.floorRates[i];
                if (sqrtTime > 0.0) {
                    stdDevs[i] = std::sqrt(volatility_->totalVariance(fixingDate, strike, Period(0, Days)));
                }
                Real floorlet = optionletImpl(Option::Put, strike, forward, stdDevs[i], d);
                Real floorletVega = optionletVegaImpl(Option::Call, strike, forward, stdDevs[i], sqrtTime, d);
                if (type == YoYInflationCapFloor::Floor) {
                    values[i] = floorlet;
                    vega -= floorletVega;
                } else {
                    // a collar is long a cap and short a floor
                    values[i] -= floorlet;
                    vega -= optionletVegaImpl(Option::Call, strike, forward, stdDevs[i], sqrtTime, d);
                }
            }
            value += values[i];
        }
    }
    results_.value = value;

    results_.additionalResults["vega"] = vega;
    results_.additionalResults["optionletsPrice"] = values;
    results_.additionalResults["optionletsAtmForward"] = forwards;
    if (type != YoYInflationCapFloor::Collar)
        results_.additionalResults["optionletsStdDev"] = stdDevs;
}

//======================================================================
// pricer implementations
//======================================================================
YoYInflationBlackCapFloorEngine::YoYInflationBlackCapFloorEngine(
    const ext::shared_ptr<QuantLib::YoYInflationIndex>& index,
    const Handle<QuantLib::YoYOptionletVolatilitySurface>& volatility, const Handle<YieldTermStructure>& discountCurve)
    : YoYInflationCapFloorEngine(index, volatility, discountCurve) {}

Real YoYInflationBlackCapFloorEngine::optionletImpl(Option::Type type, Rate strike, Rate forward, Real stdDev,
                                                    Real d) const {
    return blackFormula(type, strike, forward, stdDev, d);
}

Real YoYInflationBlackCapFloorEngine::optionletVegaImpl(Option::Type type, Rate strike, Rate forward, Real stdDev,
                                                        Real sqrtTime, Real d) const {
    return blackFormulaStdDevDerivative(type, strike, forward, stdDev, d) * sqrtTime;
}

YoYInflationUnitDisplacedBlackCapFloorEngine ::YoYInflationUnitDisplacedBlackCapFloorEngine(
    const ext::shared_ptr<QuantLib::YoYInflationIndex>& index,
    const Handle<QuantLib::YoYOptionletVolatilitySurface>& volatility, const Handle<YieldTermStructure>& discountCurve)
    : YoYInflationCapFloorEngine(index, volatility, discountCurve) {}

Real YoYInflationUnitDisplacedBlackCapFloorEngine::optionletImpl(Option::Type type, Rate strike, Rate forward,
                                                                 Real stdDev, Real d) const {
    // could use displacement parameter in blackFormula but this is clearer
    return blackFormula(type, strike + 1.0, forward + 1.0, stdDev, d);
}

Real YoYInflationUnitDisplacedBlackCapFloorEngine::optionletVegaImpl(Option::Type type, Rate strike, Rate forward,
                                                                     Real stdDev, Real sqrtTime, Real d) const {
    return blackFormulaStdDevDerivative(type, strike + 1.0, forward + 1.0, stdDev, d) * sqrtTime;
}

YoYInflationBachelierCapFloorEngine::YoYInflationBachelierCapFloorEngine(
    const ext::shared_ptr<QuantLib::YoYInflationIndex>& index,
    const Handle<QuantLib::YoYOptionletVolatilitySurface>& volatility, const Handle<YieldTermStructure>& discountCurve)
    : YoYInflationCapFloorEngine(index, volatility, discountCurve) {}

Real YoYInflationBachelierCapFloorEngine::optionletImpl(Option::Type type, Rate strike, Rate forward, Real stdDev,
                                                        Real d) const {
    return bachelierBlackFormula(type, strike, forward, stdDev, d);
}

Real YoYInflationBachelierCapFloorEngine::optionletVegaImpl(Option::Type type, Rate strike, Rate forward, Real stdDev,
                                                            Real sqrtTime, Real d) const {
    return bachelierBlackFormulaStdDevDerivative(strike, forward, stdDev, d) * sqrtTime;
}

} // namespace QuantExt
