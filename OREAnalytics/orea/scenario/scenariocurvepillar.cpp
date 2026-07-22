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

#include <orea/scenario/scenariocurvepillar.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/marketdata.hpp>

using ore::data::parsePeriod;
using ore::data::to_string;

namespace ore {
namespace analytics {

IrFutureExpiryYearMonth::IrFutureExpiryYearMonth(const std::string& str) : str_(str) {
    auto tmp = data::normaliseDeliveryCode(str_);
    QL_REQUIRE(tmp.size() == 7 && tmp[4] == '-',
               "IrFutureExpiryYearMonth " << str_ << " can not be normalzsed to YYYY-MM.");
    year_ = boost::lexical_cast<int>(tmp.substr(0, 4));
    month_ = QuantLib::Month(boost::lexical_cast<int>(tmp.substr(5, 2)));
}

QuantLib::Month IrFutureExpiryYearMonth::month() const {
    return month_;
}

QuantLib::Year IrFutureExpiryYearMonth::year() const {
    return year_;
}

std::string IrFutureExpiryYearMonth::toString() const { return str_; }

void IrFutureExpiryYearMonth::setConvention(const QuantLib::ext::shared_ptr<ore::data::FutureConvention>& convention) {
    convention_ = convention;
}

QuantLib::Period IrFutureExpiryYearMonth::toPeriod(const QuantLib::Date& referenceDate) const {
    QL_REQUIRE(convention_ != nullptr, "IRFutureExpiryies are only allowed in the context of par scenarios");
    bool isMMFuture = !convention_->isOvernightIndexFuture();
    QL_REQUIRE(isMMFuture || convention_->overnightIndexTenor().has_value(),
               "IRFutureExpiryies are only allowed for overnight index futures if an overnight index tenor is "
               "specified in the convention");
    QuantLib::Date d = isMMFuture ? getMmFutureExpiryDate(month_, year_, convention_->dateGenerationRule())
                                  : getOiFutureStartEndDate(month_, year_, convention_->overnightIndexTenor().value(),
                                                            convention_->dateGenerationRule(), convention_->calendar())
                                        .second;
    return QuantLib::Period((d - referenceDate) * QuantLib::Days);
}

std::ostream& operator<<(std::ostream& os, const IrFutureExpiryYearMonth& v) { return os << v.toString(); }

ScenarioCurvePillar parseScenarioCurvePillar(const std::string& str) {
    QL_REQUIRE(str.size() > 1, "parseScenarioCurvePillar: string must have at least 2 characters");
    QuantLib::Period p;
    if (ore::data::tryParse<QuantLib::Period>(str, p, [](const std::string& s) { return parsePeriod(s); })) {
        return p;
    } else if (str.size() == 7 && str[4] == '-') {
        return IrFutureExpiryYearMonth(str);
    } else {
        QL_FAIL("parseScenarioCurvePillar: string '" << str << "' is neither a valid period nor of the form YYYY-MM");
    }
}

std::ostream& operator<<(std::ostream& os, const ScenarioCurvePillar& v) {
    std::visit([&](const auto& x) { os << x; }, v);
    return os;
}

std::vector<QuantLib::Period> scenarioPillarsToPeriodVector(const QuantLib::Date& asof,
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
