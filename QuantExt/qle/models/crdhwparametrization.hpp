/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file crdhwparametrization.hpp
    \brief Credit spread Hull White parametrization
*/

#ifndef quantext_crd_hw_parametrization_hpp
#define quantext_crd_hw_parametrization_hpp

#include <qle/models/parametrization.hpp>

namespace QuantExt {

class CreditHwParametrization : Parametrization {
  public:
    virtual Handle<DefaultProbabilityTermStructure>
    defaultTermStructure() const = 0;
    /*! interface */
    virtual Real sigma(const Time t) const = 0;
    virtual Real a(const Time t) const = 0;
};

} // namespace QuantExt

#endif
