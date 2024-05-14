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

/*! \file test/varianceswap.hpp
  \brief variance swap pricing engine test suite
*/

#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>
#include <oret/toplevelfixture.hpp>

#include <boost/test/unit_test.hpp>

#include <qle/indexes/equityindex.hpp>
#include <qle/models/cdsoptionhelper.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/crlgm1fparametrization.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/models/crossassetanalyticsbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetmodelimpliedeqvoltermstructure.hpp>
#include <qle/models/crossassetmodelimpliedfxvoltermstructure.hpp>
#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>
#include <qle/models/dkimpliedzeroinflationtermstructure.hpp>
#include <qle/models/eqbsconstantparametrization.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/eqbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/lgmimplieddefaulttermstructure.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>
#include <qle/models/pseudoparameter.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/pricingengines/analyticlgmcdsoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>
#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingcommodityforwardengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingequityforwardengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/pricingengines/forward/replicatingvarianceswapengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>

#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <boost/make_shared.hpp>

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

namespace {

struct ReplicatingVarianceSwapData {
    Position::Type type;
    Real varStrike;
    Real nominal;
    Real s;       // spot
    Rate q;       // dividend
    Rate r;       // risk-free rate
    Time t;       // time to maturity
    Volatility v; // volatility at t
    Real result;  // result
    Real tol;     // tolerance
};

struct Datum {
    Option::Type type;
    Real strike;
    Volatility v;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREPlusEquityFXTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(GeneralisedReplicatingVarianceSwapEngineTest)

BOOST_AUTO_TEST_CASE(testT0Pricing) {

    SavedSettings backup;
    Date today = Date(3, Oct, 2019);
    Settings::instance().evaluationDate() = today;
    Calendar cal = TARGET();
    DayCounter dc = Actual365Fixed();
    Date exDate = today + Integer(0.246575 * 365 + 0.5);
    std::vector<Date> dates(1, exDate);
    Real volatilityStrike = 0.2;
    Real varianceStrike = volatilityStrike * volatilityStrike;
    Real vegaNotional = 50000.0;
    Real varianceNotional = vegaNotional / (2.0 * 100.0 * volatilityStrike);

    // add strikes in C++98 compatible way
    Real arrStrikes[] = {50.0,  55.0,  60.0,  65.0,  70.0,  75.0,  80.0, 85.0, 90.0, 95.0, 100.0, // Put Strikes
                         105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0};                        // Call Strikes
    std::vector<Real> strikes(arrStrikes, arrStrikes + sizeof(arrStrikes) / sizeof(Real));
    // add vols in C++98 compatible way
    Real arrVols[] = {0.3,  0.29, 0.28, 0.27, 0.26, 0.25, 0.24, 0.23, 0.22, 0.21, 0.2, // Put Vols
                      0.19, 0.18, 0.17, 0.16, 0.15, 0.14, 0.13};                       // Call Vols
    std::vector<Real> volsVector(arrVols, arrVols + sizeof(arrVols) / sizeof(Real));
    Matrix vols(18, 1, volsVector.begin(), volsVector.end());

    BOOST_TEST_MESSAGE("Testing t0 pricing of the QuantExt VarSwap engine, as per Demeterfi et. al (1999).");
    std::string equityName = "STE";
    QuantLib::ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<Quote> equityPrice = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100.0));
    Handle<YieldTermStructure> yieldTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    Handle<YieldTermStructure> dividendTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, dc));
    Handle<BlackVolTermStructure> volTS = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackVarianceSurface>(today, NullCalendar(), dates, strikes, vols, dc));
    Handle<YieldTermStructure> discountingTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    QuantLib::ext::shared_ptr<Index> eqIndex =
        QuantLib::ext::make_shared<EquityIndex2>(equityName, cal, EURCurrency(), equityPrice, yieldTS, dividendTS);

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
        new BlackScholesMertonProcess(equityPrice, dividendTS, discountingTS, volTS));

    QuantLib::ext::shared_ptr<PricingEngine> engine(new GeneralisedReplicatingVarianceSwapEngine(
        eqIndex, stochProcess, discountingTS, GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings()));

    QuantExt::VarianceSwap2 varianceSwap(Position::Long, varianceStrike, varianceNotional, today, exDate, cal, false);
    varianceSwap.setPricingEngine(engine);

    Real result = varianceSwap.variance();
    Real expected = 0.040203605175062058;
    Real tol = 1.0e-4;
    BOOST_CHECK_CLOSE(result, expected, tol);
    result = varianceSwap.NPV();
    expected = 2513.8798089810457;
    BOOST_CHECK_CLOSE(result, expected, tol);
}

BOOST_AUTO_TEST_CASE(testSeasonedSwapPricing) {

    SavedSettings backup;
    Date today = Date(30, April, 2018);
    Settings::instance().evaluationDate() = today;
    DayCounter dc = Actual365Fixed();
    Date startDate = today - Integer(0.019178 * 365 + 0.5); // started 7 calendar days ago
    Date exDate = today + Integer(0.246575 * 365 + 0.5);
    std::string equityName = "STE";
    Calendar cal = TARGET();
    std::vector<Date> pastDates;
    std::vector<Date> dates(1, exDate);
    Real volatilityStrike = 0.2;
    Real varianceStrike = volatilityStrike * volatilityStrike;
    Real vegaNotional = 50000.0;
    Real varianceNotional = vegaNotional / (2.0 * 100.0 * volatilityStrike);

    for (Date day = cal.adjust(cal.advance(startDate, -1, Days)); day < today; day = cal.advance(day, 1, Days)) {
        pastDates.push_back(day);
    }

    // add fixings in C++98 compatible way
    Real arrFixings[] = {98.5, 98.0, 99.0, 100.2, 99.4, 98.2};
    std::vector<Real> fixings(arrFixings, arrFixings + sizeof(arrFixings) / sizeof(Real));
    TimeSeries<Real> fixingHistory(pastDates.begin(), pastDates.end(), fixings.begin());
    IndexManager::instance().setHistory(equityName, fixingHistory);

    // add strikes in C++98 compatible way
    Real arrStrikes[] = {50.0,  55.0,  60.0,  65.0,  70.0,  75.0,  80.0, 85.0, 90.0, 95.0, 100.0, // Put Strikes
                         105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0};                        // Call Strikes
    std::vector<Real> strikes(arrStrikes, arrStrikes + sizeof(arrStrikes) / sizeof(Real));
    // add vols in C++98 compatible way
    Real arrVols[] = {0.3,  0.29, 0.28, 0.27, 0.26, 0.25, 0.24, 0.23, 0.22, 0.21, 0.2, // Put Vols
                      0.19, 0.18, 0.17, 0.16, 0.15, 0.14, 0.13};                       // Call Vols
    std::vector<Real> volsVector(arrVols, arrVols + sizeof(arrVols) / sizeof(Real));
    Matrix vols(18, 1, volsVector.begin(), volsVector.end());

    BOOST_TEST_MESSAGE("Testing seasoned swap pricing of the QuantExt VarSwap engine.");
    QuantLib::ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<Quote> equityPrice = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100.0));
    Handle<YieldTermStructure> yieldTS = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, cal, 0.05, dc));
    Handle<YieldTermStructure> dividendTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, cal, 0.0, dc));
    Handle<BlackVolTermStructure> volTS =
        Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<BlackVarianceSurface>(today, cal, dates, strikes, vols, dc));
    Handle<YieldTermStructure> discountingTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, cal, 0.05, dc));
    QuantLib::ext::shared_ptr<Index> eqIndex =
        QuantLib::ext::make_shared<EquityIndex2>(equityName, cal, EURCurrency(), equityPrice, yieldTS, dividendTS);

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
        new BlackScholesMertonProcess(equityPrice, dividendTS, yieldTS, volTS));

    QuantLib::ext::shared_ptr<PricingEngine> engine(new GeneralisedReplicatingVarianceSwapEngine(
        eqIndex, stochProcess, discountingTS, GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings()));

    QuantExt::VarianceSwap2 varianceSwap(Position::Long, varianceStrike, varianceNotional, startDate, exDate, cal,
                                        false);
    varianceSwap.setPricingEngine(engine);

    Real result = varianceSwap.variance();
    Real expected = 0.040169651620750264;
    Real tol = 1.0e-4;
    BOOST_CHECK_CLOSE(result, expected, tol);
    result = varianceSwap.NPV();
    expected = 2094.6608249765977;
    BOOST_CHECK_CLOSE(result, expected, tol);

    // A little clean up of the environment.
    IndexManager::instance().clearHistory("EQ/" + equityName);
}

BOOST_AUTO_TEST_CASE(testForwardStartPricing) {

    SavedSettings backup;
    Date today(2, Jul, 2018);
    Settings::instance().evaluationDate() = today;
    Calendar cal = TARGET();
    DayCounter dc = Actual365Fixed();
    Date exDate = today + Integer(0.246575 * 365 + 0.5);
    std::vector<Date> dates(1, exDate);
    Real volatilityStrike = 0.2;
    Real varianceStrike = volatilityStrike * volatilityStrike;
    Real vegaNotional = 50000.0;
    Real varianceNotional = vegaNotional / (2.0 * 100.0 * volatilityStrike);

    // add strikes in C++98 compatible way
    Real arrStrikes[] = {50.0,  55.0,  60.0,  65.0,  70.0,  75.0,  80.0, 85.0, 90.0, 95.0, 100.0, // Put Strikes
                         105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0};                        // Call Strikes
    std::vector<Real> strikes(arrStrikes, arrStrikes + sizeof(arrStrikes) / sizeof(Real));
    // add vols in C++98 compatible way
    Real arrVols[] = {0.3,  0.29, 0.28, 0.27, 0.26, 0.25, 0.24, 0.23, 0.22, 0.21, 0.2, // Put Vols
                      0.19, 0.18, 0.17, 0.16, 0.15, 0.14, 0.13};                       // Call Vols
    std::vector<Real> volsVector(arrVols, arrVols + sizeof(arrVols) / sizeof(Real));
    Matrix vols(18, 1, volsVector.begin(), volsVector.end());

    BOOST_TEST_MESSAGE(
        "Testing future starting pricing of the QuantExt VarSwap engine, as per Demeterfi et. al (1999).");
    std::string equityName = "STE";
    QuantLib::ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<Quote> equityPrice = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100.0));
    Handle<YieldTermStructure> yieldTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    Handle<YieldTermStructure> dividendTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, dc));
    Handle<BlackVolTermStructure> volTS = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackVarianceSurface>(today, NullCalendar(), dates, strikes, vols, dc));
    Handle<YieldTermStructure> discountingTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    QuantLib::ext::shared_ptr<Index> eqIndex =
        QuantLib::ext::make_shared<EquityIndex2>(equityName, cal, EURCurrency(), equityPrice, yieldTS, dividendTS);

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
        new BlackScholesMertonProcess(equityPrice, dividendTS, discountingTS, volTS));

    QuantLib::ext::shared_ptr<PricingEngine> engine(new GeneralisedReplicatingVarianceSwapEngine(
        eqIndex, stochProcess, discountingTS, GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings()));

    QuantExt::VarianceSwap2 varianceSwap(Position::Long, varianceStrike, varianceNotional, today + 7, exDate, cal,
                                        false);
    varianceSwap.setPricingEngine(engine);

    Real result = varianceSwap.variance();
    Real expected = 0.038880652347511133;
    Real tol = 1.0e-4;
    BOOST_CHECK_CLOSE(result, expected, tol);
    result = varianceSwap.NPV();
    expected = -13820.40246258254;
    BOOST_CHECK_CLOSE(result, expected, tol);
}

BOOST_AUTO_TEST_CASE(testReplicatingVarianceSwap) {

    BOOST_TEST_MESSAGE("Testing variance swap with replicating cost engine...");

    ReplicatingVarianceSwapData values[] = {

        // data from "A Guide to Volatility and Variance Swaps",
        //   Derman, Kamal & Zou, 1999
        //   with maturity t corrected from 0.25 to 0.246575
        //   corresponding to Jan 1, 1999 to Apr 1, 1999

        // type, varStrike, nominal,     s,    q,    r,        t,    v,  result, tol
        {Position::Long, 0.04, 50000, 100.0, 0.00, 0.05, 0.246575, 0.20, 0.041888574, 1.0e-4}

    };

    Datum replicatingOptionData[] = {

        // data from "A Guide to Volatility and Variance Swaps",
        //   Derman, Kamal & Zou, 1999

        // Option::Type, strike, v
        {Option::Put, 50, 0.30},   {Option::Put, 55, 0.29},   {Option::Put, 60, 0.28},   {Option::Put, 65, 0.27},
        {Option::Put, 70, 0.26},   {Option::Put, 75, 0.25},   {Option::Put, 80, 0.24},   {Option::Put, 85, 0.23},
        {Option::Put, 90, 0.22},   {Option::Put, 95, 0.21},   {Option::Put, 100, 0.20},  {Option::Call, 100, 0.20},
        {Option::Call, 105, 0.19}, {Option::Call, 110, 0.18}, {Option::Call, 115, 0.17}, {Option::Call, 120, 0.16},
        {Option::Call, 125, 0.15}, {Option::Call, 130, 0.14}, {Option::Call, 135, 0.13}};

    SavedSettings backup;
    DayCounter dc = Actual365Fixed();
    Date today = Date::todaysDate();
    Settings::instance().evaluationDate() = today;

    QuantLib::ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    QuantLib::ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    QuantLib::ext::shared_ptr<YieldTermStructure> qTS =
        QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(today, Handle<Quote>(qRate), dc));
    QuantLib::ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    QuantLib::ext::shared_ptr<YieldTermStructure> rTS =
        QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(today, Handle<Quote>(rRate), dc));

    for (Size i = 0; i < LENGTH(values); i++) {
        Date exDate = today + Integer(values[i].t * 365 + 0.5);
        std::vector<Date> dates(1);
        dates[0] = exDate;

        spot->setValue(values[i].s);
        qRate->setValue(values[i].q);
        rRate->setValue(values[i].r);

        Size options = LENGTH(replicatingOptionData);
        std::vector<Real> callStrikes, putStrikes, callVols, putVols;

        // Assumes ascending strikes and same min call and max put strikes
        Size j;
        for (j = 0; j < options; j++) {
            if (replicatingOptionData[j].type == Option::Call) {
                callStrikes.push_back(replicatingOptionData[j].strike);
                callVols.push_back(replicatingOptionData[j].v);
            } else if (replicatingOptionData[j].type == Option::Put) {
                putStrikes.push_back(replicatingOptionData[j].strike);
                putVols.push_back(replicatingOptionData[j].v);
            } else {
                QL_FAIL("unknown option type");
            }
        }

        Matrix vols(options - 1, 1);
        std::vector<Real> strikes;
        for (j = 0; j < putVols.size(); j++) {
            vols[j][0] = putVols[j];
            strikes.push_back(putStrikes[j]);
        }

        for (Size k = 1; k < callVols.size(); k++) {
            Size j = putVols.size() - 1;
            vols[j + k][0] = callVols[k];
            strikes.push_back(callStrikes[k]);
        }

        QuantLib::ext::shared_ptr<BlackVolTermStructure> volTS(
            new BlackVarianceSurface(today, NullCalendar(), dates, strikes, vols, dc));

        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));

        QuantLib::ext::shared_ptr<PricingEngine> engine(
            new ReplicatingVarianceSwapEngine(stochProcess, 5.0, callStrikes, putStrikes));

        QuantLib::VarianceSwap varianceSwap(values[i].type, values[i].varStrike, values[i].nominal, today, exDate);
        varianceSwap.setPricingEngine(engine);

        Real calculated = varianceSwap.variance();
        Real expected = values[i].result;
        Real tol = 1.0e-4;
        BOOST_CHECK_CLOSE(calculated, expected, tol);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
