/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <boost/test/unit_test.hpp>
#include <oret/toplevelfixture.hpp>

#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/currencies/europe.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

namespace {

LegData premiumLegData() {
    
    ScheduleData scheduleData(ScheduleRules("2019-10-02", "2024-12-20", "3M",
        "WeekendsOnly", "Following", "Unadjusted", "CDS2015"));

    auto fixedLegData = boost::make_shared<FixedLegData>(vector<Real>(1, 0.01));

    return LegData(fixedLegData, true, "EUR", scheduleData, "A360",
        vector<Real>(1, 1000000), vector<string>(), "Following");
}

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CdsReferenceInformationTests)

BOOST_AUTO_TEST_CASE(testToFromXml) {

    BOOST_TEST_MESSAGE("Testing toXML and fromXml for CdsReferenceInformation");

    // Explicitly create a CdsReferenceInformation object
    string referenceEntityId = "RED:2H6677";
    CdsTier tier = CdsTier::SNRFOR;
    Currency currency = EURCurrency();
    CdsDocClause docClause = CdsDocClause::MM14;
    CdsReferenceInformation inRef(referenceEntityId, tier, currency, docClause);

    // Check the id() is as expected
    string expId = referenceEntityId + "|" + to_string(tier) + "|" + currency.code() + "|" + to_string(docClause);
    BOOST_CHECK_EQUAL(inRef.id(), expId);

    // Use toXml to serialise to string
    string xmlStr = inRef.toXMLString();

    // Use fromXml to populate empty CdsReferenceInformation object
    CdsReferenceInformation outRef;
    outRef.fromXMLString(xmlStr);

    // Check against the original object
    BOOST_CHECK_EQUAL(inRef.referenceEntityId(), outRef.referenceEntityId());
    BOOST_CHECK_EQUAL(inRef.tier(), outRef.tier());
    BOOST_CHECK_EQUAL(inRef.currency(), outRef.currency());
    BOOST_CHECK_EQUAL(inRef.docClause(), outRef.docClause());
    BOOST_CHECK_EQUAL(inRef.id(), outRef.id());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(CreditDefaultSwapDataTests)

BOOST_AUTO_TEST_CASE(testExplicitCreditCurveId) {

    // Construct with explicit credit curve ID
    string cdsCurveId = "RED:2H6677|SNRFOR|EUR|MM14";
    CreditDefaultSwapData cdsData("DB", cdsCurveId, premiumLegData());

    // Perform some checks

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
