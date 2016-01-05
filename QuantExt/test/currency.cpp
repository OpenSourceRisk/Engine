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

void dataCheck(Currency ccy, string name, string code, int numCode) {
    BOOST_CHECK_EQUAL(ccy.name(),name);
    BOOST_CHECK_EQUAL(ccy.code(),code);
    BOOST_CHECK_EQUAL(ccy.numericCode(),numCode);

}
void CurrencyTest::testCurrency() {

    BOOST_MESSAGE("Testing QuantExt currencies");

    Currency ccy;

    //testing african currencies
    ccy=TNDCurrency();
    dataCheck(ccy,"Tunisian dinar", "TND", 788);    

    ccy=EGPCurrency();
    dataCheck(ccy,"Egyptian pound", "EGP", 818); 

    ccy=NGNCurrency();
    dataCheck(ccy,"Nigerian naira", "NGN", 566);

    ccy=MADCurrency();
    dataCheck(ccy,"Moroccan dirham", "MAD", 504);

    //testing american currencies
    ccy=MXVCurrency();
    dataCheck(ccy,"Mexican Unidad de Inversion", "MXV", 979);

    ccy=CLFCurrency();
    dataCheck(ccy,"Unidad de Fomento (funds code)", "CLF", 990);

    //testing asian currencies
    ccy=KZTCurrency();
    dataCheck(ccy,"Kazakhstani tenge", "KZT", 398);

    ccy=QARCurrency();
    dataCheck(ccy,"Qatari riyal", "QAR", 634);

    ccy=BHDCurrency();
    dataCheck(ccy,"Bahraini dinar", "BHD", 48);

    ccy=OMRCurrency();
    dataCheck(ccy,"Omani rial", "OMR", 512);

    ccy=AEDCurrency();
    dataCheck(ccy,"United Arab Emirates dirham", "AED", 784);

    ccy=PHPCurrency();
    dataCheck(ccy,"Philippine peso", "PHP", 608);

    //testing european currencies
    ccy=UAHCurrency();
    dataCheck(ccy,"Ukrainian hryvnia", "UAH", 980);
 
}

test_suite* CurrencyTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CurrencyTests");
    suite->add(BOOST_TEST_CASE(&CurrencyTest::testCurrency));
    return suite;
}
