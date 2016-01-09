/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file crdhwparametrization.hpp
    \brief Credit spread Hull White parametrization
*/

#ifndef quantext_crd_hw_parametrization_hpp
#define quantext_crd_hw_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>

namespace QuantExt {

class CreditHwParametrization : public Parametrization {
  public:
    virtual Handle<DefaultProbabilityTermStructure>
    defaultTermStructure() const = 0;
    /*! interface */
    virtual Real sigma(const Time t) const = 0;
    virtual Real a(const Time t) const = 0;
};

} // namespace QuantExt

#endif
