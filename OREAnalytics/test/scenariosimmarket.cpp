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
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/log.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/capfloor/constantcapfloortermvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <test/oreatoplevelfixture.hpp>

#include "testmarket.hpp"

#include <ql/indexes/ibor/all.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;
using namespace ore::data;

using testsuite::TestMarket;

namespace {

QuantLib::ext::shared_ptr<data::Conventions> convs() {
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    conventions->clear();
    
    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    QuantLib::ext::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    return conventions;
}

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> scenarioParameters() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> parameters(new analytics::ScenarioSimMarketParameters());
    parameters->baseCcy() = "EUR";
    parameters->setDiscountCurveNames({"EUR", "USD"});
    parameters->setYieldCurveTenors("", {6 * Months, 1 * Years, 2 * Years});
    parameters->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-6M"});
    parameters->interpolation() = "LogLinear";
    parameters->extrapolation() = "FlatFwd";

    parameters->setSwapVolTerms("", {6 * Months, 1 * Years});
    parameters->setSwapVolExpiries("", {1 * Years, 2 * Years});
    parameters->setSwapVolKeys({"EUR", "USD"});
    parameters->swapVolDecayMode() = "ForwardVariance";

    parameters->setDefaultNames({"dc2"});
    parameters->setDefaultTenors("", {6 * Months, 8 * Months, 1 * Years, 2 * Years});

    parameters->setSimulateFXVols(false);
    parameters->setFxVolExpiries("", vector<Period>{2 * Years, 3 * Years, 4 * Years});
    parameters->setFxVolDecayMode(string("ConstantVariance"));
    parameters->setSimulateEquityVols(false);

    parameters->setFxVolCcyPairs({"USDEUR"});

    parameters->setFxCcyPairs({"USDEUR"});

    parameters->setZeroInflationIndices({"EUHICPXT"});
    parameters->setZeroInflationTenors("", {6 * Months, 1 * Years, 2 * Years});

    parameters->setSimulateCorrelations(false);
    parameters->correlationExpiries() = {1 * Years, 2 * Years};
    parameters->setCorrelationPairs({"EUR-CMS-10Y:EUR-CMS-1Y", "USD-CMS-10Y:USD-CMS-1Y"});
    return parameters;
}
} // namespace

void testFxSpot(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {

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

void testDiscountCurve(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                       QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                       QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {

    for (const auto& ccy : parameters->ccys()) {
        Handle<YieldTermStructure> simCurve = simMarket->discountCurve(ccy);
        Handle<YieldTermStructure> initCurve = initMarket->discountCurve(ccy);

        BOOST_CHECK_CLOSE(simCurve->discount(0.5), initCurve->discount(0.5), 1e-12);
    }
}

void testIndexCurve(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {

    for (const auto& ind : parameters->indices()) {
        Handle<YieldTermStructure> simCurve = simMarket->iborIndex(ind)->forwardingTermStructure();
        Handle<YieldTermStructure> initCurve = initMarket->iborIndex(ind)->forwardingTermStructure();

        BOOST_CHECK_CLOSE(simCurve->discount(1), initCurve->discount(1), 1e-4);
    }
}

void testSwaptionVolCurve(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                          QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                          QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& ccy : parameters->ccys()) {
        Handle<QuantLib::SwaptionVolatilityStructure> simCurve = simMarket->swaptionVol(ccy);
        Handle<QuantLib::SwaptionVolatilityStructure> initCurve = initMarket->swaptionVol(ccy);
        for (const auto& maturity : parameters->swapVolExpiries("")) {
            for (const auto& tenor : parameters->swapVolTerms("")) {
                BOOST_CHECK_CLOSE(simCurve->volatility(maturity, tenor, 0.0, true),
                                  initCurve->volatility(maturity, tenor, 0.0, true), 1e-12);
            }
        }
    }
}

void testFxVolCurve(QuantLib::ext::shared_ptr<data::Market>& initMarket,
                    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket>& simMarket,
                    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& ccyPair : parameters->fxVolCcyPairs()) {
        Handle<BlackVolTermStructure> simCurve = simMarket->fxVol(ccyPair);
        Handle<BlackVolTermStructure> initCurve = initMarket->fxVol(ccyPair);
        vector<Date> dates;
        Date asof = initMarket->asofDate();
        for (Size i = 0; i < parameters->fxVolExpiries(ccyPair).size(); i++) {
            dates.push_back(asof + parameters->fxVolExpiries(ccyPair)[i]);
        }

        for (const auto& date : dates) {
            BOOST_CHECK_CLOSE(simCurve->blackVol(date, 0.0, true), initCurve->blackVol(date, 0.0, true), 1e-12);
        }
    }
}

void testDefaultCurve(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                      QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                      QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& spec : parameters->defaultNames()) {

        Handle<DefaultProbabilityTermStructure> simCurve = simMarket->defaultCurve(spec)->curve();
        Handle<DefaultProbabilityTermStructure> initCurve = initMarket->defaultCurve(spec)->curve();
        BOOST_CHECK_EQUAL(initCurve->referenceDate(), simCurve->referenceDate());
        vector<Date> dates;
        Date asof = initMarket->asofDate();
        for (Size i = 0; i < parameters->defaultTenors("").size(); i++) {
            dates.push_back(asof + parameters->defaultTenors("")[i]);
        }

        for (const auto& date : dates) {
            BOOST_CHECK_CLOSE(simCurve->survivalProbability(date, true), initCurve->survivalProbability(date, true),
                              1e-12);
        }
    }
}

void testZeroInflationCurve(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                            QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                            QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& spec : parameters->zeroInflationIndices()) {

        Handle<ZeroInflationTermStructure> simCurve = simMarket->zeroInflationIndex(spec)->zeroInflationTermStructure();
        Handle<ZeroInflationTermStructure> initCurve =
            initMarket->zeroInflationIndex(spec)->zeroInflationTermStructure();
        BOOST_CHECK_EQUAL(initCurve->referenceDate(), simCurve->referenceDate());
        vector<Date> dates;
        Date asof = initMarket->asofDate();
        for (Size i = 0; i < parameters->zeroInflationTenors("").size(); i++) {
            dates.push_back(asof + parameters->zeroInflationTenors("")[i]);
        }

        for (const auto& date : dates) {
            BOOST_CHECK_CLOSE(simCurve->zeroRate(date), initCurve->zeroRate(date), 1e-12);
        }
    }
}

void testCorrelationCurve(QuantLib::ext::shared_ptr<ore::data::Market>& initMarket,
                          QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
                          QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters>& parameters) {
    for (const auto& spec : parameters->correlationPairs()) {
        vector<string> tokens;
        boost::split(tokens, spec, boost::is_any_of(":&"));
        QL_REQUIRE(tokens.size() == 2, "not a valid correlation pair: " << spec);
        pair<string, string> pair = std::make_pair(tokens[0], tokens[1]);
        Handle<QuantExt::CorrelationTermStructure> simCurve = simMarket->correlationCurve(pair.first, pair.second);
        Handle<QuantExt::CorrelationTermStructure> initCurve = initMarket->correlationCurve(pair.first, pair.second);
        BOOST_CHECK_EQUAL(initCurve->referenceDate(), simCurve->referenceDate());
        vector<Date> dates;
        Date asof = initMarket->asofDate();
        for (Size i = 0; i < parameters->correlationExpiries().size(); i++) {
            dates.push_back(asof + parameters->correlationExpiries()[i]);
        }

        for (const auto& date : dates) {
            BOOST_CHECK_CLOSE(simCurve->correlation(date), initCurve->correlation(date), 1e-12);
        }
    }
}

void testToXML(QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> params) {

    BOOST_TEST_MESSAGE("Testing to XML...");
    XMLDocument outDoc;
    string testFile = "simtest.xml";
    XMLNode* simulationNode = params->toXML(outDoc);

    outDoc.appendNode(simulationNode);
    outDoc.toFile(testFile);

    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> newParams(new analytics::ScenarioSimMarketParameters());
    newParams->fromFile(testFile);
    BOOST_CHECK(*params == *newParams);

    newParams->baseCcy() = "JPY";
    BOOST_CHECK(*params != *newParams);

    remove("simtest.xml");
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(ScenarioSimMarketTest)

BOOST_AUTO_TEST_CASE(testScenarioSimMarket) {
    BOOST_TEST_MESSAGE("Testing OREAnalytics ScenarioSimMarket...");

    SavedSettings backup;

    Date today(20, Jan, 2015);
    Settings::instance().evaluationDate() = today;
    QuantLib::ext::shared_ptr<ore::data::Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // Empty scenario generator
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator> scenarioGenerator;

    // build scenario
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> parameters = scenarioParameters();
    convs();
    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket(
        new analytics::ScenarioSimMarket(initMarket, parameters));
    simMarket->scenarioGenerator() = scenarioGenerator;

    // test
    testFxSpot(initMarket, simMarket, parameters);
    testDiscountCurve(initMarket, simMarket, parameters);
    testIndexCurve(initMarket, simMarket, parameters);
    testSwaptionVolCurve(initMarket, simMarket, parameters);
    testFxVolCurve(initMarket, simMarket, parameters);
    testDefaultCurve(initMarket, simMarket, parameters);
    testZeroInflationCurve(initMarket, simMarket, parameters);
    testCorrelationCurve(initMarket, simMarket, parameters);
    testToXML(parameters);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
