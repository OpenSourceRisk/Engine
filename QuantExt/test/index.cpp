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
} // namespace

namespace testsuite {

void dataCheck(struct IndTestData& data) {

    BOOST_CHECK_EQUAL(data.ind.familyName(), data.name);
    BOOST_CHECK_EQUAL(data.ind.fixingCalendar().name(), data.calName);
    BOOST_CHECK_EQUAL(data.ind.currency().name(), data.ccyName);
}

void IndexTest::testIborIndex() {

    BOOST_TEST_MESSAGE("Testing QuantExt indexes");

    Period pd(3, Months);

    struct IndTestData data[] = { { AUDbbsw(pd), "AUD-BBSW", Australia().name(), AUDCurrency().name() },
                                  { CHFTois(), "CHF-TOIS", Switzerland().name(), CHFCurrency().name() },
                                  { CORRA(), "CORRA", Canada().name(), CADCurrency().name() },
                                  { CZKPribor(pd), "CZK-PRIBOR", CzechRepublic().name(), CZKCurrency().name() },
                                  { DKKCibor(pd), "DKK-CIBOR", Denmark().name(), DKKCurrency().name() },
                                  { HKDHibor(pd), "HKD-HIBOR", HongKong().name(), HKDCurrency().name() },
                                  { HUFBubor(pd), "HUF-BUBOR", Hungary().name(), HUFCurrency().name() },
                                  { IDRIdrfix(pd), "IDR-IDRFIX", Indonesia().name(), IDRCurrency().name() },
                                  { INRMifor(pd), "INR-MIFOR", India().name(), INRCurrency().name() },
                                  { MXNTiie(pd), "MXN-TIIE", Mexico().name(), MXNCurrency().name() },
                                  { NOKNibor(pd), "NOK-NIBOR", Norway().name(), NOKCurrency().name() },
                                  { NZDBKBM(pd), "NZD-BKBM", NewZealand().name(), NZDCurrency().name() },
                                  { PLNWibor(pd), "PLN-WIBOR", Poland().name(), PLNCurrency().name() },
                                  { SEKStibor(pd), "SEK-STIBOR", Sweden().name(), SEKCurrency().name() },
                                  { SGDSibor(pd), "SGD-SIBOR", Singapore().name(), SGDCurrency().name() },
                                  { SGDSor(pd), "SGD-SOR", Singapore().name(), SGDCurrency().name() },
                                  { SKKBribor(pd), "SKK-BRIBOR", Slovakia().name(), SKKCurrency().name() },
                                  { Tonar(), "TONAR", Japan().name(), JPYCurrency().name() },
                                  { KRWKoribor(pd), "KRW-KORIBOR", SouthKorea().name(), KRWCurrency().name() },
                                  { MYRKlibor(pd), "MYR-KLIBOR", TARGET().name(), MYRCurrency().name() },
                                  { TWDTaibor(pd), "TWD-TAIBOR", Taiwan().name(), TWDCurrency().name() } };

    Size size = sizeof(data) / sizeof(data[0]);

    for (Size i = 0; i < size; i++)
        dataCheck(data[i]);
}

test_suite* IndexTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("IborIndexTests");
    suite->add(BOOST_TEST_CASE(&IndexTest::testIborIndex));
    return suite;
}
} // namespace testsuite
