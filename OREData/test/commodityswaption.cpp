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

#include <oret/toplevelfixture.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>
#include <iomanip>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currencies/america.hpp>
#include <ql/exercise.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/schedule.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/instruments/genericswaption.hpp>
#include <qle/pricingengines/commodityswaptionengine.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

BOOST_FIXTURE_TEST_SUITE(OREPlusCommodityTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommoditySwaptionTest)

void run_test(bool averaging) {
    if (averaging)
        BOOST_TEST_MESSAGE("Testing Averaging Commodity Swaption Analytical Approximation vs MC Pricing");
    else
        BOOST_TEST_MESSAGE("Testing Non-Averaging Commodity Swaption Analytical Approximation vs MC Pricing");

    SavedSettings backup;

    Date today(5, Feb, 2019);
    Settings::instance().evaluationDate() = today;
    Calendar cal = UnitedStates(UnitedStates::Settlement);

    // Market - flat price curve
    std::vector<Date> dates = {today + 1 * Years, today + 2 * Years, today + 3 * Years, today + 4 * Years,
                               today + 5 * Years, today + 7 * Years, today + 10 * Years};
    std::vector<Real> prices = {100.0, 105.0, 110.0, 115.0, 120.0, 130.0, 150.0};
    DayCounter dc = Actual365Fixed();
    Handle<QuantExt::PriceTermStructure> priceCurve(
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(today, dates, prices, dc, USDCurrency()));
    priceCurve->enableExtrapolation();

    // Market - flat discount curve
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountCurve(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Annual));

    // Market - flat volatility structure
    Handle<QuantLib::BlackVolTermStructure> vol(QuantLib::ext::make_shared<QuantLib::BlackConstantVol>(today, cal, 0.3, dc));

    // Analytical engine
    Real beta = 0.0;
    QuantLib::ext::shared_ptr<PricingEngine> analyticalEngine =
        QuantLib::ext::make_shared<CommoditySwaptionEngine>(discountCurve, vol, beta);

    // Monte Carlo engine
    Size samples = 10000;
    QuantLib::ext::shared_ptr<PricingEngine> mcEngine =
        QuantLib::ext::make_shared<CommoditySwaptionMonteCarloEngine>(discountCurve, vol, samples, beta);

    QuantLib::ext::shared_ptr<PricingEngine> swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve);

    Real quantity = 1.0;
    std::string name = "CL";
    QuantLib::ext::shared_ptr<CommoditySpotIndex> index = QuantLib::ext::make_shared<CommoditySpotIndex>(name, cal, priceCurve);

    // Swap start times
    std::vector<Size> startTimes = {0, 1, 2, 3, 4, 5, 7, 10};
    std::vector<Real> strikes = {10.0, 60.0, 80.0, 100.0, 120.0, 140.0};

    for (Size k = 0; k < strikes.size(); ++k) {

        Real strikePrice = strikes[k];

        // Vary Swaption start dates, set up underlying Swaps of length one year with 12 monthly calculation periods
        for (Size i = 0; i < startTimes.size(); ++i) {
            Period startTerm = (i == 0 ? 6 * Months : startTimes[i] * Years);
            Date start = today + startTerm;
            Date end = start + 1 * Years;
            Schedule schedule = MakeSchedule().from(start).to(end).withTenor(1 * Months);
            Leg fixedLeg;
            for (Size j = 1; j < schedule.size(); ++j)
                fixedLeg.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(quantity * strikePrice, schedule[j]));
            Leg floatLeg;
            if (averaging)
                floatLeg = CommodityIndexedAverageLeg(schedule, index)
                               .withQuantities(quantity)
                               .withPaymentCalendar(cal)
                               .withPricingCalendar(cal)
                               .withSpreads(0.0);
            else
                floatLeg = CommodityIndexedLeg(schedule, index)
                               .withQuantities(quantity)
                               .withPaymentCalendar(cal)
                               .withPricingLagCalendar(cal)
                               .withSpreads(0.0);

            QuantLib::ext::shared_ptr<QuantLib::Swap> payerSwap = QuantLib::ext::make_shared<QuantLib::Swap>(fixedLeg, floatLeg);
            QuantLib::ext::shared_ptr<QuantLib::Swap> receiverSwap = QuantLib::ext::make_shared<QuantLib::Swap>(floatLeg, fixedLeg);

            payerSwap->setPricingEngine(swapEngine);
            receiverSwap->setPricingEngine(swapEngine);
            BOOST_TEST_MESSAGE("Testing Swap NPV " << payerSwap->NPV() << " " << receiverSwap->NPV());

            QuantLib::ext::shared_ptr<EuropeanExercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(start);
            QuantLib::ext::shared_ptr<QuantExt::GenericSwaption> payerSwaption =
                QuantLib::ext::make_shared<QuantExt::GenericSwaption>(payerSwap, exercise);
            QuantLib::ext::shared_ptr<QuantExt::GenericSwaption> receiverSwaption =
                QuantLib::ext::make_shared<QuantExt::GenericSwaption>(receiverSwap, exercise);

            boost::timer::cpu_timer tan;
            payerSwaption->setPricingEngine(analyticalEngine);
            receiverSwaption->setPricingEngine(analyticalEngine);
            Real anPayerPrice = payerSwaption->NPV();
            Real anReceiverPrice = receiverSwaption->NPV();
            Real anTime = tan.elapsed().wall * 1e-6;

            boost::timer::cpu_timer tmc;
            payerSwaption->setPricingEngine(mcEngine);
            receiverSwaption->setPricingEngine(mcEngine);
            Real mcPayerPrice = payerSwaption->NPV();
            Real mcReceiverPrice = receiverSwaption->NPV();
            Real mcTime = tmc.elapsed().wall * 1e-6;

            Real payerError1 = 100.0 * fabs(mcPayerPrice - anPayerPrice) / mcPayerPrice;
            Real payerError2 = 100.0 * fabs(mcPayerPrice - anPayerPrice) / anPayerPrice;
            Real receiverError1 = 100.0 * fabs(mcReceiverPrice - anReceiverPrice) / mcReceiverPrice;
            Real receiverError2 = 100.0 * fabs(mcReceiverPrice - anReceiverPrice) / anReceiverPrice;

            BOOST_TEST_MESSAGE("Analytical vs MC, Payer Swaption, "
                               << std::fixed << std::setprecision(2) << "strike " << strikePrice << ", expiry "
                               << startTerm << ": "
                               << "an " << anPayerPrice << " mc " << mcPayerPrice << " diff "
                               << anPayerPrice - mcPayerPrice << " rel " << std::max(payerError1, payerError2) << "% "
                               << " underlying " << payerSwap->NPV() << " (" << anTime << " ms, " << mcTime << " ms)");

            BOOST_TEST_MESSAGE(
                "Analytical vs MC, Receiver Swaption, "
                << std::fixed << std::setprecision(2) << "strike " << strikePrice << ", expiry " << startTerm << ": "
                << "an " << anReceiverPrice << " mc " << mcReceiverPrice << " diff "
                << anReceiverPrice - mcReceiverPrice << " rel " << std::max(receiverError1, receiverError2) << "% "
                << " underlying " << receiverSwap->NPV() << " (" << anTime << " ms, " << mcTime << " ms)");

            // Absolute tolerance is generous, and even if the following check is passed:
            // Relative errors for short expiry options are significant, in particular out of the money.
            // => The analytical approximation is rough, consider using the MC engine if performance permits.
            BOOST_CHECK_SMALL(anPayerPrice - mcPayerPrice, 26.0);
            BOOST_CHECK_SMALL(anReceiverPrice - mcReceiverPrice, 26.0);

            Real anPutCallPartiy = anPayerPrice - anReceiverPrice - payerSwap->NPV();
            Real mcPutCallPartiy = mcPayerPrice - mcReceiverPrice - payerSwap->NPV();

            BOOST_TEST_MESSAGE("put/call parity check, "
                               << std::fixed << std::setprecision(2) << strikePrice << " " << startTerm
                               << ": analyticalPayerSwaption - analyticalreceiverSwaption - payerSwap =  "
                               << anPutCallPartiy);
            BOOST_TEST_MESSAGE("put/call parity check, "
                               << std::fixed << std::setprecision(2) << strikePrice << " " << startTerm
                               << ": mcPayerSwaption - mcreceiverSwaption - payerSwap =  " << mcPutCallPartiy);

            // Put/call partiy check tolerances are tight on both cases
            BOOST_CHECK_SMALL(anPutCallPartiy, 0.5);
            BOOST_CHECK_SMALL(mcPutCallPartiy, 4.0);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAveragingCommoditySwaption) {
    // averaging swaption
    run_test(true);
}

BOOST_AUTO_TEST_CASE(testNonAveragingCommoditySwaption) {
    // non-averaging swaption
    run_test(false);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
