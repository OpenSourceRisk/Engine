/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

#include <ored/portfolio/additionalfieldgetter.hpp>
#include <ored/portfolio/trade.hpp>

using ore::data::Portfolio;

using std::map;
using std::set;
using std::string;

namespace ore {
namespace data {

PortfolioFieldGetter::PortfolioFieldGetter(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                                           const set<string>& baseFieldNames, bool addExtraFields)
    : portfolio_(portfolio), fieldNames_(baseFieldNames) {

    add(portfolio_, addExtraFields);
}

void PortfolioFieldGetter::add(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, bool addExtraFields) {

    if (!portfolio_)
        portfolio_ = QuantLib::ext::make_shared<Portfolio>();

    // Add to the base field names
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        if (!portfolio_->has(tradeId)) {
            portfolio_->add(trade);
            if (addExtraFields) {
                for (const auto& kv : trade->envelope().additionalFields())
                    fieldNames_.insert(kv.first);
            }
        }
    }
}

set<string> PortfolioFieldGetter::fieldNames() const { return fieldNames_; }

map<string, string> PortfolioFieldGetter::fields(const string& tradeId) const {
    auto trade = portfolio_->get(tradeId);
    QL_REQUIRE(trade, "Could not get trade with trade ID '" << tradeId << "' in the portfolio");

    const map<string, string>& additionalfields = trade->envelope().additionalFields();
    map<string, string> result;
    for (const string& fieldName : fieldNames_) {
        auto f = additionalfields.find(fieldName);
        if (f != additionalfields.end())
            result.insert(*f);
    }

    return result;
}

std::string PortfolioFieldGetter::field(const string& tradeId, const string& fieldName) const {
    auto trade = portfolio_->get(tradeId);
    QL_REQUIRE(trade, "Could not get trade with trade ID '" << tradeId << "' in the portfolio");

    const map<string, string>& additionalFields = trade->envelope().additionalFields();
    string result;
    if (additionalFields.count(fieldName) > 0)
        result = additionalFields.at(fieldName);
    
    return result;
}

std::string PortfolioFieldGetter::npvCurrency(const std::string& tradeId) const {
    auto trade = portfolio_->get(tradeId);
    QL_REQUIRE(trade, "Could no get trade with trade ID '" << tradeId << "' in the portfolio");
    return trade->npvCurrency();
}
} // namespace data
} // namespace ore
