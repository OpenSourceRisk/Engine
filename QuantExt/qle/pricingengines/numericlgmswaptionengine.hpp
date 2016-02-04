/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
*/

/*! \file nnumericlgmswaptionengine.hpp
    \brief numeric engine for bermudan swaptions in the LGM model
*/

#ifndef quantext_numeric_lgm_swaption_engine_hpp
#define quantext_numeric_lgm_swaption_engine_hpp

#include <qle/math/cumulativenormaldistribution.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

//! Numerical engine for bermudan swaptions in the LGM model
/*! \ingroup swaptionengines

    All fixed coupons with start date greater or equal to the respective
    option expiry are considered to be
    part of the exercise into right.

    \warning Cash settled swaptions are not supported

    Reference: Hagan, Methodology for callable swaps and Bermudan
               “exercise into” swaptions
*/

class NumericLgmSwaptionEngine
    : public GenericEngine<Swaption::arguments, Swaption::results> {
  public:
    NumericLgmSwaptionEngine(
        const boost::shared_ptr<LinearGaussMarkovModel> &model, const Real sy,
        const Size ny, const Real sx, const Size nx,
        const Handle<YieldTermStructure> &discountCurve =
            Handle<YieldTermStructure>())
        : GenericEngine<Swaption::arguments, Swaption::results>(),
          model_(model), nx_(nx), discountCurve_(discountCurve) {

        if (!discountCurve_.empty())
            registerWith(discountCurve_);

        // precompute weights

        // number of x, y grid points > 0
        mx_ = static_cast<Size>(floor(sx * static_cast<Real>(nx)) + 0.5);
        my_ = static_cast<Size>(floor(sy * static_cast<Real>(ny)) + 0.5);

        // y-grid spacing
        h_ = 1.0 / ny;

        // weights for convolution in the rollback step
        QuantExt::CumulativeNormalDistribution N;
        NormalDistribution G;

        y_.resize(2 * my_ + 1); // x-coordinate / standard deviation of x
        w_.resize(2 * my_ + 1); // probability weight around y-grid point i
        wsum_.resize(2 * my_ + 1, 0.0); // partial sum of the weights
        Real M0 = 0.0, M2 = 0.0, M4 = 0.0;
        for (int i = 0; i <= 2 * my_; i++) {
            y_[i] = h_ * (i - my_);
            if (i == 0 || i == 2 * my_)
                w_[i] = (1. + y_[0] / h_) * N(y_[0] + h_) -
                        y_[0] / h_ * N(y_[0]) + (G(y_[0] + h_) - G(y_[0])) / h_;
            else
                w_[i] = (1. + y_[i] / h_) * N(y_[i] + h_) -
                        2. * y_[i] / h_ * N(y_[i]) -
                        (1. - y_[i] / h_) *
                            N(y_[i] - h_) // opposite sign in the paper
                        + (G(y_[i] + h_) - 2. * G(y_[i]) + G(y_[i] - h_)) / h_;

            M0 += w_[i];
            Real y2 = y_[i] * y_[i];
            M2 += w_[i] * y2;
            M4 += w_[i] * y2 * y2;
        }
    }

    void calculate() const;

  private:
    Real conditionalSwapValue(Real x, Real t, const Date expiry0) const;
    boost::shared_ptr<LinearGaussMarkovModel> model_;
    mutable boost::shared_ptr<IborIndex> iborIndex_;
    mutable boost::shared_ptr<LgmImpliedYieldTermStructure> iborModelCurve_;
    int mx_, my_, nx_;
    const Handle<YieldTermStructure> discountCurve_;
    Real h_;
    std::vector<Real> y_, w_, wsum_;
};

} // namespace QuantExt

#endif
