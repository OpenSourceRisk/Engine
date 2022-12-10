/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file models/exactbachelierimpliedvolatility.hpp
    \brief implied bachelier volatility based on Jaeckel, Implied Normal Volatility, 2017
    \ingroup models
*/

#pragma once

#include <ql/types.hpp>
#include <ql/option.hpp>

namespace QuantExt {

using namespace QuantLib;

Real exactBachelierImpliedVolatility(Option::Type optionType, Real strike, Real forward, Real tte, Real bachelierPrice,
                                     Real discount = 1.0);

} // namespace QuantExt
