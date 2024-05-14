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

#include <boost/test/unit_test.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <oret/toplevelfixture.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>

using namespace std;
using namespace QuantLib;

namespace {

std::vector<std::string> getMarketDataStrings() {
    // clang-format off
    return boost::assign::list_of
        // borrow spread curve
        ("20160226 EQUITY/PRICE/SP5/USD 1650.17")
        ("20160226 EQUITY/PRICE/Lufthansa/EUR 17.56")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/1Y 1678.54")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/2017-02-26 1678.54")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/20170226 1678.54")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/365D 1678.54")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/1678W5D 1900.50")
        ("20160226 EQUITY_FWD/PRICE/Lufthansa/EUR/1Y1M 1678.54")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/1Y 0.025")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/2017-02-26 0.025")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/20170226 0.025")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/365D 0.025")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/1678W5D 0.025")
        ("20160226 EQUITY_DIVIDEND/RATE/Lufthansa/EUR/1Y1M 0.013")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/12M/ATMF 0.25")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/2017-02-26/ATMF 0.25")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/20170226/ATMF 0.25")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/365D/ATMF 0.25")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/1678W5D/ATMF 0.25")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/Lufthansa/EUR/1Y1M/ATMF 0.13");
}

std::vector<std::string> getBadMarketDataStrings() {
    // clang-format off
    return boost::assign::list_of
        // borrow spread curve
        ("20160226 EQUITY_SPOT/PRICE/SP5/USD 1650.17") // incorrect instrument type
        ("20160226 EQUITY/RATE/Lufthansa/EUR 17.56") // incorrect quote type
        ("20160226 EQUITY_FORWARD/PRICE/SP5/USD/1Y 1678.54") // incorrect instrument type
        ("20160226 EQUITY_FWD/SPREAD/SP5/USD/2017-02-26 1678.54") // incorrect quote type
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/zzz 1678.54") // incorrect expiry
        ("20160226 EQUITY_DIV_YIELD/RATE/SP5/USD/1Y 1678.54") // incorrect instrument type
        ("20160226 EQUITY_DIVIDEND/PRICE/SP5/USD/2017-02-26 1678.54") // incorrect quote type
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/zzz 1678.54") // incorrect expiry
        ("20160226 EQUITY_OPTION_VOL/RATE_LNVOL/SP5/USD/12M/ATMF 0.25") // incorrect instrument type
        ("20160226 EQUITY_OPTION/RATE_NVOL/SP5/USD/2017-02-26/ATMF 0.25") // normal vols not supported for equity
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/zzz/ATMF 0.25"); // invalid tenor/date input
}

std::string divYieldCurveConfigString =
"<CurveConfiguration>"
"<EquityCurves>"
"<EquityCurve>"
"<CurveId>SP5</CurveId>"
"<ForecastingCurve>USD1D</ForecastingCurve>"
"<CurveDescription>SP 500 equity price projection curve</CurveDescription>"
"<Currency>USD</Currency> <!--is this really needed ? -->"
"<Type>DividendYield</Type> <!-- {DividendYield, ForwardPrice} -->"
"<SpotQuote>EQUITY/PRICE/SP5/USD</SpotQuote> <!--the spot quote from the market data file-->"
"<Quotes>"
"<Quote>EQUITY_DIVIDEND/RATE/SP5/USD/1M</Quote>"
"<Quote>EQUITY_DIVIDEND/RATE/SP5/USD/2016-09-15</Quote>"
"<Quote>EQUITY_DIVIDEND/RATE/SP5/USD/1Y</Quote>"
"<Quote>EQUITY_DIVIDEND/RATE/SP5/USD/2Y</Quote>"
"<Quote>EQUITY_DIVIDEND/RATE/SP5/USD/5Y</Quote>"
"</Quotes>"
"<DayCounter>A365</DayCounter>"
"</EquityCurve>"
"</EquityCurves>"
"</CurveConfiguration>";

std::string eqBadConfigString =
"<CurveConfiguration>"
"<EquityCurves>"
"<EquityCurve>"
"<CurveId>SP5Mini</CurveId>"
"<ForecastCurve>USD1D</ForecastCurve>"
"<CurveDescription>SP Mini equity price projection curve</CurveDescription>"
"<Currency>USD</Currency> <!--is this really needed ? -->"
"<Type>ForwardPrice</Type> <!-- {DividendYield, ForwardPrice} -->"
"<Quotes>"
"<Quote>EQUITY_FWD/PRICE/SP5Mini/USD/1M</Quote>"
"<Quote>EQUITY_FWD/PRICE/SP5Mini/USD/2016-09-15</Quote>"
"<Quote>EQUITY_FWD/PRICE/SP5Mini/USD/1Y</Quote>"
"<Quote>EQUITY_FWD/PRICE/SP5Mini/USD/2Y</Quote>"
"<Quote>EQUITY_FWD/PRICE/SP5Mini/USD/5Y</Quote>"
"</Quotes>"
"</EquityCurve>"
"</EquityCurves>"
"</CurveConfiguration>";

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(EquityMarketDataTests)

BOOST_AUTO_TEST_CASE(testMarketDatumParser) {

    BOOST_TEST_MESSAGE("Testing equity market data parsing...");
    std::vector<std::string> marketDataStrings = getMarketDataStrings();
    for (auto s : marketDataStrings) {
        std::vector<std::string> tokens;
        boost::trim(s);
        boost::split(tokens, s, boost::is_any_of(" "), boost::token_compress_on);
        Date quoteDate = ore::data::parseDate(tokens[0]);
        const string& key = tokens[1];
        Real value = ore::data::parseReal(tokens[2]);

        BOOST_CHECK_NO_THROW(ore::data::parseMarketDatum(quoteDate, key, value));
        QuantLib::ext::shared_ptr<ore::data::MarketDatum> md = ore::data::parseMarketDatum(quoteDate, key, value);
        BOOST_CHECK(md);
        BOOST_CHECK_EQUAL(md->name(), key);
        BOOST_CHECK_EQUAL(md->asofDate(), quoteDate);
        BOOST_CHECK_EQUAL(md->quote()->value(), value);
    }
}

BOOST_AUTO_TEST_CASE(testBadMarketDatumStrings) {

    BOOST_TEST_MESSAGE("Testing equity market data parsing (bad strings)...");
    std::vector<std::string> marketDataStrings = getBadMarketDataStrings();
    for (auto s : marketDataStrings) {
        std::vector<std::string> tokens;
        boost::trim(s);
        boost::split(tokens, s, boost::is_any_of(" "), boost::token_compress_on);
        Date quoteDate = ore::data::parseDate(tokens[0]);
        const string& key = tokens[1];
        Real value = ore::data::parseReal(tokens[2]);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(quoteDate, key, value), QuantLib::Error);
    }
}

BOOST_AUTO_TEST_CASE(testEqCurveConfigLoad) {

    BOOST_TEST_MESSAGE("Testing equity curve config load...");
    ore::data::XMLDocument testDoc;
    testDoc.fromXMLString(divYieldCurveConfigString);
    // check that the root node is as expected
    ore::data::XMLNode* node = testDoc.getFirstNode("CurveConfiguration");
    BOOST_CHECK_NO_THROW(ore::data::XMLUtils::checkNode(node, "CurveConfiguration"));

    ore::data::CurveConfigurations cc;
    BOOST_CHECK_NO_THROW(cc.fromXML(node));
    QuantLib::ext::shared_ptr<ore::data::EquityCurveConfig> ec = cc.equityCurveConfig("SP5");
    BOOST_CHECK(ec);
    BOOST_CHECK_EQUAL("SP5", ec->curveID());
    BOOST_CHECK_EQUAL("SP 500 equity price projection curve", 
        ec->curveDescription());
    BOOST_CHECK_EQUAL("USD", ec->currency());
    BOOST_CHECK_EQUAL("EQUITY/PRICE/SP5/USD", ec->equitySpotQuoteID());
    bool testType = (ec->type() == ore::data::EquityCurveConfig::Type::DividendYield);
    BOOST_CHECK(testType);
    //BOOST_CHECK_EQUAL(ore::data::EquityCurveConfig::Type::DividendYield, ec->type());
    BOOST_CHECK_EQUAL("A365", ec->dayCountID());
    vector<string> anticipatedQuotes = boost::assign::list_of
        ("EQUITY/PRICE/SP5/USD")
        ("EQUITY_DIVIDEND/RATE/SP5/USD/1M")
        ("EQUITY_DIVIDEND/RATE/SP5/USD/2016-09-15")
        ("EQUITY_DIVIDEND/RATE/SP5/USD/1Y")
        ("EQUITY_DIVIDEND/RATE/SP5/USD/2Y")
        ("EQUITY_DIVIDEND/RATE/SP5/USD/5Y");
    vector<string> actualQuotes = ec->quotes();
    BOOST_CHECK_EQUAL_COLLECTIONS(anticipatedQuotes.begin(), anticipatedQuotes.end(),
        actualQuotes.begin(), actualQuotes.end());
    BOOST_CHECK(!ec->extrapolation());

    // now test the toXML member function
    ore::data::XMLDocument testDumpDoc;
    BOOST_CHECK_NO_THROW(cc.toXML(testDumpDoc));
    // TODO - query the XML to ensure that the curve configuration objects have been dumped correctly
}

BOOST_AUTO_TEST_CASE(testEqCurveConfigBadLoad) {

    BOOST_TEST_MESSAGE("Testing equity curve config load (bad input)...");
    ore::data::XMLDocument testBadDoc;
    testBadDoc.fromXMLString(eqBadConfigString);
    // check that the root node is as expected
    ore::data::XMLNode* badNode = testBadDoc.getFirstNode("CurveConfiguration");
    BOOST_CHECK_NO_THROW(ore::data::XMLUtils::checkNode(badNode, "CurveConfiguration"));
    ore::data::CurveConfigurations cc;    
    BOOST_CHECK_NO_THROW(cc.fromXML(badNode)); // the spot price is missing, but the correct behaviour is to log error and move on
    BOOST_CHECK_THROW(cc.equityCurveConfig("SP5Mini"), QuantLib::Error); // this checks that the XML throws when we try to load
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
