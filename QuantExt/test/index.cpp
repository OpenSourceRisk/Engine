/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "index.hpp"
#include <ql/currency.hpp>
#include <ql/index.hpp>
#include <qle/indexes/ibor/all.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace {
    
    struct IndTestData {
        IborIndex ind;
        string name;
        string calName;
        string ccyName;
    };

}


void dataCheck(struct IndTestData& data) {

    BOOST_CHECK_EQUAL(data.ind.familyName(), data.name);
    BOOST_CHECK_EQUAL(data.ind.fixingCalendar().name(), data.calName);
    BOOST_CHECK_EQUAL(data.ind.currency().name(), data.ccyName);

}


void IndexTest::testIborIndex() {

    BOOST_TEST_MESSAGE("Testing QuantExt indexes");

    Period pd(3, Months);

    struct IndTestData data[]= {
        { AUDbbsw(pd), "AUD-BBSW", Australia().name(), AUDCurrency().name()},
        { AverageBMA(pd), "AverageBMA", UnitedStates(UnitedStates::NYSE).name(), USDCurrency().name()},
        { CHFTois(), "CHF-TOIS", Switzerland().name(), CHFCurrency().name()},
        { CZKPribor(pd), "CZK-PRIBOR", CzechRepublic().name(), CZKCurrency().name()},
        { DKKCibor(pd), "DKK-CIBOR", Denmark().name(), DKKCurrency().name()},
        { HKDHibor(pd), "HKD-HIBOR", HongKong().name(), HKDCurrency().name()},
        { HUFBubor(pd), "HUF-BUBOR", Hungary().name(), HUFCurrency().name()},
        { IDRIdrfix(pd), "IDR-IDRFIX", Indonesia().name(), IDRCurrency().name()},
        { INRMifor(pd), "INR-MIFOR", India().name(), INRCurrency().name()},
        { MXNTiie(pd), "MXN-TIIE", Mexico().name(), MXNCurrency().name()},
        { NOKNibor(pd), "NOK-NIBOR", Norway().name(), NOKCurrency().name()},
        { NZDBKBM(pd), "NZD-BKBM", NewZealand().name(), NZDCurrency().name()},
        { PLNWibor(pd), "PLN-WIBOR", Poland().name(), PLNCurrency().name()},
        { SEKStibor(pd), "SEK-STIBOR", Sweden().name(), SEKCurrency().name()},
        { SGDSibor(pd), "SGD-SIBOR", Singapore().name(), SGDCurrency().name()},
        { SGDSor(pd), "SGD-SOR", Singapore().name(), SGDCurrency().name()},
        { SKKBribor(pd), "SKK-BRIBOR", Slovakia().name(), SKKCurrency().name()},
        { Tonar(), "TONAR", Japan().name(), JPYCurrency().name()}
    };

    Size size=sizeof(data)/sizeof(data[0]);

    for(Size i=0; i<size; i++)
        dataCheck(data[i]);
 
}

test_suite* IndexTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("IborIndexTests");
    suite->add(BOOST_TEST_CASE(&IndexTest::testIborIndex));
    return suite;
}
