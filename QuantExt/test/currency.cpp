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

#include "currency.hpp"
#include <ql/currency.hpp>
#include <qle/currencies/all.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace {

struct CcyTestData {
    Currency ccy;
    string name;
    string code;
    int numCode;
};
} // namespace

namespace testsuite {

void dataCheck(struct CcyTestData& data) {

    BOOST_CHECK_EQUAL(data.ccy.name(), data.name);
    BOOST_CHECK_EQUAL(data.ccy.code(), data.code);
    BOOST_CHECK_EQUAL(data.ccy.numericCode(), data.numCode);
}

void CurrencyTest::testCurrency() {

    BOOST_TEST_MESSAGE("Testing QuantExt currencies");

    CcyTestData data[] = {
        // African cuurencies
        { TNDCurrency(), "Tunisian dinar", "TND", 788 },
        { EGPCurrency(), "Egyptian pound", "EGP", 818 },
        { NGNCurrency(), "Nigerian naira", "NGN", 566 },
        { MADCurrency(), "Moroccan dirham", "MAD", 504 },
        // American currencies
        { MXVCurrency(), "Mexican Unidad de Inversion", "MXV", 979 },
        { CLFCurrency(), "Unidad de Fomento (funds code)", "CLF", 990 },
        // Asian currencies
        { KZTCurrency(), "Kazakhstani tenge", "KZT", 398 },
        { QARCurrency(), "Qatari riyal", "QAR", 634 },
        { BHDCurrency(), "Bahraini dinar", "BHD", 48 },
        { OMRCurrency(), "Omani rial", "OMR", 512 },
        { AEDCurrency(), "United Arab Emirates dirham", "AED", 784 },
        { PHPCurrency(), "Philippine peso", "PHP", 608 },
    };

    Size size = sizeof(data) / sizeof(data[0]);

    for (Size i = 0; i < size; i++)
        dataCheck(data[i]);
}

test_suite* CurrencyTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CurrencyTests");
    suite->add(BOOST_TEST_CASE(&CurrencyTest::testCurrency));
    return suite;
}
} // namespace testsuite
