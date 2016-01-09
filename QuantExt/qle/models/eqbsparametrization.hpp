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

class EquityBsParametrization : public Parametrization {
  public:
    virtual Handle<YieldTermStructure> dividendTermStructure() const = 0;
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
};

// inline

inline Real EquityBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real EquityBsParametrization::stdDeviation(const Time t) const {
    return std::sqrt(variance(t));
}

} // namespace QuantExt

#endif
