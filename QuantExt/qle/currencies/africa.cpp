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

#include <qle/currencies/africa.hpp>

using namespace QuantLib;

namespace QuantExt {

// Tunisian dinar
TNDCurrency::TNDCurrency() {
    static boost::shared_ptr<Data> tndData(
        new Data("Tunisian dinar", "TND", 788, "TND", "", 1000, Rounding(), "1$.2f %3%"));
    data_ = tndData;
}
// Egyptian pound
EGPCurrency::EGPCurrency() {
    static boost::shared_ptr<Data> egpData(
        new Data("Egyptian pound", "EGP", 818, "EGP", "", 100, Rounding(), "1$.2f %3%"));
    data_ = egpData;
}

// Nigerian naira
NGNCurrency::NGNCurrency() {
    static boost::shared_ptr<Data> ngnData(
        new Data("Nigerian naira", "NGN", 566, "NGN", "", 100, Rounding(), "1$.2f %3%"));
    data_ = ngnData;
}

// Moroccan dirham
MADCurrency::MADCurrency() {
    static boost::shared_ptr<Data> madData(
        new Data("Moroccan dirham", "MAD", 504, "MAD", "", 100, Rounding(), "1$.2f %3%"));
    data_ = madData;
}
} // namespace QuantExt
