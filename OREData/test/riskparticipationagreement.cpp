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

#include <boost/test/unit_test.hpp>

#include <oret/toplevelfixture.hpp>

#include <ored/scripting/engines/analyticblackriskparticipationagreementengine.hpp>
#include <ored/scripting/engines/numericlgmriskparticipationagreementengine.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/exercise.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/nonstandardswap.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/timegrid.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace boost::accumulators;

using namespace boost::unit_test_framework;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(RiskParticipationAgreementTests)

namespace {
void runTest(const std::vector<Real>& nominals, const bool isPayer, const Real errorTol,
             const std::vector<Real> cachedSimResults = {}) {

    Date today(6, Jun, 2019);
    Settings::instance().evaluationDate() = today;

    auto dsc = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, ActualActual(ActualActual::ISDA)));
    auto fwd = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.02, ActualActual(ActualActual::ISDA)));
    auto def =
        Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<FlatHazardRate>(today, 0.0050, ActualActual(ActualActual::ISDA)));
    auto blackVol = Handle<SwaptionVolatilityStructure>(
        QuantLib::ext::make_shared<ConstantSwaptionVolatility>(today, TARGET(), Following, 0.0050, ActualActual(ActualActual::ISDA), Normal));

    auto swapIndexBase = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, fwd, dsc);

    // create swap

    std::vector<Real> fixedNominals(nominals);
    fixedNominals.resize(20, nominals.back());
    std::vector<Real> fixedRates(20, 0.03);
    std::vector<Real> floatNominals;
    for (auto const& n : fixedNominals) {
        floatNominals.push_back(n);
        floatNominals.push_back(n);
    }

    QuantLib::ext::shared_ptr<VanillaSwap> tmp = MakeVanillaSwap(20 * Years, swapIndexBase->iborIndex(), 0.03);
    QuantLib::ext::shared_ptr<Swap> underlying = QuantLib::ext::make_shared<NonstandardSwap>(
        isPayer ? VanillaSwap::Payer : VanillaSwap::Receiver, fixedNominals, floatNominals, tmp->fixedSchedule(),
        fixedRates, tmp->fixedDayCount(), tmp->floatingSchedule(), tmp->iborIndex(), std::vector<Real>(40, 1.0),
        std::vector<Real>(40, 0.0), tmp->floatingDayCount());

    underlying->setPricingEngine(QuantLib::ext::make_shared<DiscountingSwapEngine>(dsc));
    BOOST_TEST_MESSAGE("Underlying NPV = " << underlying->NPV());

    // create RPA contract

    Real participationRate = 0.8;
    Real recoveryRate = 0.2;
    Date feePayDate = today + 20;
    Leg fee;
    fee.push_back(
        QuantLib::ext::make_shared<FixedRateCoupon>(feePayDate, nominals.front(), 0.02, ActualActual(ActualActual::ISDA), today, feePayDate));

    QuantLib::ext::shared_ptr<RiskParticipationAgreement> rpa = QuantLib::ext::make_shared<RiskParticipationAgreement>(
        std::vector<Leg>{underlying->leg(0), underlying->leg(1)}, std::vector<bool>{isPayer, !isPayer},
        std::vector<std::string>{"EUR", "EUR"}, std::vector<Leg>{fee}, true, std::vector<std::string>{"EUR"},
        participationRate, today, underlying->maturityDate(), true, recoveryRate);

    // create LGM model and RPA LGM pricing engine

    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> basket;
    std::vector<Date> expiryDates;
    for (Size i = 1; i < 20; ++i) {
        QuantLib::ext::shared_ptr<BlackCalibrationHelper> helper = QuantLib::ext::make_shared<SwaptionHelper>(
            i * Years, (20 - i) * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0050)),
            swapIndexBase->iborIndex(), 1 * Years, swapIndexBase->dayCounter(),
            swapIndexBase->iborIndex()->dayCounter(), dsc, BlackCalibrationHelper::RelativePriceError,
            fixedRates.front(), 1.0, Normal);
        basket.push_back(helper);
        expiryDates.push_back(
            QuantLib::ext::static_pointer_cast<SwaptionHelper>(helper)->swaption()->exercise()->dates().back());
    }
    std::vector<Date> stepDates(expiryDates.begin(), expiryDates.end() - 1);
    Array stepTimes(stepDates.size());
    for (Size i = 0; i < stepDates.size(); ++i) {
        stepTimes[i] = dsc->timeFromReference(stepDates[i]);
    }
    auto lgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), dsc, stepTimes, Array(stepTimes.size() + 1, 0.0050), stepTimes,
        Array(stepTimes.size() + 1, 0.0));
    lgm_p->shift() = -lgm_p->H(20.0);
    auto lgm = QuantLib::ext::make_shared<LGM>(lgm_p);

    auto rpaEngine = QuantLib::ext::make_shared<NumericLgmRiskParticipationAgreementEngine>(
        "EUR", std::map<std::string, Handle<YieldTermStructure>>{{"EUR", dsc}}, std::map<std::string, Handle<Quote>>(),
        lgm, 3.0, 10, 3.0, 10, def, Handle<Quote>());

    // set engine

    rpa->setPricingEngine(rpaEngine);

    // extract results from RPA pricing engine

    boost::timer::cpu_timer timer;
    std::vector<Date> gridDates = rpa->result<std::vector<Date>>("GridDates");
    std::vector<Real> epe_engine = rpa->result<std::vector<Real>>("OptionNpvs");
    timer.stop();
    BOOST_TEST_MESSAGE("EPE calculation in numeric lgm engine took " << timer.elapsed().wall * 1e-6 << "ms");

    // generate eval dates from grid dates (midpoint rule) and check them vs. cached results

    std::vector<Date> evalDates;
    for (Size i = 0; i < gridDates.size() - 1; ++i) {
        evalDates.push_back(gridDates[i] + (gridDates[i + 1] - gridDates[i]) / 2);
    }

    std::vector<Date> expectedEvalDates{
        Date(7, September, 2019),  Date(10, March, 2020), Date(9, September, 2020),  Date(11, March, 2021),
        Date(9, September, 2021),  Date(11, March, 2022), Date(10, September, 2022), Date(13, March, 2023),
        Date(11, September, 2023), Date(11, March, 2024), Date(9, September, 2024),  Date(11, March, 2025),
        Date(9, September, 2025),  Date(11, March, 2026), Date(9, September, 2026),  Date(11, March, 2027),
        Date(9, September, 2027),  Date(11, March, 2028), Date(11, September, 2028), Date(12, March, 2029),
        Date(10, September, 2029), Date(11, March, 2030), Date(9, September, 2030),  Date(11, March, 2031),
        Date(9, September, 2031),  Date(10, March, 2032), Date(9, September, 2032),  Date(11, March, 2033),
        Date(10, September, 2033), Date(13, March, 2034), Date(11, September, 2034), Date(12, March, 2035),
        Date(10, September, 2035), Date(10, March, 2036), Date(9, September, 2036),  Date(11, March, 2037),
        Date(9, September, 2037),  Date(11, March, 2038), Date(9, September, 2038),  Date(11, March, 2039)};

    BOOST_REQUIRE(evalDates.size() == expectedEvalDates.size());
    for (Size i = 0; i < evalDates.size(); ++i)
        BOOST_REQUIRE_EQUAL(evalDates[i], expectedEvalDates[i]);

    // generate EPE with full simulation

    std::vector<Real> evalTimes;
    for (Size i = 0; i < evalDates.size(); ++i) {
        evalTimes.push_back(dsc->timeFromReference(evalDates[i]));
    }
    Size nTimes = evalDates.size();
    std::vector<Real> epe_sim(nTimes);

    if (cachedSimResults.empty()) {
        Size nPaths = 10000;
        TimeGrid grid(evalTimes.begin(), evalTimes.end());

        auto swaptionEngineLgm = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(lgm);
        for (Size i = 0; i < basket.size(); ++i) {
            basket[i]->setPricingEngine(swaptionEngineLgm);
        }
        LevenbergMarquardt lm(1E-8, 1E-8, 1E-8);
        EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);
        lgm->calibrateVolatilitiesIterative(basket, lm, ec);

        MultiPathGeneratorSobolBrownianBridge pgen(lgm->stateProcess(), grid);

        auto lgm_dsc = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(lgm, dsc);
        auto lgm_fwd = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(lgm, fwd);

        auto lgmEuribor = swapIndexBase->iborIndex()->clone(Handle<YieldTermStructure>(lgm_fwd));

        std::vector<Leg> lgmLinkedUnderlying(2);
        std::set<Date> requiredFixings;
        for (Size i = 0; i < 2; ++i) {
            for (auto const& c : underlying->leg(i)) {
                auto f = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c);
                if (f) {
                    requiredFixings.insert(f->fixingDate());
                    auto ic = QuantLib::ext::make_shared<IborCoupon>(
                        f->date(), f->nominal(), f->accrualStartDate(), f->accrualEndDate(), f->fixingDays(),
                        lgmEuribor, f->gearing(), f->spread(), f->referencePeriodStart(), f->referencePeriodEnd(),
                        f->dayCounter(), f->isInArrears());
                    ic->setPricer(QuantLib::ext::make_shared<BlackIborCouponPricer>());
                    lgmLinkedUnderlying[i].push_back(ic);
                } else
                    lgmLinkedUnderlying[i].push_back(c);
            }
        }

        Swap lgmUnderlying(lgmLinkedUnderlying, {isPayer, !isPayer});

        timer.start();
        auto lgmSwapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(Handle<YieldTermStructure>(lgm_dsc));
        lgmUnderlying.setPricingEngine(lgmSwapEngine);
        std::vector<accumulator_set<double, stats<tag::mean>>> acc(nTimes);
        for (Size p = 0; p < nPaths; ++p) {
            auto currentFixing = requiredFixings.begin();
            MultiPath path = pgen.next().value;
            for (Size i = 0; i < nTimes; ++i) {
                lgm_dsc->move(evalDates[i], path[0][i + 1]);
                lgm_fwd->move(evalDates[i], path[0][i + 1]);
                Settings::instance().evaluationDate() = evalDates[i];
                while (currentFixing != requiredFixings.end() && evalDates[i] >= *currentFixing) {
                    Date evalDateAdj =
                        lgmEuribor->fixingCalendar().adjust(evalDates[i]); // for today's fixing generation
                    lgmEuribor->addFixing(*currentFixing, lgmEuribor->fixing(evalDateAdj), false);
                    ++currentFixing;
                }
                acc[i](std::max(lgmUnderlying.NPV(), 0.0) / lgm->numeraire(evalTimes[i], path[0][i + 1]));
            }
            Settings::instance().evaluationDate() = today;
            IndexManager::instance().clearHistory(lgmEuribor->name());
        }
        for (Size i = 0; i < nTimes; ++i) {
            epe_sim[i] = mean(acc[i]);
            BOOST_TEST_MESSAGE(epe_sim[i] << (i < nTimes - 1 ? "," : ""));
        }
        timer.stop();
        BOOST_TEST_MESSAGE("EPE calculation via full simulation took " << timer.elapsed().wall * 1e-6 << "ms");
    } else {
        BOOST_REQUIRE(cachedSimResults.size() == nTimes);
        for (Size i = 0; i < nTimes; ++i)
            epe_sim[i] = cachedSimResults[i];
    }

    // compare results

    Real maxError = 0.0;
    BOOST_TEST_MESSAGE("date t EPE_engine EPE_sim");
    for (Size i = 0; i < nTimes; ++i) {
        BOOST_TEST_MESSAGE(QuantLib::io::iso_date(evalDates[i])
                           << " " << evalTimes[i] << " " << epe_engine[i] << " " << epe_sim[i]);
        BOOST_CHECK_SMALL(epe_engine[i] - epe_sim[i], errorTol);
        maxError = std::max(maxError, std::abs(epe_sim[i] - epe_engine[i]));
    }
    BOOST_TEST_MESSAGE("max error = " << maxError);

    // check total npv of rpa

    Real npv = rpa->NPV();
    BOOST_TEST_MESSAGE("RPA total NPV (LGM engine) = " << npv);

    Real protNpv = 0.0;
    for (Size i = 0; i < gridDates.size() - 1; ++i) {
        protNpv += epe_engine[i] * def->defaultProbability(gridDates[i], gridDates[i + 1]);
    }
    protNpv *= participationRate * (1.0 - recoveryRate);

    Real feeNpv = 0.0;
    for (auto const& c : fee) {
        // fee contribution
        feeNpv += c->amount() * dsc->discount(c->date()) * def->survivalProbability(c->date());
        // fee accruals
        auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c);
        if (cpn) {
            Date start = std::max(cpn->accrualStartDate(), today);
            Date end = cpn->accrualEndDate();
            if (start < end) {
                Date mid = start + (end - start) / 2;
                feeNpv += cpn->accruedAmount(mid) * dsc->discount(mid) * def->defaultProbability(start, end);
            }
        }
    }

    BOOST_TEST_MESSAGE("Expected NPV = " << protNpv - feeNpv);
    BOOST_CHECK_CLOSE(npv, protNpv - feeNpv, 1E-8);

    // check npv in black engine

    auto blackEngine = QuantLib::ext::make_shared<AnalyticBlackRiskParticipationAgreementEngine>(
        "EUR", std::map<std::string, Handle<YieldTermStructure>>{{"EUR", dsc}}, std::map<std::string, Handle<Quote>>(),
        def, Handle<Quote>(), blackVol, swapIndexBase, false, 0.0, false);
    rpa->setPricingEngine(blackEngine);
    Real blackNpv = rpa->NPV();

    BOOST_TEST_MESSAGE("Black NPV = " << blackNpv);
    BOOST_CHECK_CLOSE(npv, blackNpv, 5.0);
}
} // namespace

BOOST_AUTO_TEST_CASE(testStandardPayerSwap) {
    runTest({10000.0}, true, 10.0,
            {0,       1.94889, 15.7715, 24.0619, 54.8255, 55.7279, 95.6096, 91.7498, 133.741, 123.373,
             164.17,  147.664, 189.878, 166.109, 207.458, 179.875, 220.102, 187.612, 226.417, 190.731,
             227.738, 188.212, 222.535, 180.287, 214.038, 169.203, 200.895, 154.238, 183.085, 133.985,
             161.588, 111.699, 135.997, 85.3633, 107.052, 56.2545, 74.7086, 25.7101, 38.9068, 1.31757});
}

BOOST_AUTO_TEST_CASE(testStandardReceiverSwap) {
    runTest({10000.0}, false, 10.0,
            {1776.41, 1878.45, 1694.92, 1802.31, 1637.38, 1736.49, 1582.74, 1677.57, 1526.22, 1611.41,
             1463.59, 1542.34, 1396.47, 1467.08, 1322.19, 1388.18, 1244.17, 1303.92, 1159.84, 1214.88,
             1072.06, 1122.67, 978.803, 1026.29, 882.843, 926.86,  783.296, 824.455, 679.52,  718.399,
             572.469, 608.334, 463.113, 497.353, 351.281, 384.799, 236.4,   271.088, 119.076, 164.356});
}

BOOST_AUTO_TEST_CASE(testAmortisingPayerSwap) {
    runTest({10000.0, 9500.0, 9000.0, 8500.0, 8000.0, 7500.0, 7000.0, 6500.0, 6000.0, 5500.0, 5000.0,
             4500.0,  4000.0, 3500.0, 3000.0, 2500.0, 2000.0, 1500.0, 1000.0, 500.0,  0.0},
            true, 5.0, {0,       0.58703, 8.11856, 8.93971, 26.7995, 20.7611, 44.169,  33.6139, 58.2056, 43.9656,
                        67.223,  49.9792, 72.8682, 52.7195, 74.1339, 53.0533, 72.9152, 51.5704, 69.0406, 47.9078,
                        63.6394, 42.9462, 56.4375, 36.8842, 48.762,  30.607,  40.6079, 24.2613, 32.3475, 17.8779,
                        24.4077, 12.3198, 17.092,  7.4161,  10.7417, 3.56567, 5.61611, 1.08903, 1.94534, 0.0658784});
}

BOOST_AUTO_TEST_CASE(testAmortisingReceiverSwap) {
    runTest({10000.0, 9500.0, 9000.0, 8500.0, 8000.0, 7500.0, 7000.0, 6500.0, 6000.0, 5500.0, 5000.0,
             4500.0,  4000.0, 3500.0, 3000.0, 2500.0, 2000.0, 1500.0, 1000.0, 500.0,  0.0},
            false, 50.0, {962.21,  1062.92, 873.019, 967.955, 799.857, 882.248, 731.282, 804.481, 664.708, 726.901,
                          599.229, 653.365, 535.172, 581.061, 472.101, 511.799, 411.877, 445.908, 353.651, 382.434,
                          299.225, 323.595, 247.998, 268.821, 200.983, 218.353, 158.261, 172.636, 119.932, 131.857,
                          86.3659, 95.7154, 58.1326, 65.4286, 35.2337, 40.7063, 17.7519, 21.5932, 5.95378, 8.21779});
}

BOOST_AUTO_TEST_CASE(testAccretingPayerSwap) {
    runTest({10000.0, 11000.0, 12000.0, 13000.0, 14000.0, 15000.0, 16000.0, 17000.0, 18000.0, 19000.0,
             20000.0, 21000.0, 22000.0, 23000.0, 24000.0, 25000.0, 26000.0, 27000.0, 28000.0, 29000.0},
            true, 20.0, {0,       5.03316, 31.0906, 56.0286, 110.937, 128.441, 198.589, 210.596, 284.946, 284.878,
                         358.21,  345.452, 424.04,  395.573, 474.253, 435.555, 514.612, 461.857, 541.256, 478.229,
                         556.038, 480.233, 554.782, 468.493, 544.637, 447.635, 521.492, 415.265, 484.579, 367.228,
                         435.953, 311.242, 373.81,  241.924, 299.673, 162.093, 212.894, 75.2118, 112.83,  3.82094});
}

BOOST_AUTO_TEST_CASE(testAccretingReceiverSwap) {
    runTest({10000.0, 11000.0, 12000.0, 13000.0, 14000.0, 15000.0, 16000.0, 17000.0, 18000.0, 19000.0,
             20000.0, 21000.0, 22000.0, 23000.0, 24000.0, 25000.0, 26000.0, 27000.0, 28000.0, 29000.0},
            false, 20.0, {3404.82, 3509.87, 3338.75, 3472.73, 3312.48, 3447.74, 3285.74, 3426.31, 3249.38, 3383.13,
                          3192.44, 3322.71, 3119.21, 3241.8,  3022.51, 3142.98, 2908.89, 3022.1,  2772.3,  2881.64,
                          2617.82, 2722.29, 2440.46, 2542.63, 2246.61, 2345.11, 2033.39, 2129.17, 1798.72, 1892.51,
                          1544.68, 1634.35, 1273.08, 1361.87, 983.376, 1073.45, 673.695, 770.339, 345.319, 476.632});
}

namespace {
Real computeUnderlyingNpv(const bool underlyingIsPayer, const Real cap, const Real floor, const bool nakedOption) {
    Date today(6, Jun, 2019);
    Settings::instance().evaluationDate() = today;

    auto dsc = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, ActualActual(ActualActual::ISDA)));
    auto fwd = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.02, ActualActual(ActualActual::ISDA)));
    auto def =
        Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<FlatHazardRate>(today, 0.0050, ActualActual(ActualActual::ISDA)));

    auto iborIndex = QuantLib::ext::make_shared<Euribor>(6 * Months, fwd);

    // create rpas

    Schedule schedule(today + 2 * Days, (today + 2 * Days) + 10 * Years, 6 * Months, NullCalendar(), Unadjusted,
                      Unadjusted, DateGeneration::Forward, false);
    IborLeg l(schedule, iborIndex);
    l.withNotionals(10000.0);

    if (cap != Null<Real>())
        l.withCaps(cap);
    if (floor != Null<Real>())
        l.withFloors(floor);

    Leg leg;
    if (nakedOption)
        leg = StrippedCappedFlooredCouponLeg(l);
    else
        leg = l;

    auto rpa = QuantLib::ext::make_shared<RiskParticipationAgreement>(
        std::vector<Leg>{leg}, std::vector<bool>{underlyingIsPayer}, std::vector<std::string>{"EUR"},
        std::vector<Leg>{}, false, std::vector<std::string>{}, 0.0, schedule.dates().front(), schedule.dates().back(),
        true, 0.0);

    // create lgm engine

    auto lgm =
        QuantLib::ext::make_shared<LGM>(QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), dsc, 0.0040, 0.01));
    auto engine = QuantLib::ext::make_shared<NumericLgmRiskParticipationAgreementEngine>(
        "EUR", std::map<std::string, Handle<YieldTermStructure>>{{"EUR", dsc}}, std::map<std::string, Handle<Quote>>(),
        lgm, 3.0, 10, 3.0, 10, def, Handle<Quote>());

    // compute underlying npv and return it

    rpa->setPricingEngine(engine);
    return rpa->result<Real>("UnderlyingNpv");
}
} // namespace

BOOST_AUTO_TEST_CASE(testCapFloors) {

    constexpr Real tol = 1E-10;

    // underlying is receiver

    // no cap/floor
    Real plain = computeUnderlyingNpv(false, Null<Real>(), Null<Real>(), false);

    // capped/floored coupon
    Real capped = computeUnderlyingNpv(false, 0.03, Null<Real>(), false);
    Real floored = computeUnderlyingNpv(false, Null<Real>(), 0.01, false);
    Real collared = computeUnderlyingNpv(false, 0.03, 0.01, false);

    // the embedded option
    Real cap = computeUnderlyingNpv(false, 0.03, Null<Real>(), true);
    Real floor = computeUnderlyingNpv(false, Null<Real>(), 0.01, true);
    Real collar = computeUnderlyingNpv(false, 0.03, 0.01, true);

    BOOST_CHECK_CLOSE(capped + cap, plain, tol);
    BOOST_CHECK_CLOSE(floored - floor, plain, tol);
    BOOST_CHECK_CLOSE(collared - collar, plain, tol);

    // underlying is payer

    // no cap/floor
    Real plain2 = computeUnderlyingNpv(true, Null<Real>(), Null<Real>(), false);

    // capped/floored coupon
    Real capped2 = computeUnderlyingNpv(true, 0.03, Null<Real>(), false);
    Real floored2 = computeUnderlyingNpv(true, Null<Real>(), 0.01, false);
    Real collared2 = computeUnderlyingNpv(true, 0.03, 0.01, false);

    // the embedded option
    Real cap2 = computeUnderlyingNpv(true, 0.03, Null<Real>(), true);
    Real floor2 = computeUnderlyingNpv(true, Null<Real>(), 0.01, true);
    Real collar2 = computeUnderlyingNpv(true, 0.03, 0.01, true);

    BOOST_CHECK_CLOSE(capped2 + cap2, plain2, tol);
    BOOST_CHECK_CLOSE(floored2 - floor2, plain2, tol);
    BOOST_CHECK_CLOSE(collared2 - collar2, plain2, tol);

    // check sign changes between underlying receiver and payer
    BOOST_CHECK_CLOSE(plain, -plain2, tol);
    BOOST_CHECK_CLOSE(capped, -capped2, tol);
    BOOST_CHECK_CLOSE(floored, -floored2, tol);
    BOOST_CHECK_CLOSE(collared, -collared2, tol);
    BOOST_CHECK_CLOSE(cap, -cap2, tol);
    BOOST_CHECK_CLOSE(floor, -floor2, tol);
    BOOST_CHECK_CLOSE(collar, -collar2, tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
