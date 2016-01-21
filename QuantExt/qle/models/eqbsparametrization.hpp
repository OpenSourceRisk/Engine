/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file eqbsparametrization.hpp
    \brief Equity Black Scholes parametrization
*/

#ifndef quantext_eq_bs_parametrization_hpp
#define quantext_eq_bs_parametrization_hpp

#include <qle/models/parametrization.hpp>

namespace QuantExt {

class EqBsParametrization : public Parametrization {
  public:
    EqBsParametrization(
        const Currency &currency, const Handle<Quote> spotToday,
        const Handle<YieldTermStructure> &dividendTermStructure);
    /*! must satisfy variance(0) = 0.0, variance'(t) >= 0 */
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> spotToday() const;
    const Handle<YieldTermStructure> dividendTermStructure() const;

  private:
    const Handle<Quote> spotToday_;
    const Handle<YieldTermStructure> dividendTermStructure_;
};

// inline

inline Real EqBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real EqBsParametrization::stdDeviation(const Time t) const {
    return std::sqrt(variance(t));
}

inline const Handle<Quote> EqBsParametrization::spotToday() const {
    return spotToday_;
}

inline const Handle<YieldTermStructure> EqBsParametrization::dividendTermStructure() const {
    return dividendTermStructure_;
}

} // namespace QuantExt

#endif
