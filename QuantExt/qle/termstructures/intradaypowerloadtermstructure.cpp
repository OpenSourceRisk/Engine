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

/*! \file qle/termstructures/intradayshapetermstructure.cpp
    \brief Term structure of intraday load factors
*/

#include <qle/termstructures/intradaypowerloadtermstructure.hpp>

namespace QuantExt {

IntradayLoadProfile::IntradayLoadProfile(const LoadFactors& load, const LoadFactors& loadDST)
    : loadProfile_(std::move(load)), loadProfileDST_(std::move(loadDST)) {
    totalDeliveryHours_ = 0.0;
    totalMWh_ = 0.0;

    for (const auto& [start, end, load] : loadProfile_) {
        if (load > 0.0) {
            totalMWh_ += load * (end - start) / 3600.0;
            totalDeliveryHours_ += (end - start) / 3600.0;
        }
    }

    for (const auto& [start, end, load] : loadProfileDST_) {
        if (load > 0.0) {
            totalMWh_ += load * (end - start) / 3600.0;
            totalDeliveryHours_ += (end - start) / 3600.0;
        }
    }
}

const LoadFactors& IntradayLoadProfile::loadProfile() const { return loadProfile_; }
const LoadFactors& IntradayLoadProfile::loadProfileDST() const { return loadProfileDST_; }

QuantLib::Real IntradayLoadProfile::totalMWh() const { return totalMWh_; }

QuantLib::Real IntradayLoadProfile::totalDeliveryHours() const { return totalDeliveryHours_; }

IntradayPowerLoadTermStructure::IntradayPowerLoadTermStructure(
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes)
    : loadingShapes_(std::move(loadingShapes)) {}

QuantLib::ext::shared_ptr<IntradayLoadProfile>
IntradayPowerLoadTermStructure::loadProfile(const QuantLib::Date& d) const {
    auto it = loadingShapes_.upper_bound(d);
    if (it == loadingShapes_.begin()) {
        return nullptr;
    }
    --it;
    return it->second;
}
} // namespace QuantExt