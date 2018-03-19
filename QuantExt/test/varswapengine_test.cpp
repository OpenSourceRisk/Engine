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
#include <iostream>


#include "varswapengine_test.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
#include <qle/pricingengines/all.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/pricingengines/varswapengine.hpp>

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

namespace testsuite {

void VarSwapEngineTest::testT0Pricing() {

    std::cout << "Running QuantExt VarSwap test." << std::endl;
    const Date today = Date::todaysDate();
    DayCounter dc = Actual365Fixed();
    Date exDate = today + Integer(365.0 * 0.246575);
    std::vector<Date> dates = { exDate };

    std::vector<Real> strikes = { 50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0, //Put Strikes
                                    100.0, 105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0 };   //Call Strikes
    std::vector<Real> volsVector = { 0.3, 0.29, 0.28, 0.27, 0.26, 0.25, 0.24, 0.23, 0.22, 0.21, 0.2,    //Put Vols
                                        0.2, 0.19, 0.18, 0.17, 0.16, 0.15, 0.14, 0.13 };    //Call Vols
    Matrix vols(19,1,volsVector.begin(), volsVector.end());

    BOOST_TEST_MESSAGE("Testing t0 priocing of the QuantExt VarSwap engine, as per Demeterfi et. al (1999).");
    std::string equityName = "STE";
    boost::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<Quote> equityPrice = Handle<Quote>(boost::make_shared<SimpleQuote>(100.0));
    Handle<YieldTermStructure> yieldTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.0, dc));
    Handle<YieldTermStructure> dividendTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.0, dc));
    Handle<BlackVolTermStructure>& volTS = Handle<BlackVolTermStructure>(boost::make_shared<BlackVarianceSurface>(today, NullCalendar(), dates, strikes, vols, dc));
    Handle<YieldTermStructure> discountingTS = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.05, dc));
    Size numPuts = 11;
    Size numCalls = 8;
    Real stepSize = 0.05 / sqrt(ActualActual().yearFraction(today, exDate)); //This is % step size between option strikes / Time to Maturity

    boost::shared_ptr<GeneralizedBlackScholesProcess> stochProcess(
        new BlackScholesMertonProcess(equityPrice, dividendTS, yieldTS, volTS));

    boost::shared_ptr<PricingEngine> engine(
        new VarSwapEngine(equityName,
            equityPrice,
            yieldTS,
            dividendTS,
            volTS,
            discountingTS,
            numPuts,
            numCalls,
            stepSize));

    VarianceSwap varianceSwap(Position::Long, 0.04, 50000, today, exDate);
    varianceSwap.setPricingEngine(engine);

    Real resVariance = varianceSwap.variance();
    Real resExpectedValue = 0.04189;
    Real error = std::fabs(resVariance - resExpectedValue);
    Real tol = 1.0e-4;
    BOOST_CHECK_CLOSE(resVariance, resExpectedValue, tol);

}

test_suite* VarSwapEngineTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("QuantExt variance swap engine test");
    suite->add(BOOST_TEST_CASE(&VarSwapEngineTest::testT0Pricing));
    return suite;
} 
} // namespace testsuite