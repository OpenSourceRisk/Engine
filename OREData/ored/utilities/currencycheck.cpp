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

#include <ored/utilities/currencycheck.hpp>
#include <ored/utilities/parsers.hpp>

#include <algorithm>
#include <vector>

namespace ore {
namespace data {
bool checkCurrency(const string& s) {
    //! see http://www.currency-iso.org/en/home/tables/table-a1.html (published July 1, 2016)
    const static std::vector<string> codes = {
        "AED", "AFN", "ALL", "AMD", "ANG", "AOA", "ARS", "AUD", "AWG", "AZN", "BAM", "BBD", "BDT", "BGN", "BHD",
        "BIF", "BMD", "BND", "BOB", "BOV", "BRL", "BSD", "BTN", "BWP", "BYN", "BYR", "BZD", "CAD", "CDF", "CHE",
        "CHF", "CHW", "CLF", "CLP", "CNH", "CNY", "COP", "COU", "CRC", "CUC", "CUP", "CVE", "CZK", "DJF", "DKK",
        "DOP", "DZD", "EGP", "ERN", "ETB", "EUR", "FJD", "FKP", "GBP", "GEL", "GHS", "GIP", "GMD", "GNF", "GTQ",
        "GYD", "HKD", "HNL", "HRK", "HTG", "HUF", "IDR", "ILS", "INR", "IQD", "IRR", "ISK", "JMD", "JOD", "JPY",
        "KES", "KGS", "KHR", "KMF", "KPW", "KRW", "KWD", "KYD", "KZT", "LAK", "LBP", "LKR", "LRD", "LSL", "LYD",
        "MAD", "MDL", "MGA", "MKD", "MMK", "MNT", "MOP", "MRO", "MUR", "MVR", "MWK", "MXN", "MXV", "MYR", "MZN",
        "NAD", "NGN", "NIO", "NOK", "NPR", "NZD", "OMR", "PAB", "PEN", "PGK", "PHP", "PKR", "PLN", "PYG", "QAR",
        "RON", "RSD", "RUB", "RWF", "SAR", "SBD", "SCR", "SDG", "SEK", "SGD", "SHP", "SLL", "SOS", "SRD", "SSP",
        "STD", "SVC", "SYP", "SZL", "THB", "TJS", "TMT", "TND", "TOP", "TRY", "TTD", "TWD", "TZS", "UAH", "UGX",
        "USD", "USN", "UYI", "UYU", "UZS", "VEF", "VND", "VUV", "WST", "XAF", "XAG", "XAU", "XBA", "XBB", "XBC",
        "XBD", "XCD", "XDR", "XOF", "XPD", "XPF", "XPT", "XSU", "XTS", "XUA", "XXX", "YER", "ZAR", "ZMW", "ZWL",
        "GBp", "GBX", "ILa", "ILX", "ZAc", "ZAC", "ZAX" };
    auto it = std::find(codes.begin(), codes.end(), s);
    //return it != codes.end();
    if (it != codes.end())
        return true;
    else {
        try {
	    // It might be an "external" currency if the following passes
	    parseCurrency(s);
	    return true;
	}
	catch(...) {
	    return false;
	}
    }
}

bool checkMinorCurrency(const string& s) {
    //! list of supported minor currencies
    const static std::vector<string> codes = {
       "GBp", "GBX", "ILa", "ILX", "ZAc", "ZAC", "ZAX" };
    auto it = std::find(codes.begin(), codes.end(), s);
    return it != codes.end();
}

QuantLib::Real convertMinorToMajorCurrency(const string& s, QuantLib::Real value) {
    if (checkMinorCurrency(s)) {
        QuantLib::Currency ccy = parseMinorCurrency(s);
        return value / ccy.fractionsPerUnit();
    } else {
        return value;
    }
}
} // namespace data
} // namespace ore
