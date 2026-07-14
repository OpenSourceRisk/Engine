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

int overlapWithMissingDSTHour(int start, int end) {
    return std::max(0, std::min(end, QuantExt::THREE_AM_IN_SECONDS) - std::max(start, QuantExt::TWO_AM_IN_SECONDS));
}

QuantLib::Real daylightSavingAdjustedLoadMWh(const LoadFactor& loadFactor,
                                     QuantExt::IntradayPowerDSTAdjustment dayTimeSavingsAdj) {
    const auto duration = loadFactor.endTime - loadFactor.startTime;

    auto excludedSeconds = dayTimeSavingsAdj == QuantExt::IntradayPowerDSTAdjustment::Forward
                               ? overlapWithMissingDSTHour(loadFactor.startTime, loadFactor.endTime)
                               : 0.0;

    const auto adjustedDuration = duration - excludedSeconds;

    const auto mwh = (loadFactor.isDSTextraHour && dayTimeSavingsAdj != QuantExt::IntradayPowerDSTAdjustment::Backward)
                         ? 0.0
                         : loadFactor.load * adjustedDuration / static_cast<QuantLib::Real>(QuantExt::SECONDS_PER_HOUR);

    return mwh;
}

QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>
IntradayPowerLoadTermStructureExplicit::loadProfile(const QuantLib::Date& d) const {
    auto profile = find(loadingShapes_, d);
    if (profile == nullptr)
        return QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>();
    return profile;
}

QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>
IntradayPowerLoadTermStructureBusinessDayRule::loadProfile(const QuantLib::Date& d) const {
    auto loadShape = find(loadingShapes_, d);
    if (loadShape == nullptr){
        return QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>();
    }
    if (loadShape->calendar.isBusinessDay(d)){
        return loadShape->businessDayProfile;
    }
    return loadShape->nonBusinessDayProfile;
}

} // namespace QuantExt