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
//remove these after debugging

#include "varswapengine_test.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
#include <qle/pricingengines/all.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/pricingengines/forward/replicatingvarianceswapengine.hpp>
#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>

#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

namespace {

    struct ReplicatingVarianceSwapData {
        Position::Type type;
        Real varStrike;
        Real nominal;
        Real s;         // spot
        Rate q;         // dividend
        Rate r;         // risk-free rate
        Time t;         // time to maturity
        Volatility v;   // volatility at t
        Real result;    // result
        Real tol;       // tolerance
    };

    struct Datum {
        Option::Type type;
        Real strike;
        Volatility v;
    };

}

namespace testsuite {

void GeneralisedReplicatingVarianceSwapEngineTest::testT0Pricing() {

    const Date today = Date::todaysDate();
    Calendar cal = TARGET();
    DayCounter dc = Actual365Fixed();
    Date exDate = today + Integer(0.246575 * 365 + 0.5);
    std::vector<Date> dates = { exDate };

    std::vector<Real> strikes = { 50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0, //Put Strikes
                                    105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0 };   //Call Strikes
    std::vector<Real> volsVector = { 0.3, 0.29, 0.28, 0.27, 0.26, 0.25, 0.24, 0.23, 0.22, 0.21, 0.2,    //Put Vols
                                        0.19, 0.18, 0.17, 0.16, 0.15, 0.14, 0.13 };    //Call Vols
    Matrix vols(18,1,volsVector.begin(), volsVector.end());

    BOOST_TEST_MESSAGE("Testing t0 pricing of the QuantExt VarSwap engine, as per Demeterfi et. al (1999).");
    std::string equityName = "STE";
    boost::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<Quote> equityPrice = Handle<Quote>(boost::make_shared<SimpleQuote>(100.0));
    Handle<YieldTermStructure> yieldTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    Handle<YieldTermStructure> dividendTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.0, dc));
    Handle<BlackVolTermStructure> volTS = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceSurface>(today, NullCalendar(), dates, strikes, vols, dc));
    Handle<YieldTermStructure> discountingTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    Size numPuts = 11;
    Size numCalls = 8;
    Real stepSize = 0.05 / sqrt(ActualActual().yearFraction(today, exDate)); //This is % step size between option strikes / Time to Maturity

    boost::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
        new BlackScholesMertonProcess(equityPrice, dividendTS, discountingTS, volTS));

    boost::shared_ptr<PricingEngine> engine(
        new GeneralisedReplicatingVarianceSwapEngine(equityName,
            stochProcess,
            discountingTS,
            cal,
            numPuts,
            numCalls,
            stepSize));

    VarianceSwap varianceSwap(Position::Long, 0.04, 50000, today, exDate);
    varianceSwap.setPricingEngine(engine);

    Real result = varianceSwap.variance();
    Real expected = 0.041888574;
    Real tol = 1.0e-4;
    BOOST_CHECK_CLOSE(result, expected, tol);
}

void GeneralisedReplicatingVarianceSwapEngineTest::testSeasonedSwapPricing() {

    Date today = Date::todaysDate();
    DayCounter dc = Actual365Fixed();
    Date startDate = today - Integer(0.019178 * 365 + 0.5);  //started 7 calendar days ago
    Date exDate = today + Integer(0.246575 * 365 + 0.5);
    std::string equityName = "STE";
    Calendar cal = TARGET();
    std::vector<Date> pastDates;
    std::vector<Date> dates = { exDate };

    for (Date day = cal.adjust(cal.advance(startDate, -1, Days)); day < today; day = cal.advance(day, 1, Days)) {
        pastDates.push_back(day);
    }
    
    std::vector<Real> fixings{ 98.5, 98.0, 99.0, 100.2, 99.4 ,98.2};
    TimeSeries<Real> fixingHistory(pastDates.begin(), pastDates.end(), fixings.begin());
    IndexManager::instance().setHistory("EQ/" + equityName, fixingHistory);

    std::vector<Real> strikes = { 50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0, //Put Strikes
        105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0 };   //Call Strikes
    std::vector<Real> volsVector = { 0.3, 0.29, 0.28, 0.27, 0.26, 0.25, 0.24, 0.23, 0.22, 0.21, 0.2,    //Put Vols
        0.19, 0.18, 0.17, 0.16, 0.15, 0.14, 0.13 };    //Call Vols
    Matrix vols(18, 1, volsVector.begin(), volsVector.end());

    BOOST_TEST_MESSAGE("Testing seasoned swap pricing of the QuantExt VarSwap engine.");
    boost::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<Quote> equityPrice = Handle<Quote>(boost::make_shared<SimpleQuote>(100.0));
    Handle<YieldTermStructure> yieldTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, cal, 0.05, dc));
    Handle<YieldTermStructure> dividendTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, cal, 0.0, dc));
    Handle<BlackVolTermStructure> volTS = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceSurface>(today, cal, dates, strikes, vols, dc));
    Handle<YieldTermStructure> discountingTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, cal, 0.05, dc));
    Size numPuts = 11;
    Size numCalls = 8;
    Real stepSize = 0.05 / sqrt(dc.yearFraction(today, exDate)); //This is % step size between option strikes / Time to Maturity

    boost::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
        new BlackScholesMertonProcess(equityPrice, dividendTS, yieldTS, volTS));

    boost::shared_ptr<PricingEngine> engine(
        new GeneralisedReplicatingVarianceSwapEngine(equityName,
            stochProcess,
            discountingTS,
            cal,
            numPuts,
            numCalls,
            stepSize));

    VarianceSwap varianceSwap(Position::Long, 0.04, 50000, startDate, exDate);
    varianceSwap.setPricingEngine(engine);

    Real result = varianceSwap.variance();
    Real expected = 0.041888574;
    Real tol = 1.0e-4;
    BOOST_CHECK_CLOSE(result, expected, tol);
}

void GeneralisedReplicatingVarianceSwapEngineTest::testReplicatingVarianceSwap() {

    BOOST_TEST_MESSAGE("Testing variance swap with replicating cost engine...");

    ReplicatingVarianceSwapData values[] = {

        // data from "A Guide to Volatility and Variance Swaps",
        //   Derman, Kamal & Zou, 1999
        //   with maturity t corrected from 0.25 to 0.246575
        //   corresponding to Jan 1, 1999 to Apr 1, 1999

        //type, varStrike, nominal,     s,    q,    r,        t,    v,  result, tol
        { Position::Long,      0.04,   50000, 100.0, 0.00, 0.05, 0.246575, 0.20, 0.041888574, 1.0e-4 }

    };

    Datum replicatingOptionData[] = {

        // data from "A Guide to Volatility and Variance Swaps",
        //   Derman, Kamal & Zou, 1999

        //Option::Type, strike, v
        { Option::Put,   50,  0.30 },
        { Option::Put,   55,  0.29 },
        { Option::Put,   60,  0.28 },
        { Option::Put,   65,  0.27 },
        { Option::Put,   70,  0.26 },
        { Option::Put,   75,  0.25 },
        { Option::Put,   80,  0.24 },
        { Option::Put,   85,  0.23 },
        { Option::Put,   90,  0.22 },
        { Option::Put,   95,  0.21 },
        { Option::Put,  100,  0.20 },
        { Option::Call, 100,  0.20 },
        { Option::Call, 105,  0.19 },
        { Option::Call, 110,  0.18 },
        { Option::Call, 115,  0.17 },
        { Option::Call, 120,  0.16 },
        { Option::Call, 125,  0.15 },
        { Option::Call, 130,  0.14 },
        { Option::Call, 135,  0.13 }
    };

    DayCounter dc = Actual365Fixed();
    Date today = Date::todaysDate();

    boost::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    boost::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    boost::shared_ptr<YieldTermStructure> qTS = boost::shared_ptr<YieldTermStructure>(new FlatForward(today, Handle<Quote>(qRate), dc));
    boost::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    boost::shared_ptr<YieldTermStructure> rTS = boost::shared_ptr<YieldTermStructure>(new FlatForward(today, Handle<Quote>(rRate), dc));

    for (Size i = 0; i<LENGTH(values); i++) {
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
        for (j = 0; j<options; j++) {
            if (replicatingOptionData[j].type == Option::Call) {
                callStrikes.push_back(replicatingOptionData[j].strike);
                callVols.push_back(replicatingOptionData[j].v);
            }
            else if (replicatingOptionData[j].type == Option::Put) {
                putStrikes.push_back(replicatingOptionData[j].strike);
                putVols.push_back(replicatingOptionData[j].v);
            }
            else {
                QL_FAIL("unknown option type");
            }
        }

        Matrix vols(options - 1, 1);
        std::vector<Real> strikes;
        for (j = 0; j<putVols.size(); j++) {
            vols[j][0] = putVols[j];
            strikes.push_back(putStrikes[j]);
        }

        for (Size k = 1; k<callVols.size(); k++) {
            Size j = putVols.size() - 1;
            vols[j + k][0] = callVols[k];
            strikes.push_back(callStrikes[k]);
        }

        boost::shared_ptr<BlackVolTermStructure> volTS(new
            BlackVarianceSurface(today, NullCalendar(),
                dates, strikes, vols, dc));

        boost::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
            new BlackScholesMertonProcess(
                Handle<Quote>(spot),
                Handle<YieldTermStructure>(qTS),
                Handle<YieldTermStructure>(rTS),
                Handle<BlackVolTermStructure>(volTS)));


        boost::shared_ptr<PricingEngine> engine(
            new ReplicatingVarianceSwapEngine(stochProcess, 5.0,
                callStrikes,
                putStrikes));

        VarianceSwap varianceSwap(values[i].type,
            values[i].varStrike,
            values[i].nominal,
            today,
            exDate);
        varianceSwap.setPricingEngine(engine);

        Real calculated = varianceSwap.variance();
        Real expected = values[i].result;
        Real error = std::fabs(calculated - expected);
        Real tol = 1.0e-4;
        BOOST_CHECK_CLOSE(calculated, expected, tol);
    }
}


test_suite* GeneralisedReplicatingVarianceSwapEngineTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("QuantExt variance swap engine test");
    suite->add(BOOST_TEST_CASE(&GeneralisedReplicatingVarianceSwapEngineTest::testReplicatingVarianceSwap));
    suite->add(BOOST_TEST_CASE(&GeneralisedReplicatingVarianceSwapEngineTest::testT0Pricing));
    suite->add(BOOST_TEST_CASE(&GeneralisedReplicatingVarianceSwapEngineTest::testSeasonedSwapPricing));
    return suite;
} 
} // namespace testsuite