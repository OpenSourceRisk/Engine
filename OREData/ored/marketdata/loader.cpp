/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/marketdata/loader.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<MarketDatum> Loader::get(const std::string& name, const QuantLib::Date& d) const {
    for (const auto& md : loadQuotes(d)) {
        if (md->name() == name)
            return md;
    }
    QL_FAIL("No MarketDatum for name " << name << " and date " << d);
}

std::set<QuantLib::ext::shared_ptr<MarketDatum>> Loader::get(const std::set<std::string>& names,
                                                     const QuantLib::Date& asof) const {
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> result;
    for (auto const& md : loadQuotes(asof)) {
        if (names.find(md->name()) != names.end()) {
            result.insert(md);
        }
    }
    return result;
}

std::set<QuantLib::ext::shared_ptr<MarketDatum>> Loader::get(const Wildcard& wildcard, const QuantLib::Date& asof) const {
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> result;
    for (auto const& md : loadQuotes(asof)) {
        if (wildcard.matches(md->name())) {
            result.insert(md);
        }
    }
    return result;
}

bool Loader::has(const std::string& name, const QuantLib::Date& d) const {
    try {
        return get(name, d) != nullptr;
    } catch (...) {
        return false;
    }
}

bool Loader::hasQuotes(const QuantLib::Date& d) const {
    try {
        return !loadQuotes(d).empty();
    } catch (...) {
        return false;
    }
}

QuantLib::ext::shared_ptr<MarketDatum> Loader::get(const std::pair<std::string, bool>& name, const QuantLib::Date& d) const {
    if (has(name.first, d)) {
        return get(name.first, d);
    } else {
        const Date& originalDate = actualDate() == Null<Date>() ? d : actualDate();
        if (name.second) {
            DLOG("Could not find quote for ID " << name.first << " with as of date " << QuantLib::io::iso_date(originalDate)
                                                << ".");
            return QuantLib::ext::shared_ptr<MarketDatum>();
        } else {
            QL_FAIL("Could not find quote for Mandatory ID " << name.first << " with as of date "
                                                             << QuantLib::io::iso_date(originalDate));
        }
    }
}

std::pair<bool, string> Loader::checkFxDuplicate(const ext::shared_ptr<MarketDatum> md, const QuantLib::Date& d) {
    string cc1 = ext::dynamic_pointer_cast<FXSpotQuote>(md)->unitCcy();
    string cc2 = ext::dynamic_pointer_cast<FXSpotQuote>(md)->ccy();
    string tmp = "FX/RATE/" + cc2 + "/" + cc1;
    if (Loader::has(tmp, d)) {
        string dom = fxDominance(cc1, cc2);
        if (dom == (cc1 + cc2)) {
            return {true, tmp};
        } else {
            return {false, ""};
        }
    }
    return {true, ""};
}

bool Loader::hasFixing(const string& name, const QuantLib::Date& d) const {
    try {
        return !getFixing(name, d).empty();
    } catch (...) {
        return false;
    }
}

Fixing Loader::getFixing(const string& name, const QuantLib::Date& d) const {
    Fixing fixing;
    for (auto const& f : loadFixings()) {
        if (f.name == name && f.date == d) {
            fixing = f;
        }
    }
    return fixing;
}

std::set<QuantExt::Dividend> Loader::loadDividends() const { return {}; }

} // namespace data
} // namespace ore
