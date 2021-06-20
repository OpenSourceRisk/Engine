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

/*! \file inflationcapfloorengines.hpp
    \brief Inflation cap/floor engines from QuantLib, with optional external discount curve
 */

#pragma once

#include <ql/instruments/inflationcapfloor.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

namespace QuantLib {
class Quote;
class YoYOptionletVolatilitySurface;
class YoYInflationIndex;
} // namespace QuantLib

namespace QuantExt {

using namespace QuantLib;

//! Base YoY inflation cap/floor engine
/*! This class doesn't know yet what sort of vol it is.  The
    inflation index must be linked to a yoy inflation term
    structure.  This provides the curves, hence the call uses a
    shared_ptr<> not a handle<> to the index.

    \ingroup inflationcapfloorengines
*/
class YoYInflationCapFloorEngine : public QuantLib::YoYInflationCapFloor::engine {
public:
    YoYInflationCapFloorEngine(const ext::shared_ptr<QuantLib::YoYInflationIndex>&,
                               const Handle<QuantLib::YoYOptionletVolatilitySurface>& vol,
                               const Handle<YieldTermStructure>& discountCurve);

    ext::shared_ptr<QuantLib::YoYInflationIndex> index() const { return index_; }
    Handle<QuantLib::YoYOptionletVolatilitySurface> volatility() const { return volatility_; }

    void setVolatility(const Handle<QuantLib::YoYOptionletVolatilitySurface>& vol);

    void calculate() const;

protected:
    //! descendents only need to implement this
    virtual Real optionletImpl(Option::Type type, Rate strike, Rate forward, Real stdDev, Real d) const = 0;
    virtual Real optionletVegaImpl(Option::Type type, Rate strike, Rate forward, Real stdDev, Real sqrtTime,
                                   Real d) const = 0;

    ext::shared_ptr<QuantLib::YoYInflationIndex> index_;
    Handle<QuantLib::YoYOptionletVolatilitySurface> volatility_;
    Handle<YieldTermStructure> discountCurve_;
};

//! Black-formula inflation cap/floor engine (standalone, i.e. no coupon pricer)
class YoYInflationBlackCapFloorEngine : public YoYInflationCapFloorEngine {
public:
    YoYInflationBlackCapFloorEngine(const ext::shared_ptr<QuantLib::YoYInflationIndex>&,
                                    const Handle<QuantLib::YoYOptionletVolatilitySurface>&,
                                    const Handle<YieldTermStructure>& discountCurve);

protected:
    virtual Real optionletImpl(Option::Type, Real strike, Real forward, Real stdDev, Real d) const;
    virtual Real optionletVegaImpl(Option::Type type, Rate strike, Rate forward, Real stdDev, Real sqrtTime,
                                   Real d) const;
};

//! Unit Displaced Black-formula inflation cap/floor engine (standalone, i.e. no coupon pricer)
class YoYInflationUnitDisplacedBlackCapFloorEngine : public YoYInflationCapFloorEngine {
public:
    YoYInflationUnitDisplacedBlackCapFloorEngine(const ext::shared_ptr<QuantLib::YoYInflationIndex>&,
                                                 const Handle<QuantLib::YoYOptionletVolatilitySurface>&,
                                                 const Handle<YieldTermStructure>& discountCurve);

protected:
    virtual Real optionletImpl(Option::Type, Real strike, Real forward, Real stdDev, Real d) const;
    virtual Real optionletVegaImpl(Option::Type type, Rate strike, Rate forward, Real stdDev, Real sqrtTime,
                                   Real d) const;
};

//! Unit Displaced Black-formula inflation cap/floor engine (standalone, i.e. no coupon pricer)
class YoYInflationBachelierCapFloorEngine : public YoYInflationCapFloorEngine {
public:
    YoYInflationBachelierCapFloorEngine(const ext::shared_ptr<QuantLib::YoYInflationIndex>&,
                                        const Handle<QuantLib::YoYOptionletVolatilitySurface>&,
                                        const Handle<YieldTermStructure>& discountCurve);

protected:
    virtual Real optionletImpl(Option::Type, Real strike, Real forward, Real stdDev, Real d) const;
    virtual Real optionletVegaImpl(Option::Type type, Rate strike, Rate forward, Real stdDev, Real sqrtTime,
                                   Real d) const;
};

} // namespace QuantExt
