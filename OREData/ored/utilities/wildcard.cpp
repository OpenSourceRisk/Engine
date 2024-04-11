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

#include <ql/errors.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>

#include <memory>
#include <set>

namespace ore {
namespace data {

using namespace std;

Wildcard::Wildcard(const std::string& pattern, const bool usePrefixes, const bool aggressivePrefixes)
    : pattern_(pattern), usePrefixes_(usePrefixes), aggressivePrefixes_(aggressivePrefixes) {

    wildCardPos_ = pattern_.find("*");

    if (wildCardPos_ == std::string::npos)
        return;

    hasWildCard_ = true;

    if (usePrefixes && (aggressivePrefixes || wildCardPos_ == pattern_.size() - 1)) {
        prefixString_ = pattern_.substr(0, wildCardPos_);
    } else {
        regexString_ = pattern_;
        static std::vector<std::string> specialChars = {"\\", ".", "+", "?", "^", "$", "(",
                                                        ")",  "[", "]", "{", "}", "|"};
        for (auto const& c : specialChars) {
            boost::replace_all(*regexString_, c, "\\" + c);
        }
        boost::replace_all(*regexString_, "*", ".*");
    }
}

bool Wildcard::hasWildcard() const { return hasWildCard_; }

std::size_t Wildcard::wildcardPos() const { return wildCardPos_; }

bool Wildcard::isPrefix() const { return prefixString_ ? true : false; }

bool Wildcard::matches(const std::string& s) const {
    if (prefixString_) {
        return s.substr(0, (*prefixString_).size()) == (*prefixString_);
    } else if (regexString_) {
        if (regex_ == nullptr)
            regex_ = QuantLib::ext::make_shared<std::regex>(*regexString_);
        return std::regex_match(s, *regex_);
    } else {
        return s == pattern_;
    }
}

const std::string& Wildcard::pattern() const { return pattern_; }

const std::string& Wildcard::regex() const {
    QL_REQUIRE(regexString_, "string '" << pattern_ << "' is not a regex (usePrefixes = " << std::boolalpha
                                        << usePrefixes_ << ", aggressivePrefixes = " << aggressivePrefixes_
                                        << ", isPrefix = " << !prefixString_ << ")");
    return *regexString_;
}

const std::string& Wildcard::prefix() const {
    QL_REQUIRE(prefixString_, "string '" << pattern_ << "' is not a prefix (usePrefixes = " << std::boolalpha
                                         << usePrefixes_ << ", aggressivePrefixes = " << aggressivePrefixes_
                                         << ", isRegex = " << !regexString_ << ")");
    return *prefixString_;
}

void partitionQuotes(const set<string>& quoteNames, set<string>& names, set<string>& regexes) {

    for (const string& n : quoteNames) {
        ore::data::Wildcard w(n, false);
        if (w.hasWildcard())
            regexes.insert(w.regex());
        else
            names.insert(n);
    }
}

void partitionQuotes(const set<string>& quoteNames, set<string>& names, set<string>& regexes,
                     std::set<std::string>& prefixes, const bool aggressivePrefixes) {

    for (const string& n : quoteNames) {
        ore::data::Wildcard w(n, true, aggressivePrefixes);
        if (w.hasWildcard()) {
            if (w.isPrefix())
                prefixes.insert(w.prefix());
            else
                regexes.insert(w.regex());
        } else
            names.insert(n);
    }
}

} // namespace data
} // namespace ore
