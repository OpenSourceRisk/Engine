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
#include <ql/currency.hpp>
#include <ql/index.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/indexes/ibor/chfsaron.hpp>
#include <qle/indexes/ibor/chftois.hpp>
#include <qle/indexes/ibor/clpcamara.hpp>
#include <qle/indexes/ibor/cnyrepofix.hpp>
#include <qle/indexes/ibor/copibr.hpp>
#include <qle/indexes/ibor/corra.hpp>
#include <qle/indexes/ibor/czkpribor.hpp>
#include <qle/indexes/ibor/demlibor.hpp>
#include <qle/indexes/ibor/dkkcibor.hpp>
#include <qle/indexes/ibor/dkkcita.hpp>
#include <qle/indexes/ibor/dkkois.hpp>
#include <qle/indexes/ibor/hkdhibor.hpp>
#include <qle/indexes/ibor/hufbubor.hpp>
#include <qle/indexes/ibor/idridrfix.hpp>
#include <qle/indexes/ibor/idrjibor.hpp>
#include <qle/indexes/ibor/ilstelbor.hpp>
#include <qle/indexes/ibor/inrmiborois.hpp>
#include <qle/indexes/ibor/inrmifor.hpp>
#include <qle/indexes/ibor/jpyeytibor.hpp>
#include <qle/indexes/ibor/krwcd.hpp>
#include <qle/indexes/ibor/krwkoribor.hpp>
#include <qle/indexes/ibor/mxntiie.hpp>
#include <qle/indexes/ibor/myrklibor.hpp>
#include <qle/indexes/ibor/noknibor.hpp>
#include <qle/indexes/ibor/nzdbkbm.hpp>
#include <qle/indexes/ibor/phpphiref.hpp>
#include <qle/indexes/ibor/seksior.hpp>
#include <qle/indexes/ibor/sekstibor.hpp>
#include <qle/indexes/ibor/sekstina.hpp>
#include <qle/indexes/ibor/sgdsibor.hpp>
#include <qle/indexes/ibor/sgdsor.hpp>
#include <qle/indexes/ibor/skkbribor.hpp>
#include <qle/indexes/ibor/sora.hpp>
#include <qle/indexes/ibor/thbbibor.hpp>
#include <qle/indexes/ibor/tonar.hpp>
#include <qle/indexes/ibor/twdtaibor.hpp>

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

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(IndexTest)

BOOST_AUTO_TEST_CASE(testIborIndex) {

    BOOST_TEST_MESSAGE("Testing QuantExt indexes");

    Period pd(3, Months);

    IndTestData data[] = {
        { CHFTois(), "CHF-TOIS", Switzerland().name(), CHFCurrency().name() },
        { CHFSaron(), "CHF-SARON", Switzerland().name(), CHFCurrency().name() },
        { CORRA(), "CORRA", Canada().name(), CADCurrency().name() },
        { CZKPribor(pd), "CZK-PRIBOR", CzechRepublic().name(), CZKCurrency().name() },
        { DKKCibor(pd), "DKK-CIBOR", Denmark().name(), DKKCurrency().name() },
        { DKKCita(), "DKK-CITA", Denmark().name(), DKKCurrency().name() },
        { DKKOis(), "DKK-DKKOIS", Denmark().name(), DKKCurrency().name() },
        { HKDHibor(pd), "HKD-HIBOR", HongKong().name(), HKDCurrency().name() },
        { HUFBubor(pd), "HUF-BUBOR", Hungary().name(), HUFCurrency().name() },
        { IDRIdrfix(pd), "IDR-IDRFIX", Indonesia().name(), IDRCurrency().name() },
        { IDRJibor(pd), "IDR-JIBOR", Indonesia().name(), IDRCurrency().name() },
        { ILSTelbor(pd), "ILS-TELBOR", QuantExt::Israel(QuantExt::Israel::Telbor).name(), ILSCurrency().name() },
        { INRMiborOis(), "INR-MIBOROIS", India().name(), INRCurrency().name() },
        { INRMifor(pd), "INR-MIFOR", India().name(), INRCurrency().name() },
        { JPYEYTIBOR(pd), "JPY-EYTIBOR", Japan().name(), JPYCurrency().name() },
        { MXNTiie(pd), "MXN-TIIE", Mexico().name(), MXNCurrency().name() },
        { NOKNibor(pd), "NOK-NIBOR", Norway().name(), NOKCurrency().name() },
        { NZDBKBM(pd), "NZD-BKBM", NewZealand().name(), NZDCurrency().name() },
        { SEKStibor(pd), "SEK-STIBOR", Sweden().name(), SEKCurrency().name() },
        { SEKStina(), "SEK-STINA", Sweden().name(), SEKCurrency().name() },
        { SEKSior(), "SEK-SIOR", Sweden().name(), SEKCurrency().name() },
        { SGDSibor(pd), "SGD-SIBOR", Singapore().name(), SGDCurrency().name() },
        { SGDSor(pd), "SGD-SOR", Singapore().name(), SGDCurrency().name() },
        { SKKBribor(pd), "SKK-BRIBOR", Slovakia().name(), SKKCurrency().name() },
        { Tonar(), "TONAR", Japan().name(), JPYCurrency().name() },
        { Sora(), "SGD-SORA", Singapore().name(), SGDCurrency().name() },
        { KRWCd(pd), "KRW-CD", SouthKorea(SouthKorea::Settlement).name(), KRWCurrency().name() },
        { KRWKoribor(pd), "KRW-KORIBOR", SouthKorea(SouthKorea::Settlement).name(), KRWCurrency().name() },
        { MYRKlibor(pd), "MYR-KLIBOR", Malaysia().name(), MYRCurrency().name() },
        { TWDTaibor(pd), "TWD-TAIBOR", Taiwan().name(), TWDCurrency().name() },
        { CNYRepoFix(pd), "CNY-REPOFIX", China(China::IB).name(), CNYCurrency().name() }
    };

    Size size = sizeof(data) / sizeof(data[0]);

    for (Size i = 0; i < size; i++) {
        BOOST_CHECK_EQUAL(data[i].ind.familyName(), data[i].name);
        BOOST_CHECK_EQUAL(data[i].ind.fixingCalendar().name(), data[i].calName);
        BOOST_CHECK_EQUAL(data[i].ind.currency().name(), data[i].ccyName);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
