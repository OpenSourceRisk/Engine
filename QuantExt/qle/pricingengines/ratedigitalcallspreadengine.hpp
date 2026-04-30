/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/ratedigitalcallspreadengine.hpp
    \brief Call-spread replication engine for European digital (cash-or-nothing) options on interest rates
    \ingroup engines

    Prices the digital as a call spread on vanilla caplet/floorlet prices:
        Digital(K) ≈ [C(K − ε/2) − C(K + ε/2)] / ε
    where C(K, σ(K)) is the vanilla call priced with the Black (or Bachelier)
    formula using market optionlet vol σ(K) from the capFloor surface.

    This is the standard model-free replication of a digital option and
    naturally incorporates the market smile.
*/

#ifndef quantext_ratedigitalcallspreadengine_hpp
#define quantext_ratedigitalcallspreadengine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

//! Prices a CashOrNothingPayoff via call-spread replication on market vols.
/*! For a digital call paying P if L > K:
        PV = P × [C(K − ε/2, σ−) − C(K + ε/2, σ+)] / ε
    For a digital put paying P if L < K:
        PV = P × [P(K + ε/2, σ+) − P(K − ε/2, σ−)] / ε

    The vanilla prices C / P are computed with Black (shifted-lognormal) or
    Bachelier (normal) formula depending on the optionlet surface type.

    \ingroup engines
*/
class RateDigitalCallSpreadEngine
    : public QuantLib::GenericEngine<QuantLib::VanillaOption::arguments, QuantLib::VanillaOption::results> {
public:
    /*! \param forward     Forward rate F at the fixing date
        \param dfPayment   Discount factor to the payment date
        \param ovs         Optionlet (capFloor) volatility surface
        \param fixingDate  Fixing / observation date
        \param eps         Width of the call-spread (in rate terms)
    */
    RateDigitalCallSpreadEngine(QuantLib::Real forward,
                                QuantLib::DiscountFactor dfPayment,
                                QuantLib::Handle<QuantLib::OptionletVolatilityStructure> ovs,
                                QuantLib::Date fixingDate,
                                QuantLib::Real eps = 1.0e-4);

    void calculate() const override;

private:
    QuantLib::Real forward_;
    QuantLib::DiscountFactor df_;
    QuantLib::Handle<QuantLib::OptionletVolatilityStructure> ovs_;
    QuantLib::Date fixingDate_;
    QuantLib::Real eps_;
};

} // namespace QuantExt

#endif
