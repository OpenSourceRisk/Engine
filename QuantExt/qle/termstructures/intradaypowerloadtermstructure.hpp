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

struct LoadFactor {
    const int startTime;
    const int endTime;
    const QuantLib::Real load;
    const bool isDSTextraHour;
    const QuantLib::Real mwhValue;

    LoadFactor(int start, int end, QuantLib::Real load, bool isDST = false)
        : startTime(start), endTime(end), load(load), isDSTextraHour(isDST), mwhValue(load * (end - start) / 3600.0) {}
};

class IntradayLoadProfile {
public:
    IntradayLoadProfile() {} 
    IntradayLoadProfile(std::vector<LoadFactor> load);


    const std::vector<LoadFactor>& loadProfile() const;

    QuantLib::Real totalMWh() const;

    QuantLib::Real totalDeliveryHours() const;

private:
    std::vector<LoadFactor> loadProfile_;
    QuantLib::Real totalMWh_ = 0.0;
    QuantLib::Real totalDeliveryHours_ = 0.0;
};

class IntradayPowerLoadTermStructure {
public:
    virtual ~IntradayPowerLoadTermStructure() = default;
    virtual QuantLib::ext::shared_ptr<IntradayLoadProfile> loadProfile(const QuantLib::Date& d) const = 0;
    virtual bool empty() const = 0;
};

class IntradayPowerLoadTermStructureExplicit : public IntradayPowerLoadTermStructure {

public:
    IntradayPowerLoadTermStructureExplicit(
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes)
        : loadingShapes_(std::move(loadingShapes)) {}

    QuantLib::ext::shared_ptr<IntradayLoadProfile> loadProfile(const QuantLib::Date& d) const override;
    bool empty() const override { return loadingShapes_.empty(); }

    const std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>>& loadProfiles() const {
        return loadingShapes_;
    }

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes_;
};

class IntradayPowerLoadTermStructureBusinessDayRule : public IntradayPowerLoadTermStructure {
public:
    struct BusinessDayRuleLoadProfile {
        QuantLib::Calendar calendar;
        QuantLib::ext::shared_ptr<IntradayLoadProfile> businessDayProfile;
        QuantLib::ext::shared_ptr<IntradayLoadProfile> nonBusinessDayProfile;
    };

    IntradayPowerLoadTermStructureBusinessDayRule(
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<BusinessDayRuleLoadProfile>> loadingShapes)
        : loadingShapes_(std::move(loadingShapes)) {}

    QuantLib::ext::shared_ptr<IntradayLoadProfile> loadProfile(const QuantLib::Date& d) const override;

    const std::map<QuantLib::Date, QuantLib::ext::shared_ptr<BusinessDayRuleLoadProfile>>& loadProfiles() const {
        return loadingShapes_;
    }

    bool empty() const override { return loadingShapes_.empty(); }

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<BusinessDayRuleLoadProfile>> loadingShapes_;
};

} // namespace QuantExt