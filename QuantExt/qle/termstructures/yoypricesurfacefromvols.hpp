/*
 Copyright (C) 2024 AcadiaSoft Inc.
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
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/experimental/inflation/yoycapfloortermpricesurface.hpp>
#pragma once

namespace QuantExt {
class YoYPriceSurfaceFromVolatilities {
public:
    QuantLib::ext::shared_ptr<YoYCapFloorTermPriceSurface>
    operator()(const QuantLib::ext::shared_ptr<QuantLib::CapFloorTermVolSurface>& volSurface,
               const QuantLib::ext::shared_ptr<YoYInflationIndex>& index,
               const QuantLib::Handle<QuantLib::YieldTermStructure>& nominalTs, QuantLib::VolatilityType type,
               QuantLib::Real displacement);
};
} // namespace QuantExt