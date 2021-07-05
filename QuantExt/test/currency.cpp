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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/currencies/all.hpp>
#include <ql/currency.hpp>
#include <qle/currencies/africa.hpp>
#include <qle/currencies/america.hpp>
#include <qle/currencies/asia.hpp>
#include <qle/currencies/europe.hpp>
#include <qle/currencies/metals.hpp>

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

static CcyTestData currencyData[] = {
    // African cuurencies
    { TNDCurrency(), "Tunisian dinar", "TND", 788 },
    { EGPCurrency(), "Egyptian pound", "EGP", 818 },
    { MADCurrency(), "Moroccan dirham", "MAD", 504 },
    // American currencies
    { MXVCurrency(), "Mexican Unidad de Inversion", "MXV", 979 },
    { CLFCurrency(), "Unidad de Fomento (funds code)", "CLF", 990 },
    { UYUCurrency(), "Uruguayan peso", "UYU", 858 },
    // Asian currencies
    { KZTCurrency(), "Kazakstanti Tenge", "KZT", 398 }, // note the typo in the QuantLib KZT
    { QARCurrency(), "Qatari riyal", "QAR", 634 },
    { BHDCurrency(), "Bahraini dinar", "BHD", 48 },
    { OMRCurrency(), "Omani rial", "OMR", 512 },
    { AEDCurrency(), "United Arab Emirates dirham", "AED", 784 },
    { PHPCurrency(), "Philippine peso", "PHP", 608 },
    { XAUCurrency(), "Troy Ounce of Gold", "XAU", 959 },
    { XAGCurrency(), "Troy Ounce of Silver", "XAG", 961 },
    { XPDCurrency(), "Troy Ounce of Palladium", "XPD", 964 },
    { XPTCurrency(), "Troy Ounce of Platinum", "XPT", 962 },
    { AOACurrency(), "Angolan kwanza", "AOA", 973 },
    { ETBCurrency(), "Ethiopian birr", "ETB", 230 },
    { XOFCurrency(), "West African CFA franc", "XOF", 952 },
    // European currencies
    { GELCurrency(), "Georgian lari", "GEL", 981 }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CurrencyTest)

BOOST_AUTO_TEST_CASE(testCurrency) {

    BOOST_TEST_MESSAGE("Testing QuantExt currencies");

    Size size = sizeof(currencyData) / sizeof(currencyData[0]);

    for (Size i = 0; i < size; i++) {
        BOOST_CHECK_EQUAL(currencyData[i].ccy.name(), currencyData[i].name);
        BOOST_CHECK_EQUAL(currencyData[i].ccy.code(), currencyData[i].code);
        BOOST_CHECK_EQUAL(currencyData[i].ccy.numericCode(), currencyData[i].numCode);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
