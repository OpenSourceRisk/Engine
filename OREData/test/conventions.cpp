/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>
#include <ored/configuration/conventions.hpp>
#include <ql/currencies/all.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>
#include <qle/calendars/ice.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantExt;
using namespace QuantLib;
using namespace ore::data;
using boost::algorithm::replace_all_copy;

namespace {

void testIborIndexConvention(const string& id, const string& fixingCalendar, const string& dayCounter,
    const Size settlementDays, const string& businessDayConvention, const bool endOfMonth, 
    const string& internalId = "") {
    // Check construction raises no errors
    QuantLib::ext::shared_ptr<IborIndexConvention> convention;
    BOOST_CHECK_NO_THROW(
        convention = QuantLib::ext::make_shared<IborIndexConvention>(id, fixingCalendar, dayCounter,
            settlementDays, businessDayConvention, endOfMonth));

    // Check object
    BOOST_CHECK_EQUAL(convention->id(), internalId.empty() ? id : internalId);
    BOOST_CHECK_EQUAL(convention->fixingCalendar(), fixingCalendar);
    BOOST_CHECK_EQUAL(convention->dayCounter(), dayCounter);
    BOOST_CHECK_EQUAL(convention->settlementDays(), settlementDays);
    BOOST_CHECK_EQUAL(convention->businessDayConvention(), businessDayConvention);
    BOOST_CHECK_EQUAL(convention->endOfMonth(), endOfMonth);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ConventionsTests)

BOOST_AUTO_TEST_CASE(testCrossCcyFixFloatSwapConventionConstruction) {

    BOOST_TEST_MESSAGE("Testing cross currency fix float convention construction");

    // Check construction raises no errors
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapConvention> convention;
    BOOST_CHECK_NO_THROW(
        convention = QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>("USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F",
                                                                        "TRY", "Annual", "F", "A360", "USD-LIBOR-3M"));

    // Check object
    BOOST_CHECK_EQUAL(convention->id(), "USD-TRY-XCCY-FIX-FLOAT");
    BOOST_CHECK_EQUAL(convention->settlementDays(), 2);
    BOOST_CHECK_EQUAL(convention->settlementCalendar(), JointCalendar(UnitedStates(UnitedStates::Settlement), UnitedKingdom(), Turkey()));
    BOOST_CHECK_EQUAL(convention->settlementConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedCurrency(), TRYCurrency());
    BOOST_CHECK_EQUAL(convention->fixedFrequency(), Annual);
    BOOST_CHECK_EQUAL(convention->fixedConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedDayCounter(), Actual360());
    BOOST_CHECK_EQUAL(convention->index()->name(), "USDLibor3M Actual/360");
    BOOST_CHECK(!convention->eom());

    // Check end of month when not default
    BOOST_CHECK_NO_THROW(
        convention = QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>(
            "USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F", "TRY", "Annual", "F", "A360", "USD-LIBOR-3M", "false"));
    BOOST_CHECK(!convention->eom());

    BOOST_CHECK_NO_THROW(
        convention = QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>(
            "USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F", "TRY", "Annual", "F", "A360", "USD-LIBOR-3M", "true"));
    BOOST_CHECK(convention->eom());
}

BOOST_AUTO_TEST_CASE(testCrossCcyFixFloatSwapConventionFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of cross currency fix float convention from XML");

    // XML string convention
    string xml;
    xml.append("<CrossCurrencyFixFloat>");
    xml.append("  <Id>USD-TRY-XCCY-FIX-FLOAT</Id>");
    xml.append("  <SettlementDays>2</SettlementDays>");
    xml.append("  <SettlementCalendar>US,UK,TRY</SettlementCalendar>");
    xml.append("  <SettlementConvention>F</SettlementConvention>");
    xml.append("  <FixedCurrency>TRY</FixedCurrency>");
    xml.append("  <FixedFrequency>Annual</FixedFrequency>");
    xml.append("  <FixedConvention>F</FixedConvention>");
    xml.append("  <FixedDayCounter>A360</FixedDayCounter>");
    xml.append("  <Index>USD-LIBOR-3M</Index>");
    xml.append("</CrossCurrencyFixFloat>");

    // Parse convention from XML
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapConvention> convention = QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>();
    BOOST_CHECK_NO_THROW(convention->fromXMLString(xml));

    // Check parsed object
    BOOST_CHECK_EQUAL(convention->id(), "USD-TRY-XCCY-FIX-FLOAT");
    BOOST_CHECK_EQUAL(convention->settlementDays(), 2);
    BOOST_CHECK_EQUAL(convention->settlementCalendar(), JointCalendar(UnitedStates(UnitedStates::Settlement), UnitedKingdom(), Turkey()));
    BOOST_CHECK_EQUAL(convention->settlementConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedCurrency(), TRYCurrency());
    BOOST_CHECK_EQUAL(convention->fixedFrequency(), Annual);
    BOOST_CHECK_EQUAL(convention->fixedConvention(), Following);
    BOOST_CHECK_EQUAL(convention->fixedDayCounter(), Actual360());
    BOOST_CHECK_EQUAL(convention->index()->name(), "USDLibor3M Actual/360");
    BOOST_CHECK(!convention->eom());

    // Check end of month also
    xml = replace_all_copy(xml, "</CrossCurrencyFixFloat>", "<EOM>false</EOM></CrossCurrencyFixFloat>");
    BOOST_TEST_MESSAGE("xml is: " << xml);
    convention->fromXMLString(xml);
    BOOST_CHECK(!convention->eom());

    xml = replace_all_copy(xml, "<EOM>false</EOM>", "<EOM>true</EOM>");
    convention->fromXMLString(xml);
    BOOST_CHECK(convention->eom());
}

BOOST_AUTO_TEST_CASE(testCrossCcyFixFloatSwapConventionToXml) {

    BOOST_TEST_MESSAGE("Testing writing of cross currency fix float convention to XML");

    // Construct the convention
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapConvention> convention;
    BOOST_CHECK_NO_THROW(
        convention = QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>("USD-TRY-XCCY-FIX-FLOAT", "2", "US,UK,TRY", "F",
                                                                        "TRY", "Annual", "F", "A360", "USD-LIBOR-3M"));

    // Write the convention to a string
    string xml = convention->toXMLString();

    // Read the convention back from the string using fromXMLString
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapConvention> readConvention =
        QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>();
    BOOST_CHECK_NO_THROW(readConvention->fromXMLString(xml));

    // The read convention should equal the original convention
    BOOST_CHECK_EQUAL(convention->id(), readConvention->id());
    BOOST_CHECK_EQUAL(convention->settlementDays(), readConvention->settlementDays());
    BOOST_CHECK_EQUAL(convention->settlementCalendar(), readConvention->settlementCalendar());
    BOOST_CHECK_EQUAL(convention->settlementConvention(), readConvention->settlementConvention());
    BOOST_CHECK_EQUAL(convention->fixedCurrency(), readConvention->fixedCurrency());
    BOOST_CHECK_EQUAL(convention->fixedFrequency(), readConvention->fixedFrequency());
    BOOST_CHECK_EQUAL(convention->fixedConvention(), readConvention->fixedConvention());
    BOOST_CHECK_EQUAL(convention->fixedDayCounter(), readConvention->fixedDayCounter());
    BOOST_CHECK_EQUAL(convention->index()->name(), readConvention->index()->name());
    BOOST_CHECK_EQUAL(convention->eom(), readConvention->eom());
}

BOOST_AUTO_TEST_CASE(testDayOfMonthCommodityFutureConventionConstruction) {

    BOOST_TEST_MESSAGE("Testing commodity future convention construction with day of month based anchor day");

    // Check construction raises no errors
    using PE = CommodityFutureConvention::ProhibitedExpiry;
    set<PE> prohibitedExpiries;
    prohibitedExpiries.insert(PE(Date(31, Dec, 2020)));
    prohibitedExpiries.insert(PE(Date(31, Dec, 2021)));
    prohibitedExpiries.insert(PE(Date(30, Dec, 2022)));
 
    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention;
    CommodityFutureConvention::DayOfMonth dayOfMonth("31");
    CommodityFutureConvention::CalendarDaysBefore optionExpiryBusinessDayBefore("3");
    CommodityFutureConvention::OptionExpiryAnchorDateRule optionExpiryDateRule(optionExpiryBusinessDayBefore);
    BOOST_CHECK_NO_THROW(convention = QuantLib::ext::make_shared<CommodityFutureConvention>(
                             "ICE:B", dayOfMonth, "Monthly", "ICE_FuturesEU", "UK", 2, "Jan", "0", "Preceding", true,
                             false, optionExpiryDateRule, prohibitedExpiries));

    // Check object
    BOOST_CHECK_EQUAL(convention->id(), "ICE:B");
    BOOST_CHECK(convention->anchorType() == CommodityFutureConvention::AnchorType::DayOfMonth);
    BOOST_CHECK_EQUAL(convention->dayOfMonth(), 31);
    BOOST_CHECK_EQUAL(convention->contractFrequency(), Monthly);
    BOOST_CHECK_EQUAL(convention->calendar(), ICE(ICE::FuturesEU));
    BOOST_CHECK_EQUAL(convention->expiryCalendar(), UnitedKingdom());
    BOOST_CHECK_EQUAL(convention->expiryMonthLag(), 2);
    BOOST_CHECK_EQUAL(convention->oneContractMonth(), Jan);
    BOOST_CHECK_EQUAL(convention->offsetDays(), 0);
    BOOST_CHECK_EQUAL(convention->businessDayConvention(), Preceding);
    BOOST_CHECK(convention->adjustBeforeOffset());
    BOOST_CHECK(!convention->isAveraging());
    BOOST_CHECK_EQUAL(convention->optionExpiryOffset(), 3);

    set<Date> expExpiries{Date(31, Dec, 2020), Date(31, Dec, 2021), Date(30, Dec, 2022)};
    const auto& proExps = convention->prohibitedExpiries();
    BOOST_CHECK_EQUAL(proExps.size(), expExpiries.size());
    for (const Date& expExpiry : expExpiries) {
        auto it = proExps.find(PE(expExpiry));
        BOOST_REQUIRE_MESSAGE(it != proExps.end(), "Expected date " << io::iso_date(expExpiry) <<
            " not found in Prohibited Expiries");
        BOOST_CHECK_EQUAL(it->expiry(), expExpiry);
        BOOST_CHECK(it->forFuture());
        BOOST_CHECK_EQUAL(it->futureBdc(), Preceding);
        BOOST_CHECK(it->forOption());
        BOOST_CHECK_EQUAL(it->optionBdc(), Preceding);
    }
}

BOOST_AUTO_TEST_CASE(testDayOfMonthCommodityFutureConventionFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of commodity future convention with day of month based anchor day from XML");

    // XML string convention
    string xml;
    xml.append("<CommodityFuture>");
    xml.append("  <Id>ICE:B</Id>");
    xml.append("  <AnchorDay>");
    xml.append("    <DayOfMonth>31</DayOfMonth>");
    xml.append("  </AnchorDay>");
    xml.append("  <ContractFrequency>Monthly</ContractFrequency>");
    xml.append("  <Calendar>ICE_FuturesEU</Calendar>");
    xml.append("  <ExpiryCalendar>UK</ExpiryCalendar>");
    xml.append("  <ExpiryMonthLag>2</ExpiryMonthLag>");
    xml.append("  <IsAveraging>false</IsAveraging>");
    xml.append("  <OptionExpiryOffset>3</OptionExpiryOffset>");
    xml.append("  <ProhibitedExpiries>");
    xml.append("    <Dates>");
    xml.append("      <Date>2020-12-31</Date>");
    xml.append("      <Date>2021-12-31</Date>");
    xml.append("      <Date>2022-12-30</Date>");
    xml.append("    </Dates>");
    xml.append("  </ProhibitedExpiries>");
    xml.append("</CommodityFuture>");

    // Parse convention from XML
    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention = QuantLib::ext::make_shared<CommodityFutureConvention>();
    BOOST_CHECK_NO_THROW(convention->fromXMLString(xml));

    // Check parsed object
    BOOST_CHECK_EQUAL(convention->id(), "ICE:B");
    BOOST_CHECK(convention->anchorType() == CommodityFutureConvention::AnchorType::DayOfMonth);
    BOOST_CHECK_EQUAL(convention->dayOfMonth(), 31);
    BOOST_CHECK_EQUAL(convention->contractFrequency(), Monthly);
    BOOST_CHECK_EQUAL(convention->calendar(), ICE(ICE::FuturesEU));
    BOOST_CHECK_EQUAL(convention->expiryCalendar(), UnitedKingdom());
    BOOST_CHECK_EQUAL(convention->expiryMonthLag(), 2);
    BOOST_CHECK_EQUAL(convention->oneContractMonth(), Jan);
    BOOST_CHECK_EQUAL(convention->offsetDays(), 0);
    BOOST_CHECK_EQUAL(convention->businessDayConvention(), Preceding);
    BOOST_CHECK(convention->adjustBeforeOffset());
    BOOST_CHECK(!convention->isAveraging());
    BOOST_CHECK_EQUAL(convention->optionExpiryOffset(), 3);

    using PE = CommodityFutureConvention::ProhibitedExpiry;
    set<Date> expExpiries{ Date(31, Dec, 2020), Date(31, Dec, 2021), Date(30, Dec, 2022) };
    const auto& proExps = convention->prohibitedExpiries();
    BOOST_CHECK_EQUAL(proExps.size(), expExpiries.size());
    for (const Date& expExpiry : expExpiries) {
        auto it = proExps.find(PE(expExpiry));
        BOOST_REQUIRE_MESSAGE(it != proExps.end(), "Expected date " << io::iso_date(expExpiry) <<
            " not found in Prohibited Expiries");
        BOOST_CHECK_EQUAL(it->expiry(), expExpiry);
        BOOST_CHECK(it->forFuture());
        BOOST_CHECK_EQUAL(it->futureBdc(), Preceding);
        BOOST_CHECK(it->forOption());
        BOOST_CHECK_EQUAL(it->optionBdc(), Preceding);
    }
}

BOOST_AUTO_TEST_CASE(testDayOfMonthCommodityFutureConventionToXml) {

    BOOST_TEST_MESSAGE("Testing writing of commodity future convention with day of month based anchor day to XML");

    // Construct the convention
    using PE = CommodityFutureConvention::ProhibitedExpiry;
    set<PE> prohibitedExpiries;
    prohibitedExpiries.insert(PE(Date(31, Dec, 2020), true, Following, false, ModifiedFollowing));
    prohibitedExpiries.insert(PE(Date(31, Dec, 2021), false, Preceding, true, ModifiedPreceding));
    prohibitedExpiries.insert(PE(Date(31, Dec, 2021), false, Following));

    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention;
    CommodityFutureConvention::DayOfMonth dayOfMonth("31");
    CommodityFutureConvention::CalendarDaysBefore optionExpiryBusinessDayBefore("3");
    CommodityFutureConvention::OptionExpiryAnchorDateRule optionExpiryDateRule(optionExpiryBusinessDayBefore);
    BOOST_CHECK_NO_THROW(convention = QuantLib::ext::make_shared<CommodityFutureConvention>(
                             "ICE:B", dayOfMonth, "Monthly", "ICE_FuturesEU", "UK", 2, "Jan", "0", "Preceding", true,
                             false, optionExpiryDateRule, prohibitedExpiries));

    // Write the convention to a string
    string xml = convention->toXMLString();

    // Read the convention back from the string using fromXMLString
    QuantLib::ext::shared_ptr<CommodityFutureConvention> readConvention = QuantLib::ext::make_shared<CommodityFutureConvention>();
    BOOST_CHECK_NO_THROW(readConvention->fromXMLString(xml));

    // The read convention should equal the original convention
    BOOST_CHECK_EQUAL(convention->id(), readConvention->id());
    BOOST_CHECK(convention->anchorType() == readConvention->anchorType());
    BOOST_CHECK_EQUAL(convention->dayOfMonth(), readConvention->dayOfMonth());
    BOOST_CHECK_EQUAL(convention->contractFrequency(), readConvention->contractFrequency());
    BOOST_CHECK_EQUAL(convention->calendar(), readConvention->calendar());
    BOOST_CHECK_EQUAL(convention->expiryMonthLag(), readConvention->expiryMonthLag());
    BOOST_CHECK_EQUAL(convention->oneContractMonth(), readConvention->oneContractMonth());
    BOOST_CHECK_EQUAL(convention->offsetDays(), readConvention->offsetDays());
    BOOST_CHECK_EQUAL(convention->businessDayConvention(), readConvention->businessDayConvention());
    BOOST_CHECK_EQUAL(convention->adjustBeforeOffset(), readConvention->adjustBeforeOffset());
    BOOST_CHECK_EQUAL(convention->isAveraging(), readConvention->isAveraging());

    const auto& proExps = convention->prohibitedExpiries();
    const auto& readProExps = readConvention->prohibitedExpiries();
    BOOST_CHECK_EQUAL(proExps.size(), readProExps.size());
    for (const auto& pe : proExps) {
        auto it = readProExps.find(pe);
        BOOST_REQUIRE_MESSAGE(it != readProExps.end(), "Expected date " << io::iso_date(pe.expiry()) <<
            " not found in Prohibited Expiries");
        BOOST_CHECK_EQUAL(it->expiry(), pe.expiry());
        BOOST_CHECK_EQUAL(it->forFuture(), pe.forFuture());
        BOOST_CHECK_EQUAL(it->futureBdc(), pe.futureBdc());
        BOOST_CHECK_EQUAL(it->forOption(), pe.forOption());
        BOOST_CHECK_EQUAL(it->optionBdc(), pe.optionBdc());
    }
}

BOOST_AUTO_TEST_CASE(testIborConventionConstructionWithTenor) {

    BOOST_TEST_MESSAGE("Testing Ibor Index convention construction with Tenor");
    testIborIndexConvention("AED-EIBOR-3M", "AED", "ACT/360", 2, "MF", false);
}

BOOST_AUTO_TEST_CASE(testIborConventionConstructionWithoutTenor) {

    BOOST_TEST_MESSAGE("Testing Ibor Index convention construction with Tenor");
    testIborIndexConvention("AED-EIBOR", "AED", "ACT/360", 2, "MF", false);
}

BOOST_AUTO_TEST_CASE(testIborConventionConstruction7D) {
    BOOST_TEST_MESSAGE("Testing Ibor Index convention construction with Tenor");
    testIborIndexConvention("CNY-REPO-7D", "CNY", "A365F", 2, "MF", true, "CNY-REPO-1W");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
