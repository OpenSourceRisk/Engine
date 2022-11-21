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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/instruments/commodityspreadoption.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {

double monteCarloPricing(double F1, double F2, double sigma1, double sigma2, double rho, double ttm, double df,
                         double strike) {
    constexpr size_t seed = 42;
    constexpr size_t samples = 100000;
    Matrix L(2, 2, 0);
    L[0][0] = sigma1;
    L[1][0] = rho * sigma2;
    L[1][1] = std::sqrt(1.0 - rho * rho) * sigma2;
    Array F(2, 0);
    Array Z(2, 0);
    Array sigma(2, 0);
    F[0] = std::log(F1);
    F[1] = std::log(F2);
    sigma[0] = sigma1;
    sigma[1] = sigma2;

    double payoff = 0;
    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(2, seed);
    Array drift = -0.5 * sigma * sigma * ttm;
    for (int i = 0; i < samples; i++) {
        auto sample = rsg.nextSequence().value;
        std::copy(sample.begin(), sample.end(), Z.begin());
        Array Ft = F + drift + L * Z * std::sqrt(ttm);
        payoff += std::max(std::exp(Ft[0]) - std::exp(Ft[1]) - strike, 0.0);
    }

    return payoff * df / (double)samples;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommoditySpreadOptionTests)

BOOST_AUTO_TEST_CASE(testCrossAssetFutureSpread) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    std::vector<Date> futureExpiryDates = {Date(5, Nov, 2022), Date(30, Nov, 2022), Date(31, Dec, 2022)};
    std::vector<Real> brentQuotes = {100., 105., 106.};
    std::vector<Real> wtiQuotes = {99., 104., 107.};

    double strike = 2.;
    double quantity = 1000.;
    Date expiry(15, Dec, 2022);
    double volQuote1 = 0.3;
    double volQuote2 = 0.35;

    Date contractExpiry1(31, Dec, 2022);
    Date contractExpiry2(31, Dec, 2022);

    auto brentCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));
    auto wtiCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, wtiQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), volQuote1, Actual365Fixed()));
    auto vol2 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), volQuote2, Actual365Fixed()));

    auto index1 = boost::make_shared<CommodityFuturesIndex>("BRENT_USD", contractExpiry1, NullCalendar(), brentCurve);

    auto index2 = boost::make_shared<CommodityFuturesIndex>("WTI_USD", contractExpiry2, NullCalendar(), wtiCurve);

    auto flow1 = boost::make_shared<CommodityIndexedCashFlow>(100, contractExpiry1, Date(31, Dec, 2022), index1);

    auto flow2 = boost::make_shared<CommodityIndexedCashFlow>(100, contractExpiry2, Date(31, Dec, 2022), index2);

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiry);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call);

    for (const auto& rho : {-0.95, -0.5, -0.25, 0., 0.5, 0.75, 0.9, 0.95}) {
        auto engine = boost::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol2, rho);
        spreadOption.setPricingEngine(engine);
        double npvMC =
            monteCarloPricing(106., 107., volQuote1, volQuote2, rho, discount->timeFromReference(exercise->lastDate()),
                              discount->discount(exercise->lastDate()), strike) *
            quantity;
        double npvKrik = spreadOption.NPV();
        BOOST_CHECK_CLOSE(npvKrik, npvMC, 1.);
    }
}

BOOST_AUTO_TEST_CASE(testCalendarSpread) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    std::vector<Date> futureExpiryDates = {Date(5, Nov, 2022), Date(30, Nov, 2022), Date(31, Dec, 2022)};
    std::vector<Real> brentQuotes = {100., 104, 105};

    double strike = 1;
    double volBrent = 0.3;
    double rho = 0.9;

    auto brentCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), volBrent, Actual365Fixed()));

    auto index1 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_DEC_USD", Date(31, Dec, 2022), NullCalendar(), brentCurve);

    auto index2 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_NOV_USD", Date(30, Nov, 2022), NullCalendar(), brentCurve);

    auto flow1 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(31, Dec, 2022), Date(15, Nov, 2022), index1);

    auto flow2 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(30, Nov, 2022), Date(15, Nov, 2022), index2);

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(15, Nov, 2022));

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, 1000., strike, Option::Call);

    auto engine = boost::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol1, rho);

    spreadOption.setPricingEngine(engine);

    std::cout << "MC price "
              << monteCarloPricing(105., 104., volBrent, volBrent, rho,
                                   discount->timeFromReference(exercise->lastDate()),
                                   discount->discount(exercise->lastDate()), strike) *
                     1000
              << std::endl;

    std::cout << "Kirk approximation " << spreadOption.NPV() << std::endl;
}

BOOST_AUTO_TEST_CASE(testCalendarSpread2) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    std::vector<Date> futureExpiryDates = {Date(5, Nov, 2022), Date(30, Nov, 2022), Date(31, Dec, 2022)};
    std::vector<Real> brentQuotes = {100., 104, 105};

    double strike = 1;
    double volBrent = 0.3;
    double rho = 0.9;

    auto brentCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), volBrent, Actual365Fixed()));

    auto index1 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_DEC_USD", Date(31, Dec, 2022), NullCalendar(), brentCurve);

    auto index2 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_NOV_USD", Date(30, Nov, 2022), NullCalendar(), brentCurve);

    auto flow1 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(31, Dec, 2022), Date(5, Dec, 2022), index1);

    auto flow2 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(30, Nov, 2022), Date(5, Dec, 2022), index2);

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(5, Dec, 2022));

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, 1000., strike, Option::Call);

    auto engine = boost::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol1, rho);

    spreadOption.setPricingEngine(engine);

    double volScalingFactor =
        std::sqrt(discount->timeFromReference(Date(30, Nov, 2022)) / discount->timeFromReference(exercise->lastDate()));

    std::cout << "MC price "
              << monteCarloPricing(105., 104., volBrent, volBrent * volScalingFactor, rho,
                                   discount->timeFromReference(exercise->lastDate()),
                                   discount->discount(exercise->lastDate()), strike) *
                     1000
              << std::endl;

    std::cout << "Kirk approximation " << spreadOption.NPV() << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
