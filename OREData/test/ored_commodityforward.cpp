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

#include <boost/make_shared.hpp>

#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

#include <qle/termstructures/pricecurve.hpp>

#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/portfolio.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace {

// testTolerance for Real comparison
Real testTolerance = 1e-10;

class TestMarket : public MarketImpl {
public:
    TestMarket() {
        // Reference date and common day counter
        asof_ = Date(19, Feb, 2018);
        Actual365Fixed dayCounter;

        // Add USD discount curve, discount = 1.0 everywhere
        Handle<YieldTermStructure> discount(boost::make_shared<FlatForward>(asof_, 0.0, dayCounter));
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = discount;

        // Add GOLD_USD price curve
        vector<Date> dates = {asof_, Date(19, Feb, 2019)};
        vector<Real> prices = {1346.0, 1348.0};
        Handle<PriceTermStructure> priceCurve(
            boost::make_shared<InterpolatedPriceCurve<Linear>>(asof_, dates, prices, dayCounter, USDCurrency()));
        commodityCurves_[make_pair(Market::defaultConfiguration, "GOLD_USD")] = priceCurve;
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityForwardTests)

BOOST_AUTO_TEST_CASE(testCommodityForwardTradeBuilding) {

    BOOST_TEST_MESSAGE("Testing commodity forward trade building");

    // Create market
    boost::shared_ptr<Market> market = boost::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    // Create engine factory
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("CommodityForward") = "DiscountedCashflows";
    engineData->engine("CommodityForward") = "DiscountingCommodityForwardEngine";
    boost::shared_ptr<EngineFactory> engineFactory = boost::make_shared<EngineFactory>(engineData, market);

    // Base commodity values
    string position = "Long";
    string commodityName = "GOLD_USD";
    string currency = "USD";
    Real quantity = 100.0;
    string maturity = "2019-02-19";
    Real strike = 1340.0;

    // Test the building of a commodity forward doesn't throw
    Envelope envelope;
    boost::shared_ptr<ore::data::CommodityForward> forward = boost::make_shared<ore::data::CommodityForward>(
        envelope, position, commodityName, currency, quantity, maturity, strike);
    BOOST_CHECK_NO_THROW(forward->build(engineFactory));

    // Check the instrument was built as expected
    boost::shared_ptr<Instrument> qlInstrument = forward->instrument()->qlInstrument();
    boost::shared_ptr<QuantExt::CommodityForward> commodityForward =
        boost::dynamic_pointer_cast<QuantExt::CommodityForward>(qlInstrument);
    BOOST_CHECK(commodityForward);
    BOOST_CHECK_EQUAL(commodityForward->position(), Position::Type::Long);
    BOOST_CHECK_EQUAL(commodityForward->name(), "GOLD_USD");
    BOOST_CHECK_EQUAL(commodityForward->currency(), USDCurrency());
    BOOST_CHECK_CLOSE(commodityForward->quantity(), 100.0, testTolerance);
    BOOST_CHECK_EQUAL(commodityForward->maturityDate(), Date(19, Feb, 2019));
    BOOST_CHECK_CLOSE(commodityForward->strike(), 1340.0, testTolerance);

    // Check the price (simple because DF = 1.0, 100 * (1348 - 1340))
    BOOST_CHECK_CLOSE(commodityForward->NPV(), 800.0, testTolerance);

    // Check short
    forward = boost::make_shared<ore::data::CommodityForward>(envelope, "Short", commodityName, currency, quantity,
                                                              maturity, strike);
    BOOST_CHECK_NO_THROW(forward->build(engineFactory));
    qlInstrument = forward->instrument()->qlInstrument();
    commodityForward = boost::dynamic_pointer_cast<QuantExt::CommodityForward>(qlInstrument);
    BOOST_CHECK(commodityForward);
    BOOST_CHECK_EQUAL(commodityForward->position(), Position::Type::Short);
    BOOST_CHECK_CLOSE(commodityForward->NPV(), -800.0, testTolerance);

    // Check that negative quantity throws an error
    forward = boost::make_shared<ore::data::CommodityForward>(envelope, position, commodityName, currency, -quantity,
                                                              maturity, strike);
    BOOST_CHECK_THROW(forward->build(engineFactory), Error);

    // Check that negative strike throws an error
    forward = boost::make_shared<ore::data::CommodityForward>(envelope, position, commodityName, currency, quantity,
                                                              maturity, -strike);
    BOOST_CHECK_THROW(forward->build(engineFactory), Error);

    // Check that build fails when commodity name does not match that in the market
    forward = boost::make_shared<ore::data::CommodityForward>(envelope, position, "GOLD", currency, quantity, maturity,
                                                              strike);
    BOOST_CHECK_THROW(forward->build(engineFactory), Error);
}

BOOST_AUTO_TEST_CASE(testCommodityForwardFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of commodity forward trade from XML");

    // Create an XML string representation of the trade
    string tradeXml;
    tradeXml.append("<Portfolio>");
    tradeXml.append("  <Trade id=\"CommodityForward_WTI_Oct_21\">");
    tradeXml.append("  <TradeType>CommodityForward</TradeType>");
    tradeXml.append("  <Envelope>");
    tradeXml.append("    <CounterParty>CPTY_A</CounterParty>");
    tradeXml.append("    <NettingSetId>CPTY_A</NettingSetId>");
    tradeXml.append("    <AdditionalFields/>");
    tradeXml.append("  </Envelope>");
    tradeXml.append("  <CommodityForwardData>");
    tradeXml.append("    <Position>Short</Position>");
    tradeXml.append("    <Maturity>2021-10-31</Maturity>");
    tradeXml.append("    <Name>COMDTY_WTI_USD</Name>");
    tradeXml.append("    <Currency>USD</Currency>");
    tradeXml.append("    <Strike>49.75</Strike>");
    tradeXml.append("    <Quantity>500000</Quantity>");
    tradeXml.append("  </CommodityForwardData>");
    tradeXml.append("  </Trade>");
    tradeXml.append("</Portfolio>");

    // Load portfolio from XML string
    Portfolio portfolio;
    portfolio.loadFromXMLString(tradeXml);

    // Extract CommodityForward trade from portfolio
    boost::shared_ptr<Trade> trade = portfolio.trades()[0];
    boost::shared_ptr<ore::data::CommodityForward> commodityForward =
        boost::dynamic_pointer_cast<ore::data::CommodityForward>(trade);

    // Check fields after checking that the cast was successful
    BOOST_CHECK(commodityForward);
    BOOST_CHECK_EQUAL(commodityForward->tradeType(), "CommodityForward");
    BOOST_CHECK_EQUAL(commodityForward->id(), "CommodityForward_WTI_Oct_21");
    BOOST_CHECK_EQUAL(commodityForward->position(), "Short");
    BOOST_CHECK_EQUAL(commodityForward->maturityDate(), "2021-10-31");
    BOOST_CHECK_EQUAL(commodityForward->commodityName(), "COMDTY_WTI_USD");
    BOOST_CHECK_EQUAL(commodityForward->currency(), "USD");
    BOOST_CHECK_CLOSE(commodityForward->strike(), 49.75, testTolerance);
    BOOST_CHECK_CLOSE(commodityForward->quantity(), 500000.0, testTolerance);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
