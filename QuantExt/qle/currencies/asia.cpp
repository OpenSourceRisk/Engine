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

#include <qle/currencies/asia.hpp>

using namespace QuantLib;

namespace QuantExt {

// Kazakhstani tenge
KZTCurrency::KZTCurrency() {
    static boost::shared_ptr<Data> kztData(
        new Data("Kazakhstani tenge", "KZT", 398, "KZT", "", 100, Rounding(), "1$.2f %3%"));
    data_ = kztData;
}

// Qatari riyal
QARCurrency::QARCurrency() {
    static boost::shared_ptr<Data> qarData(
        new Data("Qatari riyal", "QAR", 634, "QAR", "", 100, Rounding(), "1$.2f %3%"));
    data_ = qarData;
}

// Bahraini dinar
BHDCurrency::BHDCurrency() {
    static boost::shared_ptr<Data> bhdData(
        new Data("Bahraini dinar", "BHD", 48, "BHD", "", 1000, Rounding(), "1$.2f %3%"));
    data_ = bhdData;
}

// Omani rial
OMRCurrency::OMRCurrency() {
    static boost::shared_ptr<Data> omrData(
        new Data("Omani rial", "OMR", 512, "OMR", "", 1000, Rounding(), "1$.2f %3%"));
    data_ = omrData;
}

// United Arab Emirates dirham
AEDCurrency::AEDCurrency() {
    static boost::shared_ptr<Data> aedData(
        new Data("United Arab Emirates dirham", "AED", 784, "AED", "", 100, Rounding(), "1$.2f %3%"));
    data_ = aedData;
}

// Philippine peso
PHPCurrency::PHPCurrency() {
    static boost::shared_ptr<Data> phpData(
        new Data("Philippine peso", "PHP", 608, "PHP", "", 100, Rounding(), "1$.2f %3%"));
    data_ = phpData;
}
} // namespace QuantExt
