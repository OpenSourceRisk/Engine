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

#include "equitymarketdata.hpp"
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>

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
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/zzz/ATMF 0.25") // invalid tenor/date input
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/365D/ATM 0.25") // strike should be ATM-forward as opposed to "ATM"
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/1678W5D/37.75 0.25") // explicit strike axis not supported yet
        ("20160226 EQUITY_OPTION/RATE_LNVOL/Lufthansa/EUR/1Y1M/-0.05 0.13"); // sticky-delta strike axis not supported yet
}
}

namespace testsuite {

void EquityMarketDataTest::testMarketDatumParser() {

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
        boost::shared_ptr<ore::data::MarketDatum> md = ore::data::parseMarketDatum(quoteDate, key, value);
        BOOST_CHECK(md);
        BOOST_CHECK_EQUAL(md->name(), key);
        BOOST_CHECK_EQUAL(md->asofDate(), quoteDate);
        BOOST_CHECK_EQUAL(md->quote()->value(), value);
    }
}

void EquityMarketDataTest::testBadMarketDatumStrings() {

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

boost::unit_test_framework::test_suite* EquityMarketDataTest::suite() {
    boost::unit_test_framework::test_suite* suite = BOOST_TEST_SUITE("EquityMarketDataTest");

    suite->add(BOOST_TEST_CASE(&EquityMarketDataTest::testMarketDatumParser));
    suite->add(BOOST_TEST_CASE(&EquityMarketDataTest::testBadMarketDatumStrings));
    return suite;
}
}
