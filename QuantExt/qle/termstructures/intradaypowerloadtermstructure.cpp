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

namespace {

template <typename T> T find(const std::map<QuantLib::Date, T>& m, const QuantLib::Date& d) {
    auto it = m.upper_bound(d);
    if (it == m.begin())
        return nullptr;
    --it;
    return it->second;
}
} // namespace

IntradayLoadProfile::IntradayLoadProfile(std::vector<LoadFactor> load)
    : loadProfile_(std::move(load)) {
    totalDeliveryHours_ = 0.0;
    totalMWh_ = 0.0;

    for (const auto& [start, end, load, isDST, mwh] : loadProfile_) {
        if (load > 0.0) {
            totalMWh_ += mwh;
            totalDeliveryHours_ += (end - start) / 3600.0;
        }
    }
}

const std::vector<LoadFactor>& IntradayLoadProfile::loadProfile() const { return loadProfile_; }

QuantLib::Real IntradayLoadProfile::totalMWh() const { return totalMWh_; }

QuantLib::Real IntradayLoadProfile::totalDeliveryHours() const { return totalDeliveryHours_; }

QuantLib::ext::shared_ptr<IntradayLoadProfile>
IntradayPowerLoadTermStructureExplicit::loadProfile(const QuantLib::Date& d) const {
    auto profile = find(loadingShapes_, d);
    if (profile == nullptr)
        return QuantLib::ext::shared_ptr<IntradayLoadProfile>();
    return profile;
}

QuantLib::ext::shared_ptr<IntradayLoadProfile>
IntradayPowerLoadTermStructureBusinessDayRule::loadProfile(const QuantLib::Date& d) const {
    auto loadShape = find(loadingShapes_, d);
    if (loadShape == nullptr){
        return QuantLib::ext::shared_ptr<IntradayLoadProfile>();
    }
    if (loadShape->calendar.isBusinessDay(d)){
        return loadShape->businessDayProfile;
    }
    return loadShape->nonBusinessDayProfile;
}

} // namespace QuantExt