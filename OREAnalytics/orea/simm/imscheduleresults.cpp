/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/simm/imscheduleresults.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/utilities/null.hpp>

using QuantLib::Null;
using QuantLib::Real;
using std::make_pair;
using std::ostream;
using std::string;

namespace ore {
namespace analytics {

void IMScheduleResults::add(const CrifRecord::ProductClass& pc, const string& calculationCcy, const Real grossIM,
                            const Real grossRC, const Real netRC, const Real ngr, const Real scheduleIM) {

    // Add the value as long as the currencies are matching. If the IMScheduleResults container does not yet have
    // a currency, we set it to be that of the incoming value
    if (ccy_.empty())
        ccy_ = calculationCcy;
    else
        QL_REQUIRE(calculationCcy == ccy_, "Cannot add value to IMScheduleResults in a different currency ("
                                               << calculationCcy << "). Expected " << ccy_ << ".");

    if (has(pc)) {
        QL_REQUIRE(grossIM != Null<Real>(), "IMScheduleResults: Gross IM cannot be null.");
        data_[pc].grossIM += grossIM;
    } else {
        IMScheduleResult result = IMScheduleResult(grossIM, grossRC, netRC, ngr, scheduleIM);
        data_[pc] = result;
    }
}

// void IMScheduleResults::convert(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& currency) {
//     // Get corresponding FX spot rate
//     Real fxSpot = market->fxRate(ccy_ + currency)->value();
//
//     convert(fxSpot, currency);
// }
//
// void IMScheduleResults::convert(Real fxSpot, const string& currency) {
//     // Check that target currency is valid
//     QL_REQUIRE(ore::data::checkCurrency(currency), "Cannot convert IMSchedule results. The target currency ("
//                                                        << currency << ") must be a valid ISO currency code");
//
//     // Skip if already in target currency
//     if (ccy_ == currency)
//         return;
//
//     // Convert Schedule IM results to target currency
//     for (auto& sr : data_)
//         sr.second *= fxSpot;
//
//     // Update currency
//     ccy_ = currency;
// }

IMScheduleResult IMScheduleResults::get(const CrifRecord::ProductClass& pc) const {
    if (has(pc))
        return data_.at(pc);
    else
        return IMScheduleResult(QuantLib::Null<Real>(), QuantLib::Null<Real>(), QuantLib::Null<Real>(),
                                QuantLib::Null<Real>(), QuantLib::Null<Real>());
}

bool IMScheduleResults::has(const CrifRecord::ProductClass& pc) const { return data_.count(pc) > 0; }

bool IMScheduleResults::empty() const { return data_.empty(); }

void IMScheduleResults::clear() { data_.clear(); }

} // namespace analytics
} // namespace ore
