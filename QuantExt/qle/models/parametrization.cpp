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

#include <qle/models/parametrization.hpp>

namespace QuantExt {

Parametrization::Parametrization(const Currency &currency)
    : h_(1.0E-6), h2_(1.0E-4), currency_(currency) {}

const Currency Parametrization::currency() const { return currency_; }

Size Parametrization::parameterSize(const Size) const { return 0; }

const Array &Parametrization::parameterTimes(const Size) const {
    return emptyTimes_;
}

const Array &Parametrization::parameterValues(const Size) const {
    return emptyValues_;
}

} // namespace QuantExt
