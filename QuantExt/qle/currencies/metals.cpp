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

#include <qle/currencies/metals.hpp>
#include <set>

using namespace QuantLib;
using std::set;

namespace QuantExt {

// Troy ounce of Gold
XAUCurrency::XAUCurrency() {
    static QuantLib::ext::shared_ptr<Data> xauData(
        new Data("Troy Ounce of Gold", "XAU", 959, "XAU", "", 1, Rounding(), "1$.2f %3%"));
    data_ = xauData;
}

// Troy ounce of Silver
XAGCurrency::XAGCurrency() {
    static QuantLib::ext::shared_ptr<Data> xagData(
        new Data("Troy Ounce of Silver", "XAG", 961, "XAG", "", 1, Rounding(), "1$.2f %3%"));
    data_ = xagData;
}

// Troy ounce of Platinum
XPTCurrency::XPTCurrency() {
    static QuantLib::ext::shared_ptr<Data> xptData(
        new Data("Troy Ounce of Platinum", "XPT", 962, "XPT", "", 1, Rounding(), "1$.2f %3%"));
    data_ = xptData;
}

// Troy ounce of Palladium
XPDCurrency::XPDCurrency() {
    static QuantLib::ext::shared_ptr<Data> xpdData(
        new Data("Troy Ounce of Palladium", "XPD", 964, "XPD", "", 1, Rounding(), "1$.2f %3%"));
    data_ = xpdData;
}

bool isMetal(const Currency& currency) {

    static auto cmpCcy = [](const Currency& c1, const Currency& c2) { return c1.name() < c2.name(); };
    static set<Currency, decltype(cmpCcy)> metals(cmpCcy);

    if (metals.empty()) {
        metals.insert(XAUCurrency());
        metals.insert(XAGCurrency());
        metals.insert(XPTCurrency());
        metals.insert(XPDCurrency());
    }

    return metals.count(currency) == 1;
}

} // namespace QuantExt
