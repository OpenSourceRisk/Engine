/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/curvepillar.hpp
    \brief Variant of Periods, explicit dates and future continuations to be used for pillar selection in simmmarket and
   sceneriodata
*/

#pragma once

#include <iostream>
#include <ored/marketdata/expiry.hpp>
#include <variant>
namespace ore {
namespace data {

class ExpiryMonthYear {
public:
    explicit ExpiryMonthYear(const std::string& str) : str_(str) {
        QL_REQUIRE(str_.size() == 7 && str_[4] == '-', "ExpiryMonthYear should be of the form YYYY-MM");
        year_ = boost::lexical_cast<int>(str_.substr(0, 4));
        month_ = QuantLib::Month(boost::lexical_cast<int>(str_.substr(5, 2)));
    }

    QuantLib::Month month() const { return month_; }
    QuantLib::Year year() const { return year_; }

    std::string toString() const { return str_; }

private:
    std::string str_;
    QuantLib::Month month_;
    QuantLib::Year year_;
};

std::ostream& operator<<(std::ostream& os, const ExpiryMonthYear& v);

using CurvePillar = std::variant<QuantLib::Period, ExpiryMonthYear>;

CurvePillar parseCurvePillar(const std::string& str);

std::ostream& operator<<(std::ostream& os, const CurvePillar& v);
} // namespace data
} // namespace ore