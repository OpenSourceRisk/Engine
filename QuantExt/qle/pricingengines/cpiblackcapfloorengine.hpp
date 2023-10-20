/*
 Copyright (C) 2016,2022 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/cpiblackcapfloorengine.hpp
    \brief CPI cap/floor engine using the Black pricing formula and interpreting the volatility data as lognormal vols
    \ingroup engines
*/

#ifndef quantext_cpi_black_capfloor_engine_hpp
#define quantext_cpi_black_capfloor_engine_hpp

#include <ql/instruments/cpicapfloor.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

//! Basse Class for Black / Bachelier CPI cap floor pricing engines
class CPICapFloorEngine : public QuantLib::CPICapFloor::engine {
public:
    CPICapFloorEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                      const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& surface,
                      const bool ttmFromLastAvailableFixing = false);

    virtual void calculate() const override;

    virtual ~CPICapFloorEngine() {}

    void setVolatility(const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& surface);

protected:
    virtual double optionPriceImpl(QuantLib::Option::Type type, double forward, double strike, double stdDev, double discount) const = 0;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::CPIVolatilitySurface> volatilitySurface_;
    bool ttmFromLastAvailableFixing_;
};

class CPIBlackCapFloorEngine : public CPICapFloorEngine {
public:
    CPIBlackCapFloorEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                           const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& surface,
                           const bool ttmFromLastAvailableFixing = false)
        : CPICapFloorEngine(discountCurve, surface, ttmFromLastAvailableFixing){};

    virtual ~CPIBlackCapFloorEngine() = default;

protected:
    virtual double optionPriceImpl(QuantLib::Option::Type type, double strike, double forward, double stdDev,
                                   double discount) const override;
};

} // namespace QuantExt

#endif
