/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file constantinterpolation.hpp
    \brief flat interpolation decorator
    \ingroup math
*/

#pragma once

#include <ql/math/interpolation.hpp>

namespace QuantExt {

using QuantLib::Real;

//! %Constant interpolation
/*! \ingroup interpolations
    \warning See the Interpolation class for information about the
             required lifetime of the underlying data.
*/
class ConstantInterpolation : public QuantLib::Interpolation {
public:
    ConstantInterpolation(const Real& y) { impl_ = QuantLib::ext::make_shared<ConstantInterpolationImpl>(y); }

private:
    struct ConstantInterpolationImpl : public Interpolation::Impl {
        ConstantInterpolationImpl(const Real& y) : y_(y) {}
        void update() override {}
        Real xMin() const override { return -QL_MAX_REAL; }
        Real xMax() const override { return QL_MAX_REAL; }
        std::vector<Real> xValues() const override { return std::vector<Real>(1, 0.0); }
        std::vector<Real> yValues() const override { return std::vector<Real>(1, y_); }
        bool isInRange(Real) const override { return true; }
        Real value(Real) const override { return y_; }
        Real primitive(Real x) const override { return y_ * x; }
        Real derivative(Real) const override { return 0.0; }
        Real secondDerivative(Real) const override { return 0.0; }

        const Real& y_;
    };
};

//! %Constant-interpolation factory and traits
/*! \ingroup interpolations */
class Constant {
public:
    QuantLib::Interpolation interpolate(const Real& y) const { return ConstantInterpolation(y); }
    static const bool global = false;
    static const QuantLib::Size requiredPoints = 1;
};

} // namespace QuantExt
