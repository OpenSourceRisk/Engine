/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
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

}


void dataCheck(struct CcyTestData& data) {

    BOOST_CHECK_EQUAL(data.ccy.name(), data.name);
    BOOST_CHECK_EQUAL(data.ccy.code(), data.code);
    BOOST_CHECK_EQUAL(data.ccy.numericCode(), data.numCode);
}


void CurrencyTest::testCurrency() {

    BOOST_TEST_MESSAGE("Testing QuantExt currencies");

    CcyTestData data[]={
        //African cuurencies
        { TNDCurrency(), "Tunisian dinar", "TND", 788},
        { EGPCurrency(), "Egyptian pound", "EGP", 818},
        { NGNCurrency(), "Nigerian naira", "NGN", 566},
        { MADCurrency(), "Moroccan dirham", "MAD", 504},
        //American currencies
        { MXVCurrency(), "Mexican Unidad de Inversion", "MXV", 979},
        { CLFCurrency(), "Unidad de Fomento (funds code)", "CLF", 990},
        //Asian currencies
        { KZTCurrency(), "Kazakhstani tenge", "KZT", 398},
        { QARCurrency(), "Qatari riyal", "QAR", 634},
        { BHDCurrency(), "Bahraini dinar", "BHD", 48},
        { OMRCurrency(), "Omani rial", "OMR", 512},
        { AEDCurrency(), "United Arab Emirates dirham", "AED", 784},
        { PHPCurrency(), "Philippine peso", "PHP", 608},
        //European currencies
        { UAHCurrency(), "Ukrainian hryvnia", "UAH", 980}
    };

    Size size=sizeof(data)/sizeof(data[0]);

    for(Size i=0; i<size; i++)
        dataCheck(data[i]);

}


test_suite* CurrencyTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CurrencyTests");
    suite->add(BOOST_TEST_CASE(&CurrencyTest::testCurrency));
    return suite;
}
