/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file eqbsparametrization.hpp
    \brief Equity Black Scholes parametrization
*/

#ifndef quantext_com_gab_parametrization_hpp
#define quantext_com_gab_parametrization_hpp

#include <qle/models/parametrization.hpp>

namespace QuantExt {

class ComGabParametrization : public Parametrization {
  public:
    ComGabParametrization(const Currency &currency);
    /* ... */
};

// inline

/* ... */

} // namespace QuantExt

#endif
