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
#include <qle/utilities/intradaypower.hpp>

namespace QuantExt {

struct LoadFactor {
    const int startTime;
    const int endTime;
    const QuantLib::Real load;
    const bool isDSTextraHour;

    LoadFactor(int startTime, int endTime, QuantLib::Real load, bool isDSTextraHour)
        : startTime(startTime), endTime(endTime), load(load), isDSTextraHour(isDSTextraHour) {
        QL_REQUIRE(startTime >= 0 && startTime < QuantExt::SECONDS_PER_DAY, "startTime must be in [0, 86400)");
        QL_REQUIRE(endTime > 0 && endTime <= QuantExt::SECONDS_PER_DAY, "endTime must be in (0, 86400]");
        QL_REQUIRE(endTime > startTime, "endTime must be greater than startTime");
        QL_REQUIRE(load >= 0, "load must be non-negative");
    }
};

struct TotalLoadFactor {
    const int startTime;
    const int endTime;
    const QuantLib::Real load;
    const QuantLib::Real totalMWh;
    const bool isDSTextraHour;

    TotalLoadFactor(int startTime, int endTime, QuantLib::Real load, QuantLib::Real totalMWh, bool isDSTextraHour)
        : startTime(startTime), endTime(endTime), load(load), totalMWh(totalMWh), isDSTextraHour(isDSTextraHour) {
        QL_REQUIRE(startTime >= 0 && startTime < QuantExt::SECONDS_PER_DAY, "startTime must be in [0, 86400)");
        QL_REQUIRE(endTime > 0 && endTime <= QuantExt::SECONDS_PER_DAY, "endTime must be in (0, 86400]");
        QL_REQUIRE(endTime > startTime, "endTime must be greater than startTime");
        QL_REQUIRE(load >= 0, "load must be non-negative");
        QL_REQUIRE(totalMWh >= 0, "totalMWh must be non-negative");
    }
};

using IntradayPowerLoadProfile = std::vector<LoadFactor>;
using IntradayPowerLoadProfileWithMWh = std::vector<TotalLoadFactor>;

TotalLoadFactor dstAdjustedTotalLoad(const LoadFactor& loadFactor, QuantExt::IntradayPowerDSTAdjustment dayTimeSavingsAdj);
class IntradayPowerLoadTermStructure {
public:
    virtual ~IntradayPowerLoadTermStructure() = default;
    virtual QuantLib::ext::shared_ptr<IntradayPowerLoadProfile> loadProfile(const QuantLib::Date& d) const = 0;
    virtual bool empty() const = 0;
};

class IntradayPowerLoadTermStructureExplicit : public IntradayPowerLoadTermStructure {

public:
    IntradayPowerLoadTermStructureExplicit(
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>> loadingShapes)
        : loadingShapes_(std::move(loadingShapes)) {}

    QuantLib::ext::shared_ptr<IntradayPowerLoadProfile> loadProfile(const QuantLib::Date& d) const override;
    bool empty() const override { return loadingShapes_.empty(); }

    const std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>>& loadProfiles() const {
        return loadingShapes_;
    }

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayPowerLoadProfile>> loadingShapes_;
};

class IntradayPowerLoadTermStructureBusinessDayRule : public IntradayPowerLoadTermStructure {
public:
    struct BusinessDayRuleLoadProfile {
        QuantLib::Calendar calendar;
        QuantLib::ext::shared_ptr<IntradayPowerLoadProfile> businessDayProfile;
        QuantLib::ext::shared_ptr<IntradayPowerLoadProfile> nonBusinessDayProfile;
    };

    IntradayPowerLoadTermStructureBusinessDayRule(
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<BusinessDayRuleLoadProfile>> loadingShapes)
        : loadingShapes_(std::move(loadingShapes)) {}

    QuantLib::ext::shared_ptr<IntradayPowerLoadProfile> loadProfile(const QuantLib::Date& d) const override;

    const std::map<QuantLib::Date, QuantLib::ext::shared_ptr<BusinessDayRuleLoadProfile>>& loadProfiles() const {
        return loadingShapes_;
    }

    bool empty() const override { return loadingShapes_.empty(); }

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<BusinessDayRuleLoadProfile>> loadingShapes_;
};

} // namespace QuantExt