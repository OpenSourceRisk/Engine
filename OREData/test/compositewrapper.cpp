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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/builders/fxdoublebarrieroption.hpp>
#include <ored/portfolio/builders/fxdoubletouchoption.hpp>
#include <ored/portfolio/builders/fxtouchoption.hpp>
#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/portfolio/fxdoublebarrieroption.hpp>
#include <ored/portfolio/fxdoubletouchoption.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

BOOST_FIXTURE_TEST_SUITE(OREPlusEquityFXTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CompositeWrapperTest)

namespace {
template <class T> void loadFromXMLString(T& t, const std::string& str) {
    ore::data::XMLDocument doc;
    doc.fromXMLString(str);
    t.fromXML(doc.getFirstNode(""));
}

struct CommonVars {

    CommonVars()
        : asof(Date(5, Feb, 2016)), baseCurrency("EUR"),
          portfolio(XMLDocument(TEST_INPUT_FILE("portfolio.xml")).toString()),
          conventions(XMLDocument(TEST_INPUT_FILE("conventions.xml")).toString()),
          todaysMarketConfig(XMLDocument(TEST_INPUT_FILE("todaysmarket.xml")).toString()),
          pricingEngineConfig(XMLDocument(TEST_INPUT_FILE("pricingengine.xml")).toString()),
          curveConfig(XMLDocument(TEST_INPUT_FILE("curveconfig.xml")).toString()),
          loader(QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE("market.csv"), TEST_INPUT_FILE("fixings.csv"), "")) {

        Settings::instance().evaluationDate() = asof;
    }

    Date asof;
    string baseCurrency;
    string portfolio;
    string conventions;
    string todaysMarketConfig;
    string pricingEngineConfig;
    string curveConfig;
    QuantLib::ext::shared_ptr<Loader> loader;
    SavedSettings savedSettings;
};

} // namespace

// Common variables used in the tests below

BOOST_AUTO_TEST_CASE(testCompositeInstrumentWrapperPrice) {
    CommonVars vars;
    QuantLib::ext::shared_ptr<CurveConfigurations> curveConfig = QuantLib::ext::make_shared<CurveConfigurations>();
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
    QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketConfig = QuantLib::ext::make_shared<TodaysMarketParameters>();
    QuantLib::ext::shared_ptr<EngineData> pricingEngineConfig = QuantLib::ext::make_shared<EngineData>();
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();

    loadFromXMLString(*curveConfig, vars.curveConfig);
    loadFromXMLString(*conventions, vars.conventions);
    InstrumentConventions::instance().setConventions(conventions);

    loadFromXMLString(*todaysMarketConfig, vars.todaysMarketConfig);
    loadFromXMLString(*pricingEngineConfig, vars.pricingEngineConfig);

    portfolio->fromXMLString(vars.portfolio);

    QuantLib::ext::shared_ptr<Market> market =
        QuantLib::ext::make_shared<TodaysMarket>(vars.asof, todaysMarketConfig, vars.loader, curveConfig, true);
    auto configurations = std::map<MarketContext, string>();
    QuantLib::ext::shared_ptr<EngineFactory> factory =
        QuantLib::ext::make_shared<EngineFactory>(pricingEngineConfig, market, configurations);

    BOOST_TEST_MESSAGE("number trades " << portfolio->size());
    portfolio->build(factory);
    BOOST_TEST_MESSAGE("number trades " << portfolio->size());

    Handle<Quote> fx = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    vector<QuantLib::ext::shared_ptr<InstrumentWrapper>> iw;
    std::vector<Handle<Quote>> fxRates;

    Real totalNPV = 0;
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        Handle<Quote> fx = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
        if (trade->npvCurrency() != "USD")
            fx = factory->market()->fxRate(trade->npvCurrency() + "USD");
        fxRates.push_back(fx);
        iw.push_back(trade->instrument());
        BOOST_TEST_MESSAGE("NPV " << iw.back()->NPV());
        BOOST_TEST_MESSAGE("FX " << fxRates.back()->value());

        totalNPV += iw.back()->NPV() * fxRates.back()->value();
    }
    QuantLib::ext::shared_ptr<InstrumentWrapper> instrument =
        QuantLib::ext::shared_ptr<InstrumentWrapper>(new ore::data::CompositeInstrumentWrapper(iw, fxRates, vars.asof));

    BOOST_TEST_MESSAGE(instrument->NPV());

    BOOST_CHECK_CLOSE(instrument->NPV(), totalNPV, 0.01);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
