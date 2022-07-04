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

#include <qle/models/normalsabr.hpp>
#include <qle/models/normalsabrsmilesection.hpp>

#include <ql/termstructures/volatility/sabr.hpp>
#include <ql/utilities/dataformatters.hpp>

namespace QuantExt {

NormalSabrSmileSection::NormalSabrSmileSection(Time timeToExpiry, Rate forward, const std::vector<Real>& sabrParams)
    : SmileSection(timeToExpiry, DayCounter(), Normal), forward_(forward) {
    alpha_ = sabrParams[0];
    nu_ = sabrParams[1];
    rho_ = sabrParams[2];
    // validateSabrParameters(alpha_, 0.0, nu_, rho_);
}

NormalSabrSmileSection::NormalSabrSmileSection(const Date& d, Rate forward, const std::vector<Real>& sabrParams,
                                               const DayCounter& dc)
    : SmileSection(d, dc, Date(), Normal), forward_(forward) {
    alpha_ = sabrParams[0];
    nu_ = sabrParams[1];
    rho_ = sabrParams[2];
    // validateSabrParameters(alpha_, 0.0, nu_, rho_);
}

Real NormalSabrSmileSection::varianceImpl(Rate strike) const {
    Volatility vol = normalSabrVolatility(strike, forward_, exerciseTime(), alpha_, nu_, rho_);
    return vol * vol * exerciseTime();
}

Real NormalSabrSmileSection::volatilityImpl(Rate strike) const {
    Real vol = normalSabrVolatility(strike, forward_, exerciseTime(), alpha_, nu_, rho_);
    return vol;
}
} // namespace QuantExt
