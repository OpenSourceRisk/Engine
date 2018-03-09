/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#ifndef quantext_flat_extrapolation_hpp
#define quantext_flat_extrapolation_hpp

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Flat extrapolation given a base interpolation
/*! \ingroup math
 */
class FlatExtrapolation : public Interpolation {
private:
    class FlatExtrapolationImpl : public Interpolation::Impl {

    public:
        FlatExtrapolationImpl(const boost::shared_ptr<Interpolation>& i) : i_(i) {}
        void update() { i_->update(); }
        Real xMin() const { return i_->xMin(); }
        Real xMax() const { return i_->xMax(); }
        std::vector<Real> xValues() const { QL_FAIL("not implemented"); }
        std::vector<Real> yValues() const { QL_FAIL("not implemented"); }
        bool isInRange(Real x) const { return i_->isInRange(x); }
        Real value(Real x) const {
            Real tmp = std::max(std::min(x, i_->xMax()), i_->xMin());
            return i_->operator()(tmp);
        }
        Real primitive(Real x) const {
            if (x >= i_->xMin() && x <= i_->xMax()) {
                return i_->primitive(x);
            }
            if (x < i_->xMin()) {
                return i_->primitive(i_->xMin()) - (i_->xMin() - x);
            } else {
                return i_->primitive(i_->xMax()) + (x - i_->xMax());
            }
        }
        Real derivative(Real x) const {
            if (x > i_->xMin() && x < i_->xMax()) {
                return i_->derivative(x);
            } else {
                // that is the left derivative for xmin and
                // the right derivative for xmax
                return 0.0;
            }
        }
        Real secondDerivative(Real x) const {
            if (x > i_->xMin() && x < i_->xMax()) {
                return i_->secondDerivative(x);
            } else {
                // that is the left derivative for xmin and
                // the right derivative for xmax
                return 0.0;
            }
        }

    private:
        const boost::shared_ptr<Interpolation> i_;
    };

public:
    FlatExtrapolation(const boost::shared_ptr<Interpolation>& i) {
        impl_ = boost::make_shared<FlatExtrapolationImpl>(i);
        impl_->update();
    }
};

//! %Linear-interpolation and flat extrapolation factory and traits
class LinearFlat {
public:
    template <class I1, class I2> Interpolation interpolate(const I1& xBegin, const I1& xEnd, const I2& yBegin) const {
        return FlatExtrapolation(boost::make_shared<LinearInterpolation>(xBegin, xEnd, yBegin));
    }
    static const bool global = false;
    static const Size requiredPoints = 2;
};

} // namespace QuantExt

#endif
