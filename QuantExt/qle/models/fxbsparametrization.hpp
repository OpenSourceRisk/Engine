/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file fxbsparametrization.hpp
    \brief FX Black Scholes parametrization
*/

#ifndef quantext_fxbs_parametrization_hpp
#define quantext_fxbs_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

class FxBsParametrization : public Parametrization {
  public:
    FxBsParametrization(const Currency &foreignCurrency,
                        const Handle<Quote> &fxSpotToday);
    /*! must satisfy variance(0) = 0.0, variance'(t) >= 0 */
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> fxSpotToday() const;

  private:
    const Handle<Quote> fxSpotToday_;
};

// inline

inline Real FxBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real FxBsParametrization::stdDeviation(const Time t) const {
    return std::sqrt(variance(t));
}

inline const Handle<Quote> FxBsParametrization::fxSpotToday() const {
    return fxSpotToday_;
}

} // namespace QuantExt

#endif
