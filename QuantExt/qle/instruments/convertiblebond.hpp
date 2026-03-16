/*
 Copyright (C) 2005, 2006 Theo Boafo
 Copyright (C) 2006, 2007 StatPro Italia srl

 Copyright (C) 2020 Quaternion Risk Managment Ltd

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

/*! \file qle/instruments/convertiblebond.hpp
    \brief convertible bond class
*/

#pragma once

#include <ql/instruments/bond.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/instruments/dividendschedule.hpp>
#include <ql/instruments/oneassetoption.hpp>
#include <ql/quote.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {
class IborIndex;
class PricingEngine;
} // namespace QuantLib

namespace QuantExt {

using namespace QuantLib;

//! %callability leaving to the holder the possibility to convert
class SoftCallability : public Callability {
public:
    SoftCallability(const Bond::Price& price, const Date& date, Real trigger)
        : Callability(price, Callability::Call, date), trigger_(trigger) {}
    Real trigger() const { return trigger_; }

private:
    Real trigger_;
};

//! convertible bond
class ConvertibleBond : public Bond {
public:
    class option;
    //! similar to bond ctor, coupons should not contain redemption flows
    ConvertibleBond(Natural settlementDays, const Calendar& calendar, const Date& issueDate, const Leg& coupons,
                    const QuantLib::ext::shared_ptr<Exercise>& exercise, const Real conversionRatio,
                    const DividendSchedule& dividends, const CallabilitySchedule& callability);
    QuantLib::ext::shared_ptr<Exercise> exercise() const { return exercise_; }
    Real conversionRatio() const { return conversionRatio_; }
    const DividendSchedule& dividends() const { return dividends_; }
    const CallabilitySchedule& callability() const { return callability_; }

protected:
    void performCalculations() const override;

    QuantLib::ext::shared_ptr<Exercise> exercise_;
    Real conversionRatio_;
    DividendSchedule dividends_;
    CallabilitySchedule callability_;

    QuantLib::ext::shared_ptr<option> option_;
};

class ConvertibleBond::option : public OneAssetOption {
public:
    class arguments;
    class engine;
    option(const ConvertibleBond* bond);

    void setupArguments(PricingEngine::arguments*) const override;

    bool isExpired() const override;

private:
    const ConvertibleBond* bond_;
};

class ConvertibleBond::option::arguments : public OneAssetOption::arguments {
public:
    arguments() : conversionRatio(Null<Real>()), settlementDays(Null<Natural>()) {}

    Real conversionRatio, conversionValue;
    DividendSchedule dividends;
    std::vector<Date> dividendDates;
    std::vector<Date> callabilityDates;
    std::vector<Callability::Type> callabilityTypes;
    std::vector<Real> callabilityPrices;
    std::vector<Real> callabilityTriggers;
    std::vector<Date> cashflowDates;
    std::vector<Real> cashflowAmounts;
    std::vector<Date> notionalDates;
    std::vector<Real> notionals;
    Date issueDate;
    Date settlementDate;
    Date maturityDate;
    Natural settlementDays;

    void validate() const override;
};

class ConvertibleBond::option::engine
    : public GenericEngine<ConvertibleBond::option::arguments, ConvertibleBond::option::results> {};

} // namespace QuantExt
