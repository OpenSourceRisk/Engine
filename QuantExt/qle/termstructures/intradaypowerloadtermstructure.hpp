/*
Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file qle/termstructures/intradayshapetermstructure.hpp
    \brief Term structure of intraday shape factors
*/

#pragma once

#include <ql/currency.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quote.hpp>
#include <ql/termstructure.hpp>

namespace QuantExt {

using LoadFactors = std::vector<std::tuple<int, int, double>>;
class IntradayLoadProfile {
public:
    IntradayLoadProfile(const LoadFactors& load, const LoadFactors& loadDST);

    const LoadFactors& loadProfile() const;
    const LoadFactors& loadProfileDST() const;

    QuantLib::Real totalMWh() const;

    QuantLib::Real totalDeliveryHours() const;

private:
    LoadFactors loadProfile_;
    LoadFactors loadProfileDST_;
    QuantLib::Real totalMWh_ = 0.0;
    QuantLib::Real totalDeliveryHours_ = 0.0;
};

class IntradayPowerLoadTermStructure {

public:
    IntradayPowerLoadTermStructure(
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes);

    QuantLib::ext::shared_ptr<IntradayLoadProfile> loadProfile(const QuantLib::Date& d) const;

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes_;
};

} // namespace QuantExt