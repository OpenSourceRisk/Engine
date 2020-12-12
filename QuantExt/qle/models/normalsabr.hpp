/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file normalsabr.hpp
    \brief normal SABR model implied volatility approximation

    \ingroup termstructures
*/

#pragma once

#include <ql/types.hpp>

using namespace QuantLib;

namespace QuantExt {

Real normalSabrVolatility(Rate strike, Rate forward, Time expiryTime, Real alpha, Real nu, Real rho);
Real normalSabrAlphaFromAtmVol(Rate forward, Time expiryTime, Real atmVol, Real nu, Real rho);

} // namespace QuantExt
