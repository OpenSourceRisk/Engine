/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file scenario/scenariocurvepillar.hpp
    \brief Variant of Periods, explicit dates and future continuations to be used for pillar selection in simmmarket and
   sceneriodata
*/

#pragma once

#include <iostream>
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/expiry.hpp>
#include <ored/utilities/marketdata.hpp>
#include <variant>
namespace ore {
namespace analytics {

class IrFutureExpiryYearMonth {
public:
    explicit IrFutureExpiryYearMonth(const std::string& str) : str_(str) {
        QL_REQUIRE(str_.size() == 7 && str_[4] == '-', "IrFutureExpiryYearMonth should be of the form YYYY-MM");
        year_ = boost::lexical_cast<int>(str_.substr(0, 4));
        month_ = QuantLib::Month(boost::lexical_cast<int>(str_.substr(5, 2)));
    }

    QuantLib::Month month() const { return month_; }
    QuantLib::Year year() const { return year_; }

    std::string toString() const { return str_; }

    void setConvention(const QuantLib::ext::shared_ptr<ore::data::FutureConvention>& convention) {
        convention_ = convention;
    }

    QuantLib::Period toPeriod(const QuantLib::Date& referenceDate) const {
        QL_REQUIRE(convention_ != nullptr, "IRFutureExpiryies are only allowed in the context of par scenarios");
        bool isMMFuture = !convention_->isOvernightIndexFuture();
        QuantLib::Date d = isMMFuture
                               ? getMmFutureExpiryDate(month_, year_, convention_->dateGenerationRule())
                               : getOiFutureStartEndDate(month_, year_, convention_->tenor(),
                                                         convention_->dateGenerationRule(), convention_->calendar())
                                     .second;
        return QuantLib::Period((d - referenceDate) * QuantLib::Days);
    }

private:
    std::string str_;
    QuantLib::Month month_;
    QuantLib::Year year_;
    QuantLib::ext::shared_ptr<ore::data::FutureConvention> convention_;
};

std::ostream& operator<<(std::ostream& os, const IrFutureExpiryYearMonth& v);

using ScenarioCurvePillar = std::variant<QuantLib::Period, IrFutureExpiryYearMonth>;

ScenarioCurvePillar parseScenarioCurvePillar(const std::string& str);

std::ostream& operator<<(std::ostream& os, const ScenarioCurvePillar& v);

inline std::vector<QuantLib::Period> scenarioPillarsToPeriodVector(const QuantLib::Date& asof,
                                                                   const std::vector<ScenarioCurvePillar>& pillars,
                                                                   bool allowFutureExpiries) {
    std::vector<QuantLib::Period> result;
    for (const auto& pillar : pillars) {
        if (auto p = std::get_if<QuantLib::Period>(&pillar)) {
            result.push_back(*p);
        } else if (auto p = std::get_if<IrFutureExpiryYearMonth>(&pillar)) {
            QL_REQUIRE(allowFutureExpiries, "IR Future expiries are not allowed in this context");
            result.push_back(p->toPeriod(asof));
        }
    }
    return result;
}

} // namespace analytics
} // namespace ore