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
#include <ql/pricingengines/blackformula.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/instruments/commodityspreadoption.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/time/futureexpirycalculator.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {

class MockUpExpiryCalculator : public FutureExpiryCalculator {
public:
    // Inherited via FutureExpiryCalculator
    virtual QuantLib::Date nextExpiry(bool includeExpiry, const QuantLib::Date& referenceDate, QuantLib::Natural offset,
                                      bool forOption) override {
        return Date::endOfMonth(referenceDate);
    }
    virtual QuantLib::Date priorExpiry(bool includeExpiry, const QuantLib::Date& referenceDate,
                                       bool forOption) override {
        return Date(1, referenceDate.month(), referenceDate.year()) - 1 * Days;
    }
    virtual QuantLib::Date expiryDate(const QuantLib::Date& contractDate, QuantLib::Natural monthOffset,
                                      bool forOption) override {
        return Date::endOfMonth(contractDate);
    }
    virtual QuantLib::Date contractDate(const QuantLib::Date& expiryDate) override { return expiryDate; }
    virtual QuantLib::Date applyFutureMonthOffset(const QuantLib::Date& contractDate,
                                                  Natural futureMonthOffset) override {
        return QuantLib::Date();
    }
};

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
    for (size_t i = 0; i < samples; i++) {
        auto sample = rsg.nextSequence().value;
        std::copy(sample.begin(), sample.end(), Z.begin());
        Array Ft = F + drift + L * Z * std::sqrt(ttm);
        payoff += std::max(std::exp(Ft[0]) - std::exp(Ft[1]) - strike, 0.0);
    }

    return payoff * df / (double)samples;
}

double monteCarloPricingSpotAveraging(const ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow1,
                                      const ext::shared_ptr<PriceTermStructure> priceCurve1,
                                      const ext::shared_ptr<BlackVolTermStructure> vol1,
                                      const ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow2,
                                      const ext::shared_ptr<PriceTermStructure> priceCurve2,
                                      const ext::shared_ptr<BlackVolTermStructure> vol2, Real rho, Real strike,
                                      Real df) {

    QL_REQUIRE(flow1->startDate() == flow2->startDate() && flow1->endDate() == flow2->endDate(),
               "Support only Averaging Flows with same observation Period");

    Date today = Settings::instance().evaluationDate();

    vector<double> timeGrid;
    timeGrid.push_back(0.0);

    double accrued1 = 0;
    double accrued2 = 0;
    size_t nObs = 0; // future obs
    size_t n = flow1->indices().size();

    for (const auto& [pricingDate, index] : flow1->indices()) {

        Date fixingDate = index->fixingCalendar().adjust(pricingDate, Preceding);
        if (pricingDate > today) {
            timeGrid.push_back(vol1->timeFromReference(fixingDate));
            nObs++;
        } else {
            accrued1 += index->fixing(fixingDate);
        }
    }

    for (const auto& [pricingDate, index] : flow2->indices()) {
        if (pricingDate <= today) {
            Date fixingDate = index->fixingCalendar().adjust(pricingDate, Preceding);
            accrued2 += index->fixing(fixingDate);
        }
    }

    Array drifts;

    constexpr size_t samples = 100000;
    Matrix L(2, 2, 0);
    L[0][0] = 1;
    L[1][0] = rho;
    L[1][1] = std::sqrt(1.0 - rho * rho);

    Matrix drift(2, nObs, 0.0);
    Matrix diffusion(2, nObs, 0.0);
    Matrix St(2, nObs + 1, 0.0);

    St[0][0] = std::log(priceCurve1->price(0));
    St[1][0] = std::log(priceCurve2->price(0));

    Matrix Z(2, nObs, 0.0);

    for (size_t t = 0; t < nObs; ++t) {
        drift[0][t] =
            std::log(priceCurve1->price(timeGrid[t + 1]) / priceCurve1->price(timeGrid[t])) -
            0.5 * vol1->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1]));
        drift[1][t] =
            std::log(priceCurve2->price(timeGrid[t + 1]) / priceCurve2->price(timeGrid[t])) -
            0.5 * vol2->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1]));
        diffusion[0][t] =
            std::sqrt(vol1->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1])));
        diffusion[1][t] =
            std::sqrt(vol2->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1])));
    }

    double payoff = 0;
    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(2 * nObs, 42);
    for (size_t i = 0; i < samples; i++) {
        double avg1 = 0;
        double avg2 = 0;
        auto sample = rsg.nextSequence().value;
        std::copy(sample.begin(), sample.end(), Z.begin());
        auto Zt = L * Z;
        for (size_t t = 0; t < nObs; ++t) {
            St[0][t + 1] = St[0][t] + drift[0][t] + diffusion[0][t] * Zt[0][t];
            St[1][t + 1] = St[1][t] + drift[1][t] + diffusion[1][t] * Zt[1][t];
            avg1 += std::exp(St[0][t + 1]);
            avg2 += std::exp(St[1][t + 1]);
        }
        avg1 += accrued1;
        avg2 += accrued2;
        avg1 /= static_cast<double>(n);
        avg2 /= static_cast<double>(n);

        payoff += std::max(avg1 - avg2 - strike, 0.0);
    }
    payoff /= static_cast<double>(samples);
    return df * payoff;
}

double monteCarloPricingFutureAveraging(const ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow1,
                                        const ext::shared_ptr<PriceTermStructure> priceCurve1,
                                        const ext::shared_ptr<BlackVolTermStructure> vol1,
                                        const ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow2,
                                        const ext::shared_ptr<PriceTermStructure> priceCurve2,
                                        const ext::shared_ptr<BlackVolTermStructure> vol2, Real rho, Real strike,
                                        Real df) {

    QL_REQUIRE(flow1->startDate() == flow2->startDate() && flow1->endDate() == flow2->endDate(),
               "Support only Averaging Flows with same observation Period");

    Date futureExpiryDate = flow1->indices().begin()->second->expiryDate();
    for (const auto& [p, ind] : flow1->indices()) {
        QL_REQUIRE(ind->expiryDate() == futureExpiryDate, "MC pricer doesn't support future rolls in averaging");
    }

    futureExpiryDate = flow2->indices().begin()->second->expiryDate();
    for (const auto& [p, ind] : flow2->indices()) {
        QL_REQUIRE(ind->expiryDate() == futureExpiryDate, "MC pricer doesn't support future rolls in averaging");
    }

    vector<double> timeGrid;
    timeGrid.push_back(0.0);
    for (const auto& [pricingDate, index] : flow1->indices()) {
        timeGrid.push_back(vol1->timeFromReference(pricingDate));
    }

    Array drifts;
    size_t nObs = flow1->indices().size();
    constexpr size_t samples = 100000;
    Matrix L(2, 2, 0);
    L[0][0] = 1;
    L[1][0] = rho;
    L[1][1] = std::sqrt(1.0 - rho * rho);

    Matrix drift(2, nObs, 0.0);
    Matrix diffusion(2, nObs, 0.0);
    Matrix St(2, nObs + 1, 0.0);

    auto& [p1, index1] = *flow1->indices().begin();
    auto& [p2, index2] = *flow2->indices().begin();

    St[0][0] = std::log(index1->fixing(p1));
    St[1][0] = std::log(index2->fixing(p2));

    Matrix Z(2, nObs, 0.0);

    for (size_t t = 0; t < nObs; ++t) {
        drift[0][t] =
            -0.5 * vol1->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1]));
        drift[1][t] =
            -0.5 * vol2->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1]));
        diffusion[0][t] =
            std::sqrt(vol1->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1])));
        diffusion[1][t] =
            std::sqrt(vol2->blackForwardVariance(timeGrid[t], timeGrid[t + 1], priceCurve1->price(timeGrid[t + 1])));
    }

    double payoff = 0;

    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(2 * nObs, 42);
    for (size_t i = 0; i < samples; i++) {
        double avg1 = 0;
        double avg2 = 0;
        auto sample = rsg.nextSequence().value;
        std::copy(sample.begin(), sample.end(), Z.begin());
        auto Zt = L * Z;
        for (size_t t = 0; t < nObs; ++t) {
            St[0][t + 1] = St[0][t] + drift[0][t] + diffusion[0][t] * Zt[0][t];
            St[1][t + 1] = St[1][t] + drift[1][t] + diffusion[1][t] * Zt[1][t];
            avg1 += std::exp(St[0][t + 1]);
            avg2 += std::exp(St[1][t + 1]);
        }
        avg1 /= static_cast<double>(nObs);
        avg2 /= static_cast<double>(nObs);

        payoff += std::max(avg1 - avg2 - strike, 0.0);
    }
    payoff /= static_cast<double>(samples);
    return df * payoff;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommoditySpreadOptionTests)

BOOST_AUTO_TEST_CASE(testCrossAssetFutureSpread) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrentQuote = 0.3;
    double volWTIQuote = 0.35;
    double quantity = 1000.;
    double WTIspot = 100.;
    double WTINov = 104.;
    double WTIDec = 105.;
    double brentSpot = 101;
    double brentNov = 103;
    double brentDec = 106;

    Date novExpiry(30, Nov, 2022);
    Date decExpiry(31, Dec, 2022);
    Date exerciseDate(31, Dec, 2022);

    std::vector<Date> futureExpiryDates = {today, novExpiry, decExpiry};
    std::vector<Real> brentQuotes = {brentSpot, brentNov, brentDec};
    std::vector<Real> wtiQuotes = {WTIspot, WTINov, WTIDec};

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));
    auto wtiCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, wtiQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto brentVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrentQuote, Actual365Fixed()));
    auto wtiVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volWTIQuote, Actual365Fixed()));

    auto index1 = QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_USD", decExpiry, NullCalendar(), brentCurve);

    auto index2 = QuantLib::ext::make_shared<CommodityFuturesIndex>("WTI_USD", decExpiry, NullCalendar(), wtiCurve);

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, decExpiry, decExpiry, index1);

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, decExpiry, decExpiry, index2);

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call);

    for (const auto& rho : {-0.95, -0.5, -0.25, 0., 0.5, 0.75, 0.9, 0.95}) {
        Handle<CorrelationTermStructure> corr = Handle<CorrelationTermStructure>(ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 
        auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, brentVol, wtiVol, corr);
        spreadOption.setPricingEngine(engine);
        double npvMC = monteCarloPricing(brentDec, WTIDec, volBrentQuote, volWTIQuote, rho,
                                         discount->timeFromReference(exercise->lastDate()),
                                         discount->discount(exercise->lastDate()), strike) *
                       quantity;
        double npvKrik = spreadOption.NPV();
        BOOST_CHECK_CLOSE(npvKrik, npvMC, 1.);
    }
}

BOOST_AUTO_TEST_CASE(testCalendarSpread) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrent = 0.3;
    double rho = 0.9;
    double quantity = 1000.;
    double spot = 100.;
    double futureNov = 104.;
    Date futureNovExpiry(30, Nov, 2022);
    double futureDec = 105.;
    Date futureDecExpiry(31, Dec, 2022);
    Date exerciseDate(15, Nov, 2022);
    Date paymentDate(17, Nov, 2022);

    std::vector<Date> futureExpiryDates = {today, futureNovExpiry, futureDecExpiry};
    std::vector<Real> brentQuotes = {spot, futureNov, futureDec};

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrent, Actual365Fixed()));

    auto index1 =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_DEC_USD", futureDecExpiry, NullCalendar(), brentCurve);

    auto index2 =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_NOV_USD", futureNovExpiry, NullCalendar(), brentCurve);

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, futureDecExpiry, Date(31, Dec, 2022), index1);

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, futureNovExpiry, Date(30, Nov, 2022), index2);

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call, paymentDate);

    Handle<CorrelationTermStructure> corr =
        Handle<CorrelationTermStructure>(ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 

    auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol1, corr);

    spreadOption.setPricingEngine(engine);

    double kirkNpv = spreadOption.NPV();
    double mcNpv = quantity * monteCarloPricing(futureDec, futureNov, volBrent, volBrent, rho,
                                                discount->timeFromReference(exercise->lastDate()),
                                                discount->discount(paymentDate), strike);

    BOOST_CHECK_CLOSE(kirkNpv, mcNpv, 1);
}

BOOST_AUTO_TEST_CASE(testCalendarSpread2) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrent = 0.3;
    double rho = 0.9;
    double quantity = 1000.;
    double spot = 100.;
    double futureNov = 104.;
    Date futureNovExpiry(30, Nov, 2022);
    double futureDec = 105.;
    Date futureDecExpiry(31, Dec, 2022);
    Date exerciseDate(31, Dec, 2022);
    Date paymentDate = exerciseDate;
    std::vector<Date> futureExpiryDates = {today, futureNovExpiry, futureDecExpiry};
    std::vector<Real> brentQuotes = {spot, futureNov, futureDec};

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrent, Actual365Fixed()));

    auto index1 =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_DEC_USD", futureDecExpiry, NullCalendar(), brentCurve);

    auto index2 =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_NOV_USD", futureNovExpiry, NullCalendar(), brentCurve);

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, futureDecExpiry, Date(31, Dec, 2022), index1);

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, futureNovExpiry, Date(30, Nov, 2022), index2);

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call, paymentDate);

    Handle<CorrelationTermStructure> corr =
        Handle<CorrelationTermStructure>(ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 

    auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol1, corr);

    spreadOption.setPricingEngine(engine);
    double kirkNpv = spreadOption.NPV();

    double volScalingFactor = std::min(
        std::sqrt(discount->timeFromReference(futureNovExpiry) / discount->timeFromReference(exercise->lastDate())),
        1.0);

    double mcNpv = quantity * monteCarloPricing(futureDec, futureNov, volBrent, volBrent * volScalingFactor, rho,
                                                discount->timeFromReference(paymentDate),
                                                discount->discount(exercise->lastDate()), strike);
    BOOST_CHECK_CLOSE(kirkNpv, mcNpv, 1);
}

BOOST_AUTO_TEST_CASE(testCalendarSpreadEdgeCase) {
    // The short asset price is already fixed
    Date today(5, Dec, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrent = 0.3;
    double rho = 0.9;
    double quantity = 1000.;
    double spot = 100.;
    double futureNov = 104.;
    Date futureNovExpiry(30, Nov, 2022);
    double futureDec = 105.;
    Date futureDecExpiry(31, Dec, 2022);
    Date exerciseDate(31, Dec, 2022);

    std::vector<Date> futureExpiryDates = {today, futureDecExpiry};
    std::vector<Real> brentQuotes = {spot, futureDec};

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrent, Actual365Fixed()));

    auto index1 =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_DEC_USD", futureDecExpiry, NullCalendar(), brentCurve);

    auto index2 =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_NOV_USD", futureNovExpiry, NullCalendar(), brentCurve);

    index2->addFixing(futureNovExpiry, futureNov);

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, futureDecExpiry, Date(31, Dec, 2022), index1);

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedCashFlow>(100, futureNovExpiry, Date(30, Nov, 2022), index2);

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call, exerciseDate);

    Handle<CorrelationTermStructure> corr =
        Handle<CorrelationTermStructure>(ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 

    auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol1, corr);

    spreadOption.setPricingEngine(engine);
    double kirkNpv = spreadOption.NPV();

    double bsNpv = quantity * blackFormula(QuantLib::Option::Call, strike + futureNov, futureDec,
                                           std::sqrt(vol1->blackVariance(futureDecExpiry, strike + futureNov)),
                                           discount->discount(exercise->lastDate()));

    BOOST_CHECK_CLOSE(kirkNpv, bsNpv, 1e-8);
}

BOOST_AUTO_TEST_CASE(testSpotAveragingSpreadOption) {
    Date today(31, Oct, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrentQuote = 0.3;
    double volWTIQuote = 0.35;
    double quantity = 1000.;
    double WTIspot = 100.;
    double WTINov = 104.;
    double WTIDec = 105.;
    double brentSpot = 101;
    double brentNov = 103;
    double brentDec = 106;

    Date novExpiry(30, Nov, 2022);
    Date decExpiry(31, Dec, 2022);
    Date exerciseDate(31, Dec, 2022);

    std::vector<Date> futureExpiryDates = {today, novExpiry, decExpiry};
    std::vector<Real> brentQuotes = {brentSpot, brentNov, brentDec};
    std::vector<Real> wtiQuotes = {WTIspot, WTINov, WTIDec};

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));
    auto wtiCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, wtiQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto brentVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrentQuote, Actual365Fixed()));
    auto wtiVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volWTIQuote, Actual365Fixed()));

    auto index1 = QuantLib::ext::make_shared<CommoditySpotIndex>("BRENT_USD", NullCalendar(), brentCurve);

    auto index2 = QuantLib::ext::make_shared<CommoditySpotIndex>("WTI_USD", NullCalendar(), wtiCurve);

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, Date(1, Dec, 2022), Date(31, Dec, 2022),
                                                                     Date(31, Dec, 2022), index1, NullCalendar());

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, Date(1, Dec, 2022), Date(31, Dec, 2022),
                                                                     Date(31, Dec, 2022), index2, NullCalendar());

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call);

    Real df = discount->discount(exercise->lastDate());

    for (const auto& rho : {-0.9, -0.75, -0.5, -0.25, 0., 0.25, 0.5, 0.75, 0.9}) {

        Handle<CorrelationTermStructure> corr = Handle<CorrelationTermStructure>(
            ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 
        auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, brentVol, wtiVol, corr);
        spreadOption.setPricingEngine(engine);
        double npvMC = quantity * monteCarloPricingSpotAveraging(flow1, *brentCurve, *brentVol, flow2, *wtiCurve,
                                                                 *wtiVol, rho, strike, df);
        double npvKirk = spreadOption.NPV();
        BOOST_CHECK_CLOSE(npvKirk, npvMC, 1);
    }
}

BOOST_AUTO_TEST_CASE(testSeasonedSpotAveragingSpreadOption) {
    Date today(10, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrentQuote = 0.3;
    double volWTIQuote = 0.35;
    double quantity = 1000.;
    double WTIspot = 100.;
    double WTINov = 103.;
    double WTIDec = 105.;
    double brentSpot = 100;
    double brentNov = 104;
    double brentDec = 106;

    Date novExpiry(30, Nov, 2022);
    Date decExpiry(31, Dec, 2022);
    Date exerciseDate(30, Nov, 2022);

    std::vector<Date> futureExpiryDates = {today, novExpiry, decExpiry};
    std::vector<Real> brentQuotes = {brentSpot, brentNov, brentDec};
    std::vector<Real> wtiQuotes = {WTIspot, WTINov, WTIDec};

    vector<Date> fixingDates;

    vector<Real> fixingValuesBrent;
    vector<Real> fixingValuesWTI;

    for (int i = 1; i <= 10; ++i) {
        fixingDates.push_back(Date(i, Nov, 2022));
        fixingValuesBrent.push_back(100 + i / 10.);
        fixingValuesWTI.push_back(100 - i / 10.);
    }

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));
    auto wtiCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, wtiQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto brentVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrentQuote, Actual365Fixed()));
    auto wtiVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volWTIQuote, Actual365Fixed()));

    auto index1 = QuantLib::ext::make_shared<CommoditySpotIndex>("BRENT_USD", NullCalendar(), brentCurve);

    auto index2 = QuantLib::ext::make_shared<CommoditySpotIndex>("WTI_USD", NullCalendar(), wtiCurve);

    index1->addFixings(fixingDates.begin(), fixingDates.end(), fixingValuesBrent.begin());
    index2->addFixings(fixingDates.begin(), fixingDates.end(), fixingValuesWTI.begin());

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, Date(1, Nov, 2022), Date(30, Nov, 2022),
                                                                     Date(30, Nov, 2022), index1, NullCalendar());

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, Date(1, Nov, 2022), Date(30, Nov, 2022),
                                                                     Date(30, Nov, 2022), index2, NullCalendar());

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    Real df = discount->discount(exercise->lastDate());

    for (const auto& rho : {-0.9, -0.75, -0.5, -0.25, 0., 0.25, 0.5, 0.75, 0.9}) {
        CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call);

        Handle<CorrelationTermStructure> corr = Handle<CorrelationTermStructure>(
            ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 
        auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, brentVol, wtiVol, corr);
        spreadOption.setPricingEngine(engine);
        double npvMC = quantity * monteCarloPricingSpotAveraging(flow1, *brentCurve, *brentVol, flow2, *wtiCurve,
                                                                 *wtiVol, rho, strike, df);
        double npvKirk = spreadOption.NPV();
        BOOST_CHECK_CLOSE(npvKirk, npvMC, 1);

    }

    for (const auto& rho : {0.85}) {
        for (const auto& strike : {0.5, 1., 1.5, 2.0, 2.5}) {
            CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call);

            Handle<CorrelationTermStructure> corr = Handle<CorrelationTermStructure>(
                ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 
            auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, brentVol, wtiVol, corr);
            spreadOption.setPricingEngine(engine);
            double npvMC = quantity * monteCarloPricingSpotAveraging(flow1, *brentCurve, *brentVol, flow2, *wtiCurve,
                                                                     *wtiVol, rho, strike, df);
            double npvKirk = spreadOption.NPV();
            BOOST_CHECK_CLOSE(npvKirk, npvMC, 1);
        }
    }
}

BOOST_AUTO_TEST_CASE(testFutureAveragingSpreadOption) {
    Date today(31, Oct, 2022);
    Settings::instance().evaluationDate() = today;

    double strike = 1;
    double volBrentQuote = 0.3;
    double volWTIQuote = 0.35;
    double quantity = 1000.;
    double WTIspot = 100.;
    double WTINov = 104.;
    double WTIDec = 105.;
    double brentSpot = 101;
    double brentNov = 103;
    double brentDec = 106;

    Date novExpiry(30, Nov, 2022);
    Date decExpiry(31, Dec, 2022);
    Date exerciseDate(31, Dec, 2022);

    std::vector<Date> futureExpiryDates = {today, novExpiry, decExpiry};
    std::vector<Real> brentQuotes = {brentSpot, brentNov, brentDec};
    std::vector<Real> wtiQuotes = {WTIspot, WTINov, WTIDec};

    auto feCalc = ext::make_shared<MockUpExpiryCalculator>();

    auto brentCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));
    auto wtiCurve = Handle<PriceTermStructure>(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, wtiQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto brentVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volBrentQuote, Actual365Fixed()));
    auto wtiVol = Handle<BlackVolTermStructure>(
        QuantLib::ext::make_shared<BlackConstantVol>(today, NullCalendar(), volWTIQuote, Actual365Fixed()));

    auto index1 = QuantLib::ext::make_shared<CommodityFuturesIndex>("BRENT_USD", novExpiry, NullCalendar(), brentCurve);

    auto index2 = QuantLib::ext::make_shared<CommodityFuturesIndex>("WTI_USD", novExpiry, NullCalendar(), wtiCurve);

    auto flow1 = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, Date(1, Dec, 2022), Date(31, Dec, 2022),
                                                                     Date(31, Dec, 2022), index1, NullCalendar(), 0.0,
                                                                     1.0, true, 0, 0, feCalc);

    auto flow2 = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, Date(1, Dec, 2022), Date(31, Dec, 2022),
                                                                     Date(31, Dec, 2022), index2, NullCalendar(), 0.0,
                                                                     1.0, true, 0, 0, feCalc);

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, quantity, strike, Option::Call);

    Real df = discount->discount(exercise->lastDate());

    for (const auto& rho : {-0.9, -0.75, -0.5, -0.25, 0., 0.25, 0.5, 0.75, 0.9}) {

        Handle<CorrelationTermStructure> corr = Handle<CorrelationTermStructure>(
            ext::make_shared<FlatCorrelation>(0, NullCalendar(), rho, Actual365Fixed())); 
        auto engine = QuantLib::ext::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, brentVol, wtiVol, corr);
        spreadOption.setPricingEngine(engine);
        double npvMC = quantity * monteCarloPricingFutureAveraging(flow1, *brentCurve, *brentVol, flow2, *wtiCurve,
                                                                   *wtiVol, rho, strike, df);
        double npvKirk = spreadOption.NPV();
        BOOST_CHECK_CLOSE(npvKirk, npvMC, 1);
    }
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
