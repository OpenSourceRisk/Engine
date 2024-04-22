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

#include <qle/models/representativeswaption.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/exercise.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/nonstandardswap.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
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
using namespace boost::accumulators;

using namespace boost::unit_test_framework;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(RepresentativeSwaptionTest)

BOOST_AUTO_TEST_CASE(testStandardUnderlying) {

    Date today(6, Jun, 2019);
    Settings::instance().evaluationDate() = today;

    auto dsc = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, ActualActual(ActualActual::ISDA)));
    auto fwd = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.02, ActualActual(ActualActual::ISDA)));

    auto swapIndexBase = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, fwd, dsc);

    for (Size i = 1; i <= 19; ++i) {

        Date exerciseDate = TARGET().advance(today, i * Years);
        Date valueDate = swapIndexBase->iborIndex()->valueDate(exerciseDate);
        VanillaSwap underlying = MakeVanillaSwap((20 - i) * Years, swapIndexBase->iborIndex(), 0.03)
                                     .withNominal(10000.0)
                                     .withEffectiveDate(valueDate);

        RepresentativeSwaptionMatcher matcher({underlying.leg(0), underlying.leg(1)}, {true, false}, swapIndexBase,
                                              false, dsc, 0.01);

        static constexpr Real tol = 1E-6;

        auto w = matcher.representativeSwaption(
            exerciseDate, RepresentativeSwaptionMatcher::InclusionCriterion::AccrualStartGeqExercise);

        BOOST_REQUIRE_EQUAL(w->exercise()->dates().size(), 1);
        BOOST_CHECK_EQUAL(w->exercise()->date(0), exerciseDate);
        auto s = w->underlyingSwap();
        BOOST_CHECK_EQUAL(s->type(), VanillaSwap::Payer);
        BOOST_CHECK_CLOSE(s->nominal(), 10000.0, tol);
        BOOST_CHECK_CLOSE(s->fixedRate(), 0.03, tol);
        BOOST_REQUIRE(!s->fixedSchedule().dates().empty());
        BOOST_CHECK_EQUAL(s->fixedSchedule().dates().front(), valueDate);
        BOOST_CHECK_EQUAL(s->fixedSchedule().dates().back(), underlying.maturityDate());
        BOOST_REQUIRE(!s->floatingSchedule().dates().empty());
        BOOST_CHECK_EQUAL(s->floatingSchedule().dates().front(), valueDate);
        BOOST_CHECK_EQUAL(s->floatingSchedule().dates().back(), underlying.maturityDate());
        BOOST_CHECK_EQUAL(s->iborIndex()->name(), swapIndexBase->iborIndex()->name());
        BOOST_CHECK_CLOSE(s->spread(), 0.0, tol);
    }
}

namespace {
void runTest(const std::vector<Real>& nominals, const bool isPayer, const Real errorTol,
             const std::vector<Real> cachedSimResults = {}) {

    Date today(6, Jun, 2019);
    Settings::instance().evaluationDate() = today;

    auto dsc = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, ActualActual(ActualActual::ISDA)));
    auto fwd = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.02, ActualActual(ActualActual::ISDA)));
    auto blackVol = Handle<SwaptionVolatilityStructure>(
        QuantLib::ext::make_shared<ConstantSwaptionVolatility>(today, TARGET(), Following, 0.0050, ActualActual(ActualActual::ISDA), Normal));
    auto blackEngine = QuantLib::ext::make_shared<BachelierSwaptionEngine>(dsc, blackVol);

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

    // set evaluation dates / times

    std::vector<Date> evalDates;
    std::vector<Real> evalTimes;
    for (Size i = 1; i < 250; ++i) {
        evalDates.push_back(today + i * Months);
        evalTimes.push_back(dsc->timeFromReference(evalDates.back()));
    }
    Size nTimes = evalDates.size();
    std::vector<Real> epe_repr(nTimes), epe_sim(nTimes);

    // generate EPE by pricing representative swaptions

    boost::timer::cpu_timer timer;
    for (Size i = 0; i < nTimes; ++i) {
        evalTimes[i] = dsc->timeFromReference(evalDates[i]);

        RepresentativeSwaptionMatcher matcher({underlying->leg(0), underlying->leg(1)}, {isPayer, !isPayer},
                                              swapIndexBase, false, dsc, 0.0);

        Real npv = 0.0;
        auto w = matcher.representativeSwaption(evalDates[i],
                                                RepresentativeSwaptionMatcher::InclusionCriterion::PayDateGtExercise);
        if (w) {
            w->setPricingEngine(blackEngine);
            npv = w->NPV();
        }
        epe_repr[i] = npv;
    }
    timer.stop();
    BOOST_TEST_MESSAGE("EPE calculation via representative swaptions took " << timer.elapsed().wall * 1e-6 << "ms");

    // generate EPE with full simulation (reversion = 0, so LGM vol = black vol)

    if (cachedSimResults.empty()) {
        Size nPaths = 10000;
        TimeGrid grid(evalTimes.begin(), evalTimes.end());

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

    BOOST_TEST_MESSAGE("date t EPE_repr EPE_sim");
    for (Size i = 0; i < nTimes; ++i) {
        BOOST_TEST_MESSAGE(QuantLib::io::iso_date(evalDates[i])
                           << " " << evalTimes[i] << " " << epe_repr[i] << " " << epe_sim[i]);
        BOOST_CHECK_SMALL(epe_repr[i] - epe_sim[i], errorTol);
    }
}
} // namespace

BOOST_AUTO_TEST_CASE(testEPEStandardPayerSwap) {
    runTest({10000.0}, true, 5.0,
            {0,       0,       0,       0.00476312, 0.141555, 0.229646, 0.285095, 0.750794, 1.43698, 2.23473, 3.12387,
             4.08822, 10.1648, 11.9295, 15.377,     18.4359,  21.1733,  23.829,   18.5224,  21.1229, 24.0598, 26.4625,
             28.777,  31.1131, 46.7003, 50.1594,    53.5407,  57.2505,  60.4863,  64.6806,  51.473,  54.8708, 57.7915,
             60.302,  62.9418, 65.1625, 86.3816,    89.7665,  93.488,   97.5451,  101.403,  105.314, 84.7002, 88.0465,
             90.626,  94.0729, 97.6903, 100.334,    125.85,   129.309,  131.478,  133.738,  139.173, 143.482, 116.766,
             119.195, 121.608, 124.51,  127.738,    130.716,  157.044,  160.824,  163.489,  166.715, 169.565, 172.198,
             140.408, 143.187, 145.431, 148.512,    151.258,  154.147,  182.893,  186.55,   188.204, 190.143, 193.236,
             196.31,  161.458, 163.681, 165.816,    167.875,  170.316,  172.51,   201.745,  204.853, 206.583, 208.683,
             211.533, 213.877, 174.826, 177.235,    178.714,  180.611,  183.115,  185.117,  214.554, 216.786, 218.909,
             221.963, 223.604, 225.513, 182.987,    184.414,  186.651,  188.521,  190.39,   192.539, 221.357, 223.192,
             225.106, 226.645, 227.932, 230.81,     186.467,  188.049,  189.68,   191.836,  193.588, 195.276, 224.057,
             225.636, 226.516, 228.609, 230.082,    231.878,  184.733,  186.123,  187.525,  189.293, 190.731, 191.882,
             219.822, 221.445, 222.857, 223.791,    224.945,  226.246,  177.87,   179.055,  180.006, 181.366, 182.334,
             183.467, 209.987, 211.417, 212.994,    214.195,  215.049,  216.597,  166.943,  168.152, 169.01,  169.767,
             170.732, 171.77,  198.04,  199.156,    200.184,  201.36,   201.983,  202.727,  151.235, 152.127, 152.92,
             153.78,  154.52,  155.813, 181.015,    181.805,  182.547,  183.035,  183.925,  184.402, 131.595, 132.355,
             133.101, 134.118, 134.54,  134.86,     159.379,  160.045,  160.721,  161.29,   161.831, 162.648, 110.028,
             110.509, 110.892, 111.467, 111.937,    112.641,  134.687,  135.165,  135.463,  136.04,  136.535, 136.947,
             84.0783, 84.501,  84.9166, 85.1605,    85.5312,  85.9006,  105.707,  106.004,  106.191, 106.522, 106.961,
             107.167, 55.3124, 55.482,  55.6544,    55.9519,  56.2398,  56.4192,  73.8899,  74.0573, 74.2138, 74.3796,
             74.547,  74.6136, 25.2369, 25.2908,    25.3794,  25.4661,  25.5298,  25.5784,  38.5024, 38.5559, 38.5982,
             38.6212, 38.6586, 38.7142, 1.25658,    1.25658,  1.25658,  1.25658,  1.25658,  1.25658, 0,       0,
             0,       0,       0,       0,          0,        0,        0});
}

BOOST_AUTO_TEST_CASE(testEPEStandardReceiverSwap) {
    runTest({10000.0}, false, 5.0,
            {1776.1,  1776.16, 1776.27, 1776.39, 1776.42, 1776.58, 1876.93, 1877.36, 1878.17, 1878.88, 1879.66, 1880.74,
             1689.31, 1691.07, 1694.48, 1697.55, 1700.31, 1703.1,  1796.8,  1799.45, 1802.44, 1804.92, 1807.18, 1809.65,
             1629.45, 1632.97, 1636.23, 1640,    1643.18, 1647.31, 1732.38, 1735.85, 1738.74, 1741.2,  1743.84, 1746.11,
             1573.39, 1576.8,  1580.64, 1584.57, 1588.55, 1592.54, 1670.29, 1673.64, 1676.17, 1679.7,  1683.49, 1686.24,
             1518.25, 1521.77, 1524.01, 1526.41, 1531.78, 1535.98, 1604.98, 1607.35, 1609.64, 1612.66, 1615.93, 1618.91,
             1456.63, 1460.35, 1463.11, 1466.31, 1469.16, 1471.81, 1535.16, 1538.05, 1540.22, 1543.35, 1545.96, 1549.03,
             1389.55, 1393.3,  1395.02, 1397,    1400.06, 1403.08, 1462.55, 1464.82, 1466.93, 1468.96, 1471.46, 1473.84,
             1316.64, 1319.69, 1321.4,  1323.64, 1326.29, 1328.68, 1383.02, 1385.55, 1387.06, 1389.04, 1391.56, 1393.56,
             1238.64, 1240.94, 1243.11, 1246.07, 1247.79, 1249.73, 1299.71, 1301.09, 1303.37, 1305.24, 1307,    1309.24,
             1155.17, 1157.04, 1158.95, 1160.47, 1161.84, 1164.64, 1211.03, 1212.48, 1214.12, 1216.31, 1218.07, 1219.78,
             1068.44, 1069.98, 1070.92, 1072.97, 1074.54, 1076.37, 1119.5,  1120.92, 1122.32, 1123.97, 1125.53, 1126.69,
             976.39,  978.009, 979.414, 980.4,   981.515, 982.851, 1024.18, 1025.38, 1026.36, 1027.75, 1028.67, 1029.81,
             879.091, 880.557, 882.126, 883.351, 884.23,  885.776, 924.896, 926.077, 926.923, 927.62,  928.595, 929.658,
             780.719, 781.807, 782.875, 784.059, 784.728, 785.429, 821.668, 822.593, 823.351, 824.199, 825.001, 826.25,
             677.732, 678.55,  679.337, 679.897, 680.778, 681.247, 716.469, 717.211, 717.967, 718.95,  719.399, 719.747,
             570.817, 571.447, 572.099, 572.637, 573.167, 573.963, 607.066, 607.543, 607.927, 608.494, 608.939, 609.607,
             462.062, 462.526, 462.861, 463.412, 463.919, 464.334, 496.515, 496.929, 497.349, 497.577, 497.939, 498.317,
             350.516, 350.807, 350.997, 351.318, 351.743, 351.953, 384.239, 384.402, 384.575, 384.872, 385.161, 385.345,
             235.92,  236.085, 236.247, 236.417, 236.582, 236.647, 270.98,  271.032, 271.119, 271.205, 271.265, 271.319,
             119.009, 119.059, 119.098, 119.124, 119.161, 119.217, 164.655, 164.655, 164.655, 164.655, 164.655, 164.655,
             0,       0,       0,       0,       0,       0,       0,       0,       0});
}

BOOST_AUTO_TEST_CASE(testEPEAmortisingPayerSwap) {
    runTest({10000.0, 9500.0, 9000.0, 8500.0, 8000.0, 7500.0, 7000.0, 6500.0, 6000.0, 5500.0, 5000.0,
             4500.0,  4000.0, 3500.0, 3000.0, 2500.0, 2000.0, 1500.0, 1000.0, 500.0,  0.0},
            true, 5.0,
            {0,        0,       0,       0,       0.0635402, 0.0996111, 0.035936,  0.166171,  0.388186,  0.634441,
             0.903261, 1.16444, 5.22218, 6.0652,  7.75181,   9.2517,    10.602,    11.8243,   6.61368,   7.63869,
             8.68031,  9.73107, 10.5444, 11.5449, 22.8357,   24.4621,   25.9824,   27.7498,   29.2149,   31.165,
             19.2384,  20.6274, 21.7167, 22.7248, 23.7752,   24.4396,   39.8875,   41.3704,   42.9692,   44.7158,
             46.4326,  48.1301, 30.8631, 32.1783, 33.1271,   34.4257,   35.8597,   36.958,    54.8657,   56.2677,
             57.103,   57.9756, 60.2837, 62.0728, 41.1482,   42.0981,   43.0602,   44.1488,   45.2792,   46.4176,
             64.2242,  65.7178, 66.695,  67.9049, 69.0142,   69.9904,   46.9911,   48.0362,   48.8651,   49.8856,
             50.7845,  51.6657, 70.0341, 71.3877, 72.0085,   72.6884,   73.7758,   74.8804,   51.3413,   51.997,
             52.6139,  53.2466, 53.9994, 54.6788, 72.1067,   73.1625,   73.7329,   74.4325,   75.3778,   76.1421,
             51.6972,  52.403,  52.8399, 53.3164, 54.0852,   54.6213,   71.0502,   71.7524,   72.3968,   73.3006,
             73.83,    74.4205, 49.8508, 50.2944, 50.8843,   51.4477,   52.029,    52.7141,   67.5214,   68.0293,
             68.5529,  68.9871, 69.3272, 70.1703, 46.8175,   47.2749,   47.6874,   48.1455,   48.5909,   48.9994,
             62.5804,  62.9791, 63.1859, 63.7347, 64.1205,   64.5542,   42.0059,   42.3223,   42.6418,   43.0848,
             43.3828,  43.6137, 55.6927, 56.082,  56.3962,   56.5947,   56.8809,   57.1763,   36.3262,   36.5648,
             36.7741,  37.0445, 37.2607, 37.4808, 47.8274,   48.1118,   48.4317,   48.6837,   48.8367,   49.1609,
             30.1894,  30.3809, 30.5203, 30.6408, 30.8201,   30.9834,   40.0044,   40.2067,   40.3934,   40.6031,
             40.7129,  40.843,  23.7438, 23.8644, 23.9791,   24.1195,   24.2254,   24.4208,   31.9716,   32.0886,
             32.1989,  32.2737, 32.4059, 32.4767, 17.4688,   17.573,    17.6864,   17.8163,   17.8776,   17.9241,
             24.0637,  24.1502, 24.2402, 24.3139, 24.3819,   24.4917,   12.1097,   12.1592,   12.2033,   12.2612,
             12.3094,  12.3633, 16.9255, 16.9758, 17.0046,   17.0663,   17.1174,   17.161,    7.27119,   7.30655,
             7.33206,  7.3495,  7.382,   7.40213, 10.5911,   10.6146,   10.6292,   10.6574,   10.6943,   10.7102,
             3.50722,  3.51497, 3.52593, 3.53726, 3.55108,   3.56092,   5.55212,   5.5622,    5.57099,   5.58081,
             5.59027,  5.59405, 1.0627,  1.06533, 1.06692,   1.06874,   1.06958,   1.07202,   1.92512,   1.92779,
             1.92991,  1.93106, 1.93293, 1.93571, 0.0628288, 0.0628288, 0.0628288, 0.0628288, 0.0628288, 0.0628289,
             0,        0,       0,       0,       0,         0,         0,         0,         0});
}

BOOST_AUTO_TEST_CASE(testEPEAmortisingReceiverSwap) {
    runTest({10000.0, 9500.0, 9000.0, 8500.0, 8000.0, 7500.0, 7000.0, 6500.0, 6000.0, 5500.0, 5000.0,
             4500.0,  4000.0, 3500.0, 3000.0, 2500.0, 2000.0, 1500.0, 1000.0, 500.0,  0.0},
            false, 10.0,
            {961.998, 962.037, 962.093, 962.152, 962.166, 962.237, 1062.45, 1062.55, 1062.85, 1063.06, 1063.27, 1063.59,
             870.155, 870.999, 872.661, 874.169, 875.532, 876.831, 965.656, 966.707, 967.782, 968.878, 969.664, 970.75,
             796.03,  797.683, 799.142, 800.936, 802.374, 804.28,  880.794, 882.224, 883.305, 884.29,  885.348, 886.051,
             726.982, 728.478, 730.147, 731.837, 733.62,  735.345, 801.657, 802.972, 803.892, 805.221, 806.744, 807.896,
             661.313, 662.726, 663.603, 664.556, 666.834, 668.564, 724.232, 725.157, 726.057, 727.194, 728.346, 729.481,
             596.371, 597.825, 598.856, 600.059, 601.174, 602.158, 650.48,  651.571, 652.373, 653.42,  654.263, 655.219,
             532.429, 533.819, 534.461, 535.156, 536.233, 537.315, 579.784, 580.467, 581.072, 581.694, 582.468, 583.218,
             470.182, 471.213, 471.776, 472.542, 473.394, 474.18,  510.442, 511.19,  511.636, 512.148, 512.923, 513.461,
             410.043, 410.763, 411.425, 412.296, 412.855, 413.461, 444.386, 444.818, 445.418, 445.977, 446.516, 447.238,
             352.314, 352.837, 353.361, 353.789, 354.161, 354.972, 381.526, 381.944, 382.361, 382.829, 383.274, 383.688,
             298.222, 298.614, 298.844, 299.368, 299.784, 300.23,  322.807, 323.135, 323.454, 323.857, 324.194, 324.43,
             247.386, 247.777, 248.087, 248.301, 248.576, 248.88,  268.402, 268.647, 268.865, 269.143, 269.347, 269.569,
             200.161, 200.452, 200.77,  201.032, 201.188, 201.512, 218.047, 218.234, 218.371, 218.477, 218.657, 218.826,
             157.762, 157.958, 158.155, 158.366, 158.485, 158.606, 172.207, 172.335, 172.442, 172.582, 172.699, 172.885,
             119.639, 119.76,  119.878, 119.966, 120.097, 120.166, 131.566, 131.667, 131.782, 131.905, 131.971, 132.022,
             86.1339, 86.2152, 86.302,  86.3712, 86.4373, 86.5439, 95.5948, 95.6435, 95.688,  95.7448, 95.7896, 95.839,
             58.0129, 58.0613, 58.095,  58.1534, 58.2064, 58.2508, 65.3708, 65.4053, 65.4312, 65.4474, 65.4788, 65.4999,
             35.1676, 35.1909, 35.2061, 35.2332, 35.2684, 35.2847, 40.7032, 40.7105, 40.7214, 40.7327, 40.7466, 40.7568,
             17.7212, 17.7311, 17.7403, 17.7504, 17.7597, 17.7634, 21.6034, 21.6059, 21.6075, 21.6092, 21.6099, 21.6126,
             5.95044, 5.95296, 5.95492, 5.95619, 5.95805, 5.96085, 8.23275, 8.23275, 8.23275, 8.23275, 8.23275, 8.23275,
             0,       0,       0,       0,       0,       0,       0,       0,       0});
}

BOOST_AUTO_TEST_CASE(testEPEAccretingPayerSwap) {
    runTest({10000.0, 11000.0, 12000.0, 13000.0, 14000.0, 15000.0, 16000.0, 17000.0, 18000.0, 19000.0,
             20000.0, 21000.0, 22000.0, 23000.0, 24000.0, 25000.0, 26000.0, 27000.0, 28000.0, 29000.0},
            true, 10.0,
            {0,       0,       0,       0.0201268, 0.303151, 0.492128, 0.996429, 2.34557, 3.95999, 6.21514, 8.50881,
             10.8789, 20.0638, 23.6787, 30.6577,   36.8466,  42.3846,  47.9246,  44.0161, 49.9686, 56.4549, 61.9091,
             67.2612, 72.4536, 94.4797, 101.627,   108.739,  116.334,  123.174,  131.838, 118.332, 125.834, 132.444,
             138.029, 143.965, 149.348, 179.48,    186.679,  194.639,  203.367,  211.48,  219.84,  194.913, 202.513,
             208.166, 216.025, 224.238, 230.37,    267.965,  275.567,  280.404,  285.421, 297.124, 306.53,  270.392,
             276.072, 281.611, 287.977, 295.193,   302.139,  342.83,   351.21,   357.283, 364.517, 370.867, 376.839,
             329.971, 336.168, 341.369, 348.435,   354.896,  361.73,   408.801,  417.062, 420.807, 425.29,  432.412,
             439.367, 384.213, 389.783, 394.784,   399.9,    405.539,  410.878,  461.195, 468.37,  472.436, 477.354,
             484.027, 489.531, 423.175, 429.096,   432.727,  437.811,  443.607,  448.655, 501.693, 506.981, 512.066,
             519.459, 523.331, 527.855, 451.411,   454.973,  460.204,  465.019,  469.408, 474.516, 529.141, 533.641,
             538.364, 542.097, 545.255, 552.239,   467.676,  471.477,  475.712,  480.977, 485.523, 489.821, 547.096,
             551.04,  553.258, 558.449, 562.113,   566.617,  471.922,  475.411,  478.939, 483.373, 487.003, 490.123,
             548.138, 552.224, 555.849, 558.254,   561.161,  564.46,   462.237,  465.516, 467.838, 471.329, 473.883,
             476.937, 534.355, 538.071, 542.18,    545.286,  547.528,  551.54,   441.736, 444.906, 447.21,  449.377,
             451.856, 454.677, 514.14,  517.086,   519.79,   522.9,    524.555,  526.529, 407.306, 409.76,  411.966,
             414.283, 416.297, 419.716, 479.117,   481.257,  483.262,  484.575,  486.981, 488.28,  360.88,  362.938,
             364.905, 367.684, 368.849, 369.822,   430.016,  431.843,  433.689,  435.253, 436.746, 438.971, 306.661,
             307.983, 309.111, 310.686, 312.051,   314.024,  370.213,  371.546,  372.385, 373.993, 375.38,  376.525,
             238.345, 239.511, 240.697, 241.412,   242.45,   243.555,  295.939,  296.785, 297.316, 298.253, 299.496,
             300.087, 159.434, 159.911, 160.415,   161.286,  162.106,  162.602,  210.565, 211.048, 211.5,   211.978,
             212.462, 212.654, 73.8598, 74.0193,   74.2862,  74.5391,  74.7318,  74.8647, 111.657, 111.812, 111.935,
             112.002, 112.11,  112.271, 3.64407,   3.64407,  3.64407,  3.64407,  3.64407, 3.64407, 0,       0,
             0,       0,       0,       0,         0,        0,        0});
}

BOOST_AUTO_TEST_CASE(testEPEAccretingReceiverSwap) {
    runTest({10000.0, 11000.0, 12000.0, 13000.0, 14000.0, 15000.0, 16000.0, 17000.0, 18000.0, 19000.0,
             20000.0, 21000.0, 22000.0, 23000.0, 24000.0, 25000.0, 26000.0, 27000.0, 28000.0, 29000.0},
            false, 10.0,
            {3404.31, 3404.42, 3404.64, 3404.86, 3404.94, 3405.25, 3506.11, 3507.41, 3509.22, 3511.31, 3513.38, 3515.98,
             3327.63, 3331.23, 3338.16, 3344.34, 3349.95, 3355.74, 3460.75, 3466.82, 3473.4,  3478.98, 3484.22, 3489.64,
             3296.33, 3303.63, 3310.49, 3318.2,  3324.93, 3333.48, 3437.95, 3445.57, 3452.12, 3457.58, 3463.5,  3468.97,
             3266.31, 3273.56, 3281.74, 3290.2,  3298.54, 3307.08, 3410.11, 3417.72, 3423.28, 3431.31, 3439.86, 3446.21,
             3232.28, 3240.03, 3244.99, 3250.28, 3261.86, 3271.03, 3368.87, 3374.43, 3379.72, 3386.34, 3393.63, 3400.6,
             3177.29, 3185.58, 3191.84, 3199,    3205.33, 3211.35, 3307.24, 3313.69, 3318.71, 3325.88, 3332.06, 3339.26,
             3103.98, 3112.46, 3116.36, 3120.92, 3127.98, 3134.82, 3230.59, 3236.27, 3241.21, 3246.27, 3252.05, 3257.8,
             3009.72, 3016.78, 3020.79, 3025.99, 3032.27, 3037.86, 3130.27, 3136.46, 3140.16, 3145.44, 3151.27, 3156.31,
             2895.98, 2901.43, 2906.62, 2913.77, 2917.83, 2922.44, 3012.5,  3015.96, 3021.3,  3026.12, 3030.27, 3035.58,
             2760.99, 2765.57, 2770.28, 2773.96, 2777.31, 2784.13, 2871.94, 2875.44, 2879.7,  2885.02, 2889.59, 2893.95,
             2608.95, 2612.81, 2615.15, 2620.26, 2624.16, 2628.75, 2714.6,  2718.19, 2721.71, 2725.87, 2729.77, 2732.93,
             2434.45, 2438.53, 2442.14, 2444.67, 2447.48, 2450.87, 2537.02, 2540.33, 2542.72, 2546.29, 2548.73, 2551.79,
             2237,    2240.81, 2244.9,  2248.06, 2250.37, 2254.37, 2339.88, 2342.98, 2345.25, 2347.26, 2349.77, 2352.66,
             2026.66, 2029.53, 2032.34, 2035.47, 2037.25, 2039.11, 2121.68, 2124.22, 2126.33, 2128.61, 2130.79, 2134.1,
             1793.93, 1796.15, 1798.27, 1799.78, 1802.16, 1803.44, 1887.31, 1889.32, 1891.31, 1894,    1895.24, 1896.29,
             1540.19, 1541.92, 1543.7,  1545.18, 1546.64, 1548.81, 1630.81, 1632.12, 1633.25, 1634.8,  1636.1,  1637.97,
             1270.16, 1271.46, 1272.4,  1273.93, 1275.35, 1276.51, 1359.46, 1360.6,  1361.8,  1362.47, 1363.48, 1364.61,
             981.213, 982.04,  982.58,  983.491, 984.696, 985.296, 1071.82, 1072.28, 1072.79, 1073.65, 1074.48, 1074.99,
             672.318, 672.794, 673.262, 673.752, 674.228, 674.414, 770.008, 770.161, 770.425, 770.674, 770.856, 771.005,
             345.125, 345.272, 345.386, 345.459, 345.567, 345.729, 477.499, 477.499, 477.499, 477.499, 477.499, 477.499,
             0,       0,       0,       0,       0,       0,       0,       0,       0});
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
