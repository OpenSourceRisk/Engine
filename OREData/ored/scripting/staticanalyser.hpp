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

/*! \file ored/scripting/staticanalyser.hpp
    \brief static script analyser
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>

#include <ored/configuration/conventions.hpp>

namespace ore {
namespace data {

/*! assumption: the context passed to a script engine is const and no declarations of type
  event or index are allowed in a script. */
class StaticAnalyser {
public:
    StaticAnalyser(const ASTNodePtr root, const QuantLib::ext::shared_ptr<Context> context)
        : root_(root), context_(context) {}
    void run(const std::string& script = "");

    // maps an index (EQ-IDX, EUR-CMS-10Y, ...) to the set of observation dates on which it is is evaluated
    // via ()(obsDate) or ()(obsdate,fwdDate) or [ABOVE|BELOW]PROB(d1, d2)
    const std::map<std::string, std::set<QuantLib::Date>>& indexEvalDates() const { return indexEvalDates_; }

    // maps an index (EQ-IDX, EUR-CMS-10Y, ...) to the set of forward dates which are requested via ()(obsDate, fwdDate)
    const std::map<std::string, std::set<QuantLib::Date>>& indexFwdDates() const { return indexFwdDates_; }

    // maps a pay currency to obsdate required from PAY()
    const std::map<std::string, std::set<QuantLib::Date>>& payObsDates() const { return payObsDates_; }

    // maps a pay currency to paydate required from PAY()
    const std::map<std::string, std::set<QuantLib::Date>>& payPayDates() const { return payPayDates_; }

    // maps a pay currency to obsdate required from DISCOUNT()
    const std::map<std::string, std::set<QuantLib::Date>>& discountObsDates() const { return discountObsDates_; }

    // maps a pay currency to paydate required from DISCOUNT()
    const std::map<std::string, std::set<QuantLib::Date>>& discountPayDates() const { return discountPayDates_; }

    // set of obs dates where a conditional expectation from NPV() is required
    const std::set<QuantLib::Date>& regressionDates() const { return regressionDates_; }

    // maps an index (EUR-EONIA) to the set of fixing dates from FWD[COMP|AVG](index, obs, start, end, ...)
    const std::map<std::string, std::set<QuantLib::Date>>& fwdCompAvgFixingDates() const { return fwdCompAvgFixingDates_; }

    // maps an index (EUR-EONIA) to the set of obs dates from FWD[COMP|AVG](index ,obs, start, end, ...)
    const std::map<std::string, std::set<QuantLib::Date>>& fwdCompAvgEvalDates() const { return fwdCompAvgEvalDates_; }

    // maps an index (EUR-EONIA) to the set of start or end (value) dates from FWD[COMP|AVG](index, obs, start, end, ...)
    const std::map<std::string, std::set<QuantLib::Date>>& fwdCompAvgStartEndDates() const {
        return fwdCompAvgStartEndDates_;
    }

    // maps an index (EQ-IDX, EUR-CMS-10Y, ...) to the set of fixing dates from [ABOVE|BELOW]PROB(d1, d2)
    const std::map<std::string, std::set<QuantLib::Date>>& probFixingDates() const { return probFixingDates_; }

private:
    const ASTNodePtr root_;
    const QuantLib::ext::shared_ptr<Context> context_;
    //
    std::map<std::string, std::set<QuantLib::Date>> indexEvalDates_, indexFwdDates_, payObsDates_, payPayDates_,
        discountObsDates_, discountPayDates_, fwdCompAvgFixingDates_, fwdCompAvgEvalDates_, fwdCompAvgStartEndDates_,
        probFixingDates_;
    std::set<QuantLib::Date> regressionDates_;
};

} // namespace data
} // namespace ore
