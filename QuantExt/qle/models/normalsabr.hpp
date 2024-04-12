/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file normalsabr.hpp
    \brief normal SABR model implied volatility approximation

    \ingroup termstructures
*/

#pragma once

#include <ql/types.hpp>

namespace QuantExt {
using namespace QuantLib;

// Hagan 2002
Real normalSabrVolatility(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho);
Real normalSabrAlphaFromAtmVol(Rate forward, Time expiryTime, Real atmVol, Real nu, Real rho);

// Antonov 2015, Mixing SABR models for Negative Rates and 2013, SABR spreads its wings
Real normalFreeBoundarySabrPrice(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho);
Real normalFreeBoundarySabrVolatility(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho);

} // namespace QuantExt
