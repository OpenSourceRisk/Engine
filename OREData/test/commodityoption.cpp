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

#include <ql/currencies/america.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

#include <qle/termstructures/pricecurve.hpp>

#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/portfolio.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace boost::algorithm;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace {

// testTolerance for Real comparison
Real testTolerance = 1e-10;

// Cached prices of call and put
// Satisfies put call parity: call - put = (1348 - 1340) * 0.990049833749 * 100
Real cachedCallPrice = 5711.6321329012244;
Real cachedPutPrice = 4919.5922659018906;

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        // Reference date and common day counter
        asof_ = Date(19, Feb, 2018);
        Actual365Fixed dayCounter;

        // Add USD discount curve
        Handle<YieldTermStructure> discount(boost::make_shared<FlatForward>(asof_, 0.01, dayCounter));
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = discount;

        // Add GOLD_USD price curve
        vector<Date> dates = {asof_, Date(19, Feb, 2019)};
        vector<Real> prices = {1346.0, 1348.0};
        Handle<PriceTermStructure> priceCurve(
            boost::make_shared<InterpolatedPriceCurve<Linear>>(asof_, dates, prices, dayCounter, USDCurrency()));
        Handle<CommodityIndex> commIdx(boost::make_shared<CommoditySpotIndex>("GOLD_USD", NullCalendar(), priceCurve));
        commodityIndices_[make_pair(Market::defaultConfiguration, "GOLD_USD")] = commIdx;

        // Add GOLD_USD volatilities
        vector<Date> volatilityDates = {Date(19, Feb, 2019), Date(19, Feb, 2020)};
        vector<Real> volatilities = {0.10, 0.11};
        Handle<BlackVolTermStructure> volatility(
            boost::make_shared<BlackVarianceCurve>(asof_, volatilityDates, volatilities, dayCounter));
        commodityVols_[make_pair(Market::defaultConfiguration, "GOLD_USD")] = volatility;
    }
};

class CommonData {
public:
    Envelope envelope;
    string commodityName;
    string currency;
    Real quantity;
    TradeStrike strike;
    bool payOffAtExpiry;
    vector<string> expiry;
    Date expiryDate;

    boost::shared_ptr<Market> market;
    boost::shared_ptr<EngineFactory> engineFactory;

    CommonData()
        : commodityName("GOLD_USD"), currency("USD"), quantity(100), strike(1340.0, "USD"), payOffAtExpiry(false),
          expiry(1, "2019-02-19"), expiryDate(19, Feb, 2019) {

        // Create engine factory
        market = boost::make_shared<TestMarket>();
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        engineData->model("CommodityOption") = "BlackScholes";
        engineData->engine("CommodityOption") = "AnalyticEuropeanEngine";
        engineFactory = boost::make_shared<EngineFactory>(engineData, market);

        // Set evaluation date
        Settings::instance().evaluationDate() = market->asofDate();
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityOptionTests)

BOOST_AUTO_TEST_CASE(testCommodityOptionTradeBuilding) {

    BOOST_TEST_MESSAGE("Testing commodity option trade building");

    // Common test data and setup
    CommonData td;

    // Test the building of a commodity option doesn't throw
    OptionData optionData("Long", "Call", "European", td.payOffAtExpiry, td.expiry);
    boost::shared_ptr<CommodityOption> option = boost::make_shared<CommodityOption>(
        td.envelope, optionData, td.commodityName, td.currency, td.quantity, td.strike);
    BOOST_CHECK_NO_THROW(option->build(td.engineFactory));

    // Check the underlying instrument was built as expected
    boost::shared_ptr<Instrument> qlInstrument = option->instrument()->qlInstrument();

    boost::shared_ptr<VanillaOption> vanillaOption = boost::dynamic_pointer_cast<VanillaOption>(qlInstrument);
    BOOST_CHECK(vanillaOption);
    BOOST_CHECK_EQUAL(vanillaOption->exercise()->type(), Exercise::Type::European);
    BOOST_CHECK_EQUAL(vanillaOption->exercise()->dates().size(), 1);
    BOOST_CHECK_EQUAL(vanillaOption->exercise()->dates()[0], td.expiryDate);

    boost::shared_ptr<TypePayoff> payoff = boost::dynamic_pointer_cast<TypePayoff>(vanillaOption->payoff());
    BOOST_CHECK(payoff);
    BOOST_CHECK_EQUAL(payoff->optionType(), Option::Type::Call);

    // Calculate the expected price and check against cached price
    // This is an extra check of the market etc.
    // Know it is then safe to use the cached price elsewhere in this suite
    Real forwardPrice = td.market->commodityPriceCurve(td.commodityName)->price(td.expiryDate);
    DiscountFactor discount = td.market->discountCurve(td.currency)->discount(td.expiryDate);
    Real variance = td.market->commodityVolatility(td.commodityName)->blackVariance(td.expiryDate, td.strike.value());
    Real expectedPrice =
        td.quantity * blackFormula(Option::Type::Call, td.strike.value(), forwardPrice, sqrt(variance), discount);
    BOOST_CHECK_CLOSE(expectedPrice, cachedCallPrice, testTolerance);

    // Check the price
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), expectedPrice, testTolerance);
}

BOOST_AUTO_TEST_CASE(testCommodityOptionFromXml) {

    BOOST_TEST_MESSAGE("Testing parsing of commodity option trade from XML");

    // Common test data and setup
    CommonData td;

    // Create an XML string representation of the trade
    string tradeXml;
    tradeXml.append("<Portfolio>");
    tradeXml.append("  <Trade id=\"CommodityOption_Gold\">");
    tradeXml.append("    <TradeType>CommodityOption</TradeType>");
    tradeXml.append("    <Envelope>");
    tradeXml.append("      <CounterParty>CPTY_A</CounterParty>");
    tradeXml.append("      <NettingSetId>CPTY_A</NettingSetId>");
    tradeXml.append("      <AdditionalFields/>");
    tradeXml.append("    </Envelope>");
    tradeXml.append("    <CommodityOptionData>");
    tradeXml.append("      <OptionData>");
    tradeXml.append("        <LongShort>Long</LongShort>");
    tradeXml.append("        <OptionType>Call</OptionType>");
    tradeXml.append("        <Style>European</Style>");
    tradeXml.append("        <Settlement>Cash</Settlement>");
    tradeXml.append("        <PayOffAtExpiry>false</PayOffAtExpiry>");
    tradeXml.append("        <ExerciseDates>");
    tradeXml.append("          <ExerciseDate>2019-02-19</ExerciseDate>");
    tradeXml.append("        </ExerciseDates>");
    tradeXml.append("      </OptionData>");
    tradeXml.append("      <Name>GOLD_USD</Name>");
    tradeXml.append("      <Currency>USD</Currency>");
    tradeXml.append("      <Strike>1340</Strike>");
    tradeXml.append("      <Quantity>100</Quantity>");
    tradeXml.append("    </CommodityOptionData>");
    tradeXml.append("  </Trade>");
    tradeXml.append("</Portfolio>");

    // Load portfolio from XML string
    Portfolio portfolio;
    portfolio.loadFromXMLString(tradeXml);

    // Extract CommodityOption trade from portfolio
    boost::shared_ptr<Trade> trade = portfolio.trades()[0];
    boost::shared_ptr<CommodityOption> option = boost::dynamic_pointer_cast<ore::data::CommodityOption>(trade);

    // Check fields after checking that the cast was successful
    BOOST_CHECK(option);
    BOOST_CHECK_EQUAL(option->tradeType(), "CommodityOption");
    BOOST_CHECK_EQUAL(option->id(), "CommodityOption_Gold");
    BOOST_CHECK_EQUAL(option->asset(), "GOLD_USD");
    BOOST_CHECK_EQUAL(option->currency(), "USD");
    BOOST_CHECK_CLOSE(option->strike().value(), 1340, testTolerance);
    BOOST_CHECK_CLOSE(option->quantity(), 100, testTolerance);
    BOOST_CHECK_EQUAL(option->option().longShort(), "Long");
    BOOST_CHECK_EQUAL(option->option().callPut(), "Call");
    BOOST_CHECK_EQUAL(option->option().style(), "European");
    BOOST_CHECK_EQUAL(option->option().exerciseDates().size(), 1);
    BOOST_CHECK_EQUAL(option->option().exerciseDates()[0], "2019-02-19");

    // Build the option and check the price
    BOOST_CHECK_NO_THROW(option->build(td.engineFactory));
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), cachedCallPrice, testTolerance);

    // Make trade short call and test price is negated
    replace_all(tradeXml, ">Long<", ">Short<");
    portfolio.clear();
    portfolio.loadFromXMLString(tradeXml);
    trade = portfolio.trades()[0];
    BOOST_CHECK_NO_THROW(trade->build(td.engineFactory));
    BOOST_CHECK_CLOSE(trade->instrument()->NPV(), -cachedCallPrice, testTolerance);

    // Make trade short put and test price
    replace_all(tradeXml, ">Call<", ">Put<");
    portfolio.clear();
    portfolio.loadFromXMLString(tradeXml);
    trade = portfolio.trades()[0];
    BOOST_CHECK_NO_THROW(trade->build(td.engineFactory));
    BOOST_CHECK_CLOSE(trade->instrument()->NPV(), -cachedPutPrice, testTolerance);

    // Make trade long put and test price
    replace_all(tradeXml, ">Short<", ">Long<");
    portfolio.clear();
    portfolio.loadFromXMLString(tradeXml);
    trade = portfolio.trades()[0];
    BOOST_CHECK_NO_THROW(trade->build(td.engineFactory));
    BOOST_CHECK_CLOSE(trade->instrument()->NPV(), cachedPutPrice, testTolerance);
}

BOOST_AUTO_TEST_CASE(testLongShortCallPutPrices) {

    BOOST_TEST_MESSAGE("Testing commodity option prices");

    // Common test data and setup
    CommonData td;

    // Option
    boost::shared_ptr<CommodityOption> option;

    // Long call
    OptionData optionData("Long", "Call", "European", td.payOffAtExpiry, td.expiry);
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency,
                                                 td.quantity, td.strike);
    option->build(td.engineFactory);
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), cachedCallPrice, testTolerance);

    // Short call
    optionData = OptionData("Short", "Call", "European", td.payOffAtExpiry, td.expiry);
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency,
                                                 td.quantity, td.strike);
    option->build(td.engineFactory);
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), -cachedCallPrice, testTolerance);

    // Long put
    optionData = OptionData("Long", "Put", "European", td.payOffAtExpiry, td.expiry);
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency,
                                                 td.quantity, td.strike);
    option->build(td.engineFactory);
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), cachedPutPrice, testTolerance);

    // Short put
    optionData = OptionData("Short", "Put", "European", td.payOffAtExpiry, td.expiry);
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency, td.quantity,
                                                 td.strike);
    option->build(td.engineFactory);
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), -cachedPutPrice, testTolerance);
}

BOOST_AUTO_TEST_CASE(testCommodityOptionBuildExceptions) {

    BOOST_TEST_MESSAGE("Testing commodity option exceptions during building");

    // Common test data and setup
    CommonData td;

    // Option
    boost::shared_ptr<CommodityOption> option;

    // Negative strike throws
    OptionData optionData("Long", "Call", "European", td.payOffAtExpiry, td.expiry);
    TradeStrike ts(TradeStrike::Type::Price, -td.strike.value());
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency, td.quantity, ts);
    BOOST_CHECK_THROW(option->build(td.engineFactory), Error);

    // Negative quantity throws
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency, -td.quantity,
                                                 td.strike);
    BOOST_CHECK_THROW(option->build(td.engineFactory), Error);

    // Name of commodity with no market data throws
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, "GOLD_USD_MISSING", td.currency,
                                                 td.quantity, td.strike);
    BOOST_CHECK_THROW(option->build(td.engineFactory), Error);

    // Non-European OptionData style throws
    optionData = OptionData("Long", "Call", "American", td.payOffAtExpiry, td.expiry);
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency, td.quantity,
                                                 td.strike);
    BOOST_CHECK_THROW(option->build(td.engineFactory), Error);

    // More than one expiry date throws
    vector<string> extraExpiries = td.expiry;
    extraExpiries.push_back("2019-08-19");
    optionData = OptionData("Long", "Call", "European", td.payOffAtExpiry, extraExpiries);
    option = boost::make_shared<CommodityOption>(td.envelope, optionData, td.commodityName, td.currency,
                                                 td.quantity, td.strike);
    BOOST_CHECK_THROW(option->build(td.engineFactory), Error);
}

BOOST_AUTO_TEST_CASE(testCommodityOptionPremium) {

    BOOST_TEST_MESSAGE("Testing commodity option premium works");

    // Common test data and setup
    CommonData td;

    // Premium
    Real premium = 5000;
    Date premiumDate(21, Feb, 2018);

    // Create option
    OptionData optionData("Long", "Call", "European", td.payOffAtExpiry, td.expiry, "Cash", "",
                          PremiumData{premium, td.currency, Date(21, Feb, 2018)});
    boost::shared_ptr<CommodityOption> option = boost::make_shared<CommodityOption>(
        td.envelope, optionData, td.commodityName, td.currency, td.quantity, td.strike);

    // Test building succeeds
    BOOST_CHECK_NO_THROW(option->build(td.engineFactory));

    // Test that price is correct
    DiscountFactor premiumDiscount = td.market->discountCurve(td.currency)->discount(premiumDate);
    BOOST_CHECK_CLOSE(option->instrument()->NPV(), cachedCallPrice - premiumDiscount * premium, testTolerance);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
