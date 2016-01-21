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

class CrdHwParametrization : public Parametrization {
  public:
    CrdHwParametrization(
        const Currency &currency,
        const Handle<DefaultProbabilityTermStructure> &defaultTermStructure);

    virtual Real sigma(const Time t) const = 0;
    virtual Real a(const Time t) const = 0;

    const Handle<DefaultProbabilityTermStructure> defaultTermStructure() const;

  private:
    const Handle<DefaultProbabilityTermStructure> defaultTermStructure_;
};

} // namespace QuantExt

#endif
