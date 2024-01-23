/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/scripting/paylog.hpp>
#include <set>

namespace ore {
namespace data {

void PayLog::write(RandomVariable value, const Filter& filter, const Date& obs, const Date& pay, const std::string& ccy,
                   const Size legNo, const std::string& cashflowType, const Size slot) {

    // if a slot is given, we erase the results that we already have for this slot

    if (slot != 0) {
        for (Size i = 0; i < slots_.size(); ++i) {
            if (slot == slots_[i]) {
                amounts_[i] = applyInverseFilter(amounts_[i], filter);
            }
        }
    }

    // determine the index where the result belongs, i.e. look for an existing entry for the given
    // pay date, ccy, legNo, cashflowType and slot

    Size idx = Null<Size>();
    for (Size i = 0; i < slots_.size(); ++i) {
        if (dates_[i] == pay && currencies_[i] == ccy && legNos_[i] == legNo && cashflowTypes_[i] == cashflowType &&
            (slot == slots_[i]))
            idx = i;
    }

    // if we did not find a slot, we create one

    if (idx == Null<Size>()) {
        slots_.push_back(slot);
        amounts_.push_back(RandomVariable(value.size(), 0.0));
        dates_.push_back(pay);
        currencies_.push_back(ccy);
        legNos_.push_back(legNo);
        cashflowTypes_.push_back(cashflowType);
        idx = slots_.size() - 1;
    }

    // add the value

    amounts_[idx] += applyFilter(value, filter);
}

void PayLog::consolidateAndSort() {

    // Create set of (legNo, payDate, payCcy, cfType),  index in amounts / dates / ... vectors.
    // This will also create the sorting we want. Ignore slots from here on.

    std::set<std::tuple<Size, Date, std::string, std::string, Size>> dates;
    for (Size i = 0; i < slots_.size(); ++i) {
        dates.insert(std::make_tuple(legNos_[i], dates_[i], currencies_[i], cashflowTypes_[i], i));
    }

    // build consolidated and sorted vector

    std::vector<Date> resultDates;
    std::vector<std::string> resultCurrencies;
    std::vector<Size> resultLegNos;
    std::vector<std::string> resultCashflowTypes;
    std::vector<RandomVariable> resultAmounts;
    Date lastDate = Null<Date>();
    std::string lastCurrency;
    Size lastLegNo = Null<Size>();
    std::string lastCashflowType;
    for (auto const& d : dates) {
        if (std::get<1>(d) == lastDate && std::get<2>(d) == lastCurrency && std::get<0>(d) == lastLegNo &&
            std::get<3>(d) == lastCashflowType)
            resultAmounts.back() += amounts_[std::get<4>(d)];
        else {
            resultAmounts.push_back(amounts_[std::get<4>(d)]);
            resultDates.push_back(std::get<1>(d));
            resultCurrencies.push_back(std::get<2>(d));
            resultLegNos.push_back(std::get<0>(d));
            resultCashflowTypes.push_back(std::get<3>(d));
        }
        lastDate = std::get<1>(d);
        lastCurrency = std::get<2>(d);
        lastLegNo = std::get<0>(d);
        lastCashflowType = std::get<3>(d);
    }

    // overwrite the existing members

    amounts_ = std::move(resultAmounts);
    dates_ = std::move(resultDates);
    currencies_ = std::move(resultCurrencies);
    legNos_ = std::move(resultLegNos);
    cashflowTypes_ = std::move(resultCashflowTypes);

    // reset slots

    slots_ = std::vector<Size>(amounts_.size(), 0);
}

} // namespace data
} // namespace ore
