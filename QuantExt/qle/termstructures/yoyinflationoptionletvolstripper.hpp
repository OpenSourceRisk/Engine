/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/yoyinflationoptionletvolstripper.hpp
    \brief YoY Inflation Optionlet (caplet/floorlet) volatility strippers
    \ingroup termstructures
*/

#ifndef quantext_yoyinflationoptionletvolstripper_hpp
#define quantext_yoyinflationoptionletvolstripper_hpp

#include <ql/indexes/inflationindex.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! Helper class to strip yoy inflation optionlet (i.e. caplet/floorlet) volatilities
    from the (cap/floor) term volatilities of a CapFloorTermVolSurface.
\ingroup termstructures
*/
class YoYInflationOptionletVolStripper {
public:
    YoYInflationOptionletVolStripper(const QuantLib::ext::shared_ptr<QuantLib::CapFloorTermVolSurface>& volSurface,
                                     const QuantLib::ext::shared_ptr<YoYInflationIndex>& index,
                                     const Handle<YieldTermStructure>& nominalTs,
                                     VolatilityType type = ShiftedLognormal, Real displacement = 0.0);

    const QuantLib::ext::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyInflationCapFloorVolSurface() const {
        return yoyOptionletVolSurface_;
    }

    //! \name LazyObject interface
    //@{
    void performCalculations();
    //@}
private:
    QuantLib::ext::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyOptionletVolSurface_;
    QuantLib::ext::shared_ptr<QuantLib::CapFloorTermVolSurface> volSurface_;
    QuantLib::ext::shared_ptr<YoYInflationIndex> yoyIndex_;
    Handle<YieldTermStructure> nominalTs_;
    VolatilityType type_;
    Real displacement_;
};
} // namespace QuantExt

#endif
