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

#include <test/scenariosimmarket.hpp>
#include <test/testmarket.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/capfloor/constantcapfloortermvol.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/configuration/conventions.hpp>

#include <ql/indexes/ibor/all.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;

namespace {

boost::shared_ptr<data::Conventions> convs() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    boost::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    return conventions;
}

boost::shared_ptr<analytics::ScenarioSimMarketParameters> scenarioParameters() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> parameters(new analytics::ScenarioSimMarketParameters());
    parameters->baseCcy() = "EUR";
    parameters->ccys() = {"EUR", "USD"};
    parameters->yieldCurveTenors() = {6 * Months, 1 * Years, 2 * Years};
    parameters->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-6M"};
    parameters->interpolation() = "LogLinear";
    parameters->extrapolate() = true;

    parameters->swapVolTerms() = {6 * Months, 1 * Years};
    parameters->swapVolExpiries() = {1 * Years, 2 * Years};
    parameters->swapVolCcys() = {"EUR", "USD"};
    parameters->swapVolDecayMode() = "ForwardVariance";

    parameters->defaultNames() = {"dc2"};
    parameters->defaultTenors() = {6 * Months, 8 * Months, 1 * Years, 2 * Years};

    parameters->simulateFXVols() = false;
    parameters->fxVolExpiries() = {2 * Years, 3 * Years, 4 * Years};
    parameters->fxVolDecayMode() = "ConstantVariance";
    parameters->simulateEQVols() = false;

    parameters->ccyPairs() = {"EURUSD"};

    return parameters;
}
}

namespace testsuite {

void testFxSpot(boost::shared_ptr<ore::data::Market>& initMarket,
                boost::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                boost::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {

    for (const auto& ccy : parameters->ccys()) {
        if (ccy != parameters->baseCcy()) {
            string ccyPair(parameters->baseCcy() + ccy);
            string ccyReverse(ccy + parameters->baseCcy());

            Handle<Quote> fx_sim = simMarket->fxSpot(ccyPair);
            Handle<Quote> fx_init = initMarket->fxSpot(ccyPair);
            if (fx_sim.empty())
                BOOST_FAIL("fx_sim handle is empty");
            if (fx_init.empty())
                BOOST_FAIL("fx_init handle is empty");

            BOOST_CHECK_CLOSE(fx_init->value(), fx_sim->value(), 1e-12);

            fx_sim = simMarket->fxSpot(ccyReverse);
            fx_init = initMarket->fxSpot(ccyReverse);
            if (fx_sim.empty())
                BOOST_FAIL("fx_sim handle is empty");
            if (fx_init.empty())
                BOOST_FAIL("fx_init handle is empty");

            BOOST_CHECK_CLOSE(fx_init->value(), fx_sim->value(), 1e-12);
        }
    }
}

void testDiscountCurve(boost::shared_ptr<ore::data::Market>& initMarket,
                       boost::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                       boost::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {

    for (const auto& ccy : parameters->ccys()) {
        Handle<YieldTermStructure> simCurve = simMarket->discountCurve(ccy);
        Handle<YieldTermStructure> initCurve = initMarket->discountCurve(ccy);

        BOOST_CHECK_CLOSE(simCurve->discount(0.5), initCurve->discount(0.5), 1e-12);
    }
}

void testIndexCurve(boost::shared_ptr<ore::data::Market>& initMarket,
                    boost::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                    boost::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {

    for (const auto& ind : parameters->indices()) {
        Handle<YieldTermStructure> simCurve = simMarket->iborIndex(ind)->forwardingTermStructure();
        Handle<YieldTermStructure> initCurve = initMarket->iborIndex(ind)->forwardingTermStructure();

        BOOST_CHECK_CLOSE(simCurve->discount(1), initCurve->discount(1), 1e-4);
    }
}

void testSwaptionVolCurve(boost::shared_ptr<ore::data::Market>& initMarket,
                          boost::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                          boost::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& ccy : parameters->ccys()) {
        Handle<QuantLib::SwaptionVolatilityStructure> simCurve = simMarket->swaptionVol(ccy);
        Handle<QuantLib::SwaptionVolatilityStructure> initCurve = initMarket->swaptionVol(ccy);
        for (const auto& maturity : parameters->swapVolExpiries()) {
            for (const auto& tenor : parameters->swapVolTerms()) {
                BOOST_CHECK_CLOSE(simCurve->volatility(maturity, tenor, 0.0, true),
                                  initCurve->volatility(maturity, tenor, 0.0, true), 1e-12);
            }
        }
    }
}

void testFxVolCurve(boost::shared_ptr<data::Market>& initMarket,
                    boost::shared_ptr<analytics::ScenarioSimMarket>& simMarket,
                    boost::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& ccyPair : parameters->ccyPairs()) {
        Handle<BlackVolTermStructure> simCurve = simMarket->fxVol(ccyPair);
        Handle<BlackVolTermStructure> initCurve = initMarket->fxVol(ccyPair);
        vector<Date> dates;
        Date asof = initMarket->asofDate();
        for (Size i = 0; i < parameters->fxVolExpiries().size(); i++) {
            dates.push_back(asof + parameters->fxVolExpiries()[i]);
        }

        for (const auto& date : dates) {
            BOOST_CHECK_CLOSE(simCurve->blackVol(date, 0.0, true), initCurve->blackVol(date, 0.0, true), 1e-12);
        }
    }
}

void testDefaultCurve(boost::shared_ptr<ore::data::Market>& initMarket,
                      boost::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                      boost::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& spec : parameters->defaultNames()) {

        Handle<DefaultProbabilityTermStructure> simCurve = simMarket->defaultCurve(spec);
        Handle<DefaultProbabilityTermStructure> initCurve = initMarket->defaultCurve(spec);
        BOOST_CHECK_EQUAL(initCurve->referenceDate(), simCurve->referenceDate());
        vector<Date> dates;
        Date asof = initMarket->asofDate();
        for (Size i = 0; i < parameters->defaultTenors().size(); i++) {
            dates.push_back(asof + parameters->defaultTenors()[i]);
        }

        for (const auto& date : dates) {
            BOOST_CHECK_CLOSE(simCurve->survivalProbability(date, true), initCurve->survivalProbability(date, true),
                              1e-12);
        }
    }
}

void testToXML(boost::shared_ptr<analytics::ScenarioSimMarketParameters> params) {

    BOOST_TEST_MESSAGE("Testing to XML...");
    XMLDocument outDoc;
    XMLDocument inDoc;
    string testFile = "simtest.xml";
    XMLNode* node = params->toXML(outDoc);

    XMLNode* simulationNode = outDoc.allocNode("Simulation");
    outDoc.appendNode(simulationNode);
    XMLUtils::appendNode(simulationNode, node);
    outDoc.toFile(testFile);

    boost::shared_ptr<analytics::ScenarioSimMarketParameters> newParams(new analytics::ScenarioSimMarketParameters());
    newParams->fromFile(testFile);
    BOOST_CHECK(*params == *newParams);

    newParams->baseCcy() = "JPY";
    BOOST_CHECK(*params != *newParams);
}

void ScenarioSimMarketTest::testScenarioSimMarket() {
    BOOST_TEST_MESSAGE("Testing Wrap ScenarioSimMarket...");

    Date tmp = Settings::instance().evaluationDate(); // archive original value
    Date today(20, Jan, 2015);
    Settings::instance().evaluationDate() = today;
    boost::shared_ptr<ore::data::Market> initMarket = boost::make_shared<TestMarket>(today);

    // Empty scenario generator
    boost::shared_ptr<ore::analytics::ScenarioGenerator> scenarioGenerator;

    // build scenario
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> parameters = scenarioParameters();
    Conventions conventions = *convs();
    // build scenario sim market
    boost::shared_ptr<analytics::ScenarioSimMarket> simMarket(
        new analytics::ScenarioSimMarket(scenarioGenerator, initMarket, parameters, conventions));

    // test
    testFxSpot(initMarket, simMarket, parameters);
    testDiscountCurve(initMarket, simMarket, parameters);
    testIndexCurve(initMarket, simMarket, parameters);
    testSwaptionVolCurve(initMarket, simMarket, parameters);
    testFxVolCurve(initMarket, simMarket, parameters);
    testDefaultCurve(initMarket, simMarket, parameters);
    testToXML(parameters);

    Settings::instance().evaluationDate() = tmp; // reset to original value
}

test_suite* ScenarioSimMarketTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("ScenarioSimMarketTests");
    suite->add(BOOST_TEST_CASE(&ScenarioSimMarketTest::testScenarioSimMarket));
    return suite;
}
}
