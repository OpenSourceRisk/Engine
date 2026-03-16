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

/*! \file flatextrapolation.hpp
    \brief flat interpolation decorator
    \ingroup math
*/

#pragma once

#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %BiLinear-interpolation and flat extrapolation factory
class BilinearFlat {
    public:
    template <class I1, class I2, class M>
    Interpolation2D interpolate(const I1& xBegin, const I1& xEnd, const I2& yBegin, const I2& yEnd, const M& z) const {
            return FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(xBegin, xEnd, yBegin, yEnd, z));
    }
};


//! %BiCubicSpline-interpolation and flat extrapolation factory
class BicubicFlat {
public:
    template <class I1, class I2, class M>
    Interpolation2D interpolate(const I1& xBegin, const I1& xEnd, const I2& yBegin, const I2& yEnd, const M& z) const {
        return FlatExtrapolator2D(QuantLib::ext::make_shared<BicubicSpline>(xBegin, xEnd, yBegin, yEnd, z));
    }
};


} // namespace QuantExt

