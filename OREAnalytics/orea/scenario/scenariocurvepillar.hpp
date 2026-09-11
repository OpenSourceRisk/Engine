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

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/expiry.hpp>
#include <ored/utilities/marketdata.hpp>

#include <variant>

namespace ore {
namespace analytics {

class IrFutureExpiryYearMonth {
public:
    explicit IrFutureExpiryYearMonth(const std::string& str);
    QuantLib::Month month() const;
    QuantLib::Year year() const;
    std::string toString() const;
    void setConvention(const QuantLib::ext::shared_ptr<ore::data::FutureConvention>& convention);
    QuantLib::Period toPeriod(const QuantLib::Date& referenceDate) const;

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

std::vector<QuantLib::Period> scenarioPillarsToPeriodVector(const QuantLib::Date& asof,
                                                            const std::vector<ScenarioCurvePillar>& pillars,
                                                            bool allowFutureExpiries);

} // namespace analytics
} // namespace ore
