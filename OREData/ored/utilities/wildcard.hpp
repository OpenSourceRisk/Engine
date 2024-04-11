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

#pragma once

#include <boost/optional.hpp>
#include <ql/shared_ptr.hpp>

#include <regex>
#include <set>
#include <string>

namespace ore {
namespace data {

class Wildcard {
public:
    /*! all characters in s keep their original meaning except * which is a placeholder for zero or
      more characters not equal to newline */
    explicit Wildcard(const std::string& pattern, const bool usePrefixes = true, const bool aggressivePrefixes = false);

    bool hasWildcard() const;
    std::size_t wildcardPos() const; // string::npos if hasWildcard() == false
    bool isPrefix() const;

    bool matches(const std::string& s) const;

    const std::string& pattern() const;
    const std::string& regex() const;
    const std::string& prefix() const;

private:
    std::string pattern_;
    bool usePrefixes_;
    bool aggressivePrefixes_;

    bool hasWildCard_ = false;
    std::size_t wildCardPos_;
    boost::optional<std::string> regexString_;
    boost::optional<std::string> prefixString_;
    mutable QuantLib::ext::shared_ptr<std::regex> regex_;
};

//! checks if at most one element in C has a wild card and returns it in this case
template <class C> boost::optional<Wildcard> getUniqueWildcard(const C& c) {
    for (auto const& a : c) {
        Wildcard w(a);
        if (w.hasWildcard()) {
            QL_REQUIRE(c.size() == 1, "If wild cards are used, only one entry should exist.");
            return w;
        }
    }
    return boost::none;
}

// The quoteNames set can have a mix of exact RIC names and regex strings to match multiple RICs. This function splits
// them into two separate sets.
void partitionQuotes(const std::set<std::string>& quoteNames, std::set<std::string>& names,
                     std::set<std::string>& regexes);

// As above, but the split is into names, regexes and prefixes
void partitionQuotes(const std::set<std::string>& quoteNames, std::set<std::string>& names,
                     std::set<std::string>& regexes, std::set<std::string>& prefixes,
                     const bool aggressivePrefixes = false);

} // namespace data
} // namespace ore
