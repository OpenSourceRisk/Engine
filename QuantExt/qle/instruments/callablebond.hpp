/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file callablebond.hpp */

#pragma once

#include <ql/instruments/bond.hpp>
#include <ql/pricingengine.hpp>
#include <ql/time/daycounter.hpp>

namespace QuantExt {

using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

class CallableBond : public QuantLib::Bond {
public:
    class arguments;
    class results;
    class engine;

    struct CallabilityData {
        enum class ExerciseType { OnThisDate, FromThisDateOn };
        enum class PriceType { Clean, Dirty };
        Date exerciseDate;
        ExerciseType exerciseType;
        Real price;
        PriceType priceType;
        bool includeAccrual;
    };

    /* callData, putData must be sorted w.r.t. their event dates */
    CallableBond(Size settlementDays, const Calendar& calendar, const Date& issueDate, const QuantLib::Leg& coupons,
                 const std::vector<CallabilityData>& callData = {}, const std::vector<CallabilityData>& putData = {},
                 const bool perpetual = false);

    const std::vector<CallabilityData>& callData() const { return callData_; }
    const std::vector<CallabilityData>& putData() const { return putData_; }
    bool isPerpetual() const { return perpetual_; }

private:
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;

    std::vector<CallabilityData> callData_;
    std::vector<CallabilityData> putData_;
    bool perpetual_;
};

class CallableBond::arguments : public QuantLib::Bond::arguments {
public:
    void validate() const override;
    Date startDate;
    std::vector<Real> notionals;
    std::vector<CallabilityData> callData;
    std::vector<CallabilityData> putData;
    bool perpetual;
};

class CallableBond::results : public QuantLib::Bond::results {
public:
    void reset() override;
};

class CallableBond::engine : public QuantLib::GenericEngine<CallableBond::arguments, CallableBond::results> {};

} // namespace QuantExt
