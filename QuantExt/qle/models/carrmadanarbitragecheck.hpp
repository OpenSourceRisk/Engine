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

/*! \file models/carrmadanarbitragecheck.hpp
    \brief arbitrage checks based on Carr, Madan, A note on sufficient conditions for no arbitrage (2005)
    \ingroup models
*/

#pragma once

#include <ql/types.hpp>

#include <vector>

namespace QuantExt {

using namespace QuantLib;

class CarrMadanMarginalProbability {
public:
    CarrMadanMarginalProbability(const std::vector<Real>& strikes, const std::vector<Real>& callPrices);
    const std::vector<Real>& strikes() const;
    const std::vector<Real>& callPrices() const;
    bool arbitrageFree() const;
    const std::vector<bool> violationsType1() const;
    const std::vector<bool> violationsType2() const;
    const std::vector<Real> density() const;

private:
    std::vector<Real> strikes_, callPrices_;
    std::vector<bool> violationsType1_, violationsType2_;
    std::vector<Real> q_;
};

std::string arbitrageViolationsAsString(const CarrMadanMarginalProbability& cm);

} // namespace QuantExt
