/*
Copyright (C) 2019 Quaternion Risk Management Ltd
All rights reserved.
*/

/*
  References:
  - Iain Clark, Commodity Option Pricing, section 2.7
  - Pal Nicolai Henriksen: LOGNORMAL MOMENT MATCHING AND PRICING OF BASKET OPTIONS
    https://pdfs.semanticscholar.org/16ed/c0e804379e22ff36dcbab7e9bb06519faa43.pdf
    This paper shows that the moment matching works well when forward correlation
    is high and forward vols have similar levels, but even then distributions
    deviate in the tails causing discrepancies in options that are deep in or
    out of the money.
*/

#include <oret/toplevelfixture.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>
#include <ql/currencies/america.hpp>
#include <ql/exercise.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <qle/instruments/commodityapo.hpp>
#include <qle/pricingengines/commodityapoengine.hpp>
#include <qle/termstructures/pricecurve.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

typedef struct {
    Real strikePrice;
    QuantLib::Option::Type optionType;
} ApoTestCase;

BOOST_FIXTURE_TEST_SUITE(OREPlusCommodityTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityApoTest)

BOOST_AUTO_TEST_CASE(testCommodityAPO) {

    BOOST_TEST_MESSAGE("Testing Commodity APO Analytical Approximation vs MC Pricing");

    SavedSettings backup;

    Date today(5, Feb, 2019);
    Settings::instance().evaluationDate() = today;
    Calendar cal = UnitedStates(UnitedStates::Settlement);

    // Market - flat price curve
    std::vector<Date> dates = {today + 1 * Years, today + 5 * Years, today + 10 * Years};
    std::vector<Real> prices = {100.0, 100.0, 100.0};
    DayCounter dc = Actual365Fixed();
    Handle<QuantExt::PriceTermStructure> priceCurve(
        boost::make_shared<InterpolatedPriceCurve<Linear>>(today, dates, prices, dc, USDCurrency()));
    priceCurve->enableExtrapolation();

    // Market - flat discount curve
    Handle<Quote> rateQuote(boost::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountCurve(boost::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Annual));

    // Market - flat volatility structure
    Handle<QuantLib::BlackVolTermStructure> vol(boost::make_shared<QuantLib::BlackConstantVol>(today, cal, 0.3, dc));

    // Analytical engine
    Real beta = 0.0;
    boost::shared_ptr<PricingEngine> analyticalEngine =
        boost::make_shared<CommodityAveragePriceOptionAnalyticalEngine>(discountCurve, vol, beta);

    // Monte Carlo engine
    Size samples = 10000;
    boost::shared_ptr<PricingEngine> mcEngine =
        boost::make_shared<CommodityAveragePriceOptionMonteCarloEngine>(discountCurve, vol, samples, beta);

    // Instrument
    Real quantity = 1.0;
    std::string name = "CL";
    Period term = 1 * Months;

    // std::vector<ApoTestCase> cases = {{100.0, Option::Call}, {100.0, Option::Put}};
    std::vector<ApoTestCase> cases = {{100.0, Option::Call}, {120.0, Option::Call}, {80.0, Option::Call},
                                      {100.0, Option::Put},  {120.0, Option::Put},  {80.0, Option::Put}};

    for (Size k = 0; k < cases.size(); ++k) {

        Real strikePrice = cases[k].strikePrice;
        QuantLib::Option::Type optionType = cases[k].optionType;
        std::string optionTypeString = (optionType == Option::Call ? "Call" : "Put");

        // Vary APO start dates
        for (Size i = 1; i <= 10; ++i) {

            // Instrument
            Period startTerm = i * Years;
            Date startDate = today + startTerm;
            Date endDate = startDate + term;
            Date payDate = endDate;
            boost::shared_ptr<CommoditySpotIndex> index = boost::make_shared<CommoditySpotIndex>(name, cal, priceCurve);
            boost::shared_ptr<CommodityIndexedAverageCashFlow> flow =
                boost::make_shared<CommodityIndexedAverageCashFlow>(quantity, startDate, endDate, payDate, index);
            boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(endDate);

            boost::shared_ptr<CommodityAveragePriceOption> apo =
                boost::make_shared<CommodityAveragePriceOption>(flow, exercise, quantity, strikePrice, optionType);

            boost::timer::cpu_timer tan;
            apo->setPricingEngine(analyticalEngine);
            Real anPrice = apo->NPV();
            tan.stop();
            Real anTime = tan.elapsed().wall * 1e-6;

            boost::timer::cpu_timer tmc;
            apo->setPricingEngine(mcEngine);
            Real mcPrice = apo->NPV();
            Real mcTime = tmc.elapsed().wall * 1e-6;

            Real error1 = 100.0 * fabs(mcPrice - anPrice) / mcPrice;
            Real error2 = 100.0 * fabs(mcPrice - anPrice) / anPrice;

            BOOST_TEST_MESSAGE(optionTypeString << " " << std::fixed << std::setprecision(2) << strikePrice << " "
                                                << startTerm << " Analytical vs MC price: " << std::fixed
                                                << std::setprecision(2) << anPrice << " vs " << mcPrice << " diff "
                                                << anPrice - mcPrice << " error " << std::max(error1, error2) << "% ("
                                                << anTime << " ms, " << mcTime << " ms)");
            BOOST_CHECK_CLOSE(anPrice, mcPrice, 1.0); // accept 1% relative error
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
