/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/wildcard.hpp
    \brief utilities for wildcard handling
    \ingroup utilities
*/

#include <ored/utilities/wildcard.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>

#include <memory>
#include <set>

namespace ore {
namespace data {

using namespace std;

Wildcard::Wildcard(const std::string& s) : s_(s) {

    if (s_.find("*") == std::string::npos)
        return;

    static std::vector<std::string> specialChars = {"\\", ".", "+", "?", "^", "$", "(", ")", "[", "]", "{", "}", "|"};

    for (auto const& c : specialChars) {
        boost::replace_all(s_, c, "\\" + c);
    }

    boost::replace_all(s_, "*", ".*");

    regex_ = boost::make_shared<std::regex>(s_);
}

bool Wildcard::hasWildcard() const { return regex_ != nullptr; }

bool Wildcard::matches(const std::string& s) const {
    if (regex_ != nullptr) {
        return std::regex_match(s, *regex_);
    } else {
        return s == s_;
    }
}

const std::string& Wildcard::regex() const { return s_; }

void partitionQuotes(const set<string>& quoteNames, set<string>& names, set<string>& regexes) {

    for (const string& n : quoteNames) {
        ore::data::Wildcard w(n);
        if (w.hasWildcard())
            regexes.insert(w.regex());
        else
            names.insert(n);
    }
}

} // namespace data
} // namespace ore
