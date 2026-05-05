/*
 Copyright (C) 2026 Quaternion Risk Management Ltd.
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

#include <map>
#include <ored/utilities/simmcurrencies.hpp>
#include <set>

using std::map;
using std::set;
using std::string;

namespace ore {
namespace data {

namespace {

std::map<std::string, std::string> nonStdCcys = {{"CLF", "CLP"}, {"CNH", "CNY"}, {"COU", "CUP"}, {"CUC", "CUP"},
                                                 {"MXV", "MXN"}, {"UYI", "UYU"}, {"UYW", "UYU"}};

std::set<std::string> unidadeCcys = {"CLF", "COU", "MXV", "UYW"};

} // namespace

bool isSimmNonStandardCurrency(const std::string& ccy) { return nonStdCcys.find(ccy) != nonStdCcys.end(); }

bool isUnidadeCurrency(const std::string& ccy) { return unidadeCcys.find(ccy) != unidadeCcys.end(); }

std::string simmStandardCurrency(const std::string& ccy) {
    if (auto c = nonStdCcys.find(ccy); c != nonStdCcys.end()) {
        return c->second;
    } else {
        return ccy;
    }
}

} // namespace data
} // namespace ore
