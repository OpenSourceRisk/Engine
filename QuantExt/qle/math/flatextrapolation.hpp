/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file flatextrapolation.hpp
    \brief flat interpolation decorator
*/

#ifndef quantext_flat_extrapolation_hpp
#define quantext_flat_extrapolation_hpp

#include <ql/math/interpolation.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Flat extrapolation given a base interpolation
/*! \ingroup interpolations */
class FlatExtrapolation : public Interpolation {
  private:
    class FlatExtrapolationImpl : public Interpolation::Impl {

      public:
        FlatExtrapolationImpl(const boost::shared_ptr<Interpolation> &i)
            : i_(i) {}
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
    FlatExtrapolation(const boost::shared_ptr<Interpolation> &i) {
        impl_ = boost::make_shared<FlatExtrapolationImpl>(i);
        impl_->update();
    }
};

} // namespace QuantExt

#endif
