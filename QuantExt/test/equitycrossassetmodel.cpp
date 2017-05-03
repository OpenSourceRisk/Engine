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

#include "equitycrossassetmodel.hpp"

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/all.hpp>
#include <qle/processes/all.hpp>
#include <qle/pricingengines/all.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/currencies/america.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/gbplibor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/quotes/simplequote.hpp>

#include <test-suite/utilities.hpp>

#include <boost/make_shared.hpp>
// fix for boost 1.64, see https://lists.boost.org/Archives/boost/2016/11/231756.php
#if BOOST_VERSION >= 106400
#include <boost/serialization/array_wrapper.hpp>
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;

namespace {

struct CrossAssetData {
    CrossAssetData()
        : referenceDate(30, July, 2015), eurYts(boost::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed())),
          usdYts(boost::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed())),
          eqDivSp(boost::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed())),
          eqDivLh(boost::make_shared<FlatForward>(referenceDate, 0.0075, Actual365Fixed())),
          usdEurSpotToday(boost::make_shared<SimpleQuote>(0.90)), eurEurSpotToday(boost::make_shared<SimpleQuote>(1.0)),
          spSpotToday(boost::make_shared<SimpleQuote>(2100)), lhSpotToday(boost::make_shared<SimpleQuote>(12.50)) {

        SavedSettings backup;

        Settings::instance().evaluationDate() = referenceDate;

        // use different grids for each of the individual processes
        // to test the piecewise numerical integration ...

        std::vector<Date> volstepdatesEur, volstepdatesUsd, volstepdatesFx;
        volstepdatesEqSp.resize(0);
        volstepdatesEqLh.resize(0);

        volstepdatesEur.push_back(Date(15, July, 2016));
        volstepdatesEur.push_back(Date(15, July, 2017));
        volstepdatesEur.push_back(Date(15, July, 2018));
        volstepdatesEur.push_back(Date(15, July, 2019));
        volstepdatesEur.push_back(Date(15, July, 2020));

        volstepdatesUsd.push_back(Date(13, April, 2016));
        volstepdatesUsd.push_back(Date(13, September, 2016));
        volstepdatesUsd.push_back(Date(13, April, 2017));
        volstepdatesUsd.push_back(Date(13, September, 2017));
        volstepdatesUsd.push_back(Date(13, April, 2018));
        volstepdatesUsd.push_back(Date(15, July, 2018)); // shared with EUR
        volstepdatesUsd.push_back(Date(13, April, 2019));
        volstepdatesUsd.push_back(Date(13, September, 2019));

        volstepdatesFx.push_back(Date(15, July, 2016)); // shared with EUR
        volstepdatesFx.push_back(Date(15, October, 2016));
        volstepdatesFx.push_back(Date(15, May, 2017));
        volstepdatesFx.push_back(Date(13, September, 2017)); // shared with USD
        volstepdatesFx.push_back(Date(15, July, 2018));      //  shared with EUR and USD

        volstepdatesEqSp.push_back(Date(13, April, 2016));   // shared with USD
        volstepdatesEqSp.push_back(Date(15, October, 2016)); // shared with FX
        volstepdatesEqSp.push_back(Date(15, March, 2017));
        volstepdatesEqSp.push_back(Date(13, October, 2017));
        volstepdatesEqSp.push_back(Date(15, July, 2018)); //  shared with EUR and USD
        volstepdatesEqSp.push_back(Date(13, October, 2018));

        volstepdatesEqLh.push_back(Date(13, June, 2016));
        volstepdatesEqLh.push_back(Date(15, September, 2016));
        volstepdatesEqLh.push_back(Date(15, April, 2017));
        volstepdatesEqLh.push_back(Date(13, October, 2017));
        volstepdatesEqLh.push_back(Date(15, July, 2018)); //  shared with EUR and USD
        volstepdatesEqLh.push_back(Date(13, December, 2018));

        std::vector<Real> eurVols, usdVols, fxVols, eqSpVols, eqLhVols;

        for (Size i = 0; i < volstepdatesEur.size() + 1; ++i) {
            eurVols.push_back(0.0050 + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesUsd.size() + 1; ++i) {
            usdVols.push_back(0.0030 + (0.0110 - 0.0030) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
            fxVols.push_back(0.15 + (0.20 - 0.15) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesEqSp.size() + 1; ++i) {
            eqSpVols.push_back(0.20 + (0.35 - 0.20) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesEqLh.size() + 1; ++i) {
            eqLhVols.push_back(0.25 + (0.45 - 0.25) * std::exp(-0.3 * static_cast<double>(i)));
        }

        Array alphaTimesEur(volstepdatesEur.size()), alphaEur(eurVols.begin(), eurVols.end()), kappaTimesEur(0),
            kappaEur(1, 0.02);
        Array alphaTimesUsd(volstepdatesUsd.size()), alphaUsd(usdVols.begin(), usdVols.end()), kappaTimesUsd(0),
            kappaUsd(1, 0.04);
        Array fxTimes(volstepdatesFx.size()), fxSigmas(fxVols.begin(), fxVols.end());
        Array eqSpTimes(volstepdatesEqSp.size()), spSigmas(eqSpVols.begin(), eqSpVols.end());
        Array eqLhTimes(volstepdatesEqLh.size()), lhSigmas(eqLhVols.begin(), eqLhVols.end());

        for (Size i = 0; i < alphaTimesEur.size(); ++i) {
            alphaTimesEur[i] = eurYts->timeFromReference(volstepdatesEur[i]);
        }
        for (Size i = 0; i < alphaTimesUsd.size(); ++i) {
            alphaTimesUsd[i] = eurYts->timeFromReference(volstepdatesUsd[i]);
        }
        for (Size i = 0; i < fxTimes.size(); ++i) {
            fxTimes[i] = eurYts->timeFromReference(volstepdatesFx[i]);
        }
        for (Size i = 0; i < eqSpTimes.size(); ++i) {
            eqSpTimes[i] = eurYts->timeFromReference(volstepdatesEqSp[i]);
        }
        for (Size i = 0; i < eqLhTimes.size(); ++i) {
            eqLhTimes[i] = eurYts->timeFromReference(volstepdatesEqLh[i]);
        }

        boost::shared_ptr<IrLgm1fParametrization> eurLgmParam =
            boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYts, alphaTimesEur, alphaEur,
                                                                        kappaTimesEur, kappaEur);

        boost::shared_ptr<IrLgm1fParametrization> usdLgmParam =
            boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(USDCurrency(), usdYts, alphaTimesUsd, alphaUsd,
                                                                        kappaTimesUsd, kappaUsd);

        boost::shared_ptr<FxBsParametrization> fxUsdEurBsParam =
            boost::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), usdEurSpotToday, fxTimes, fxSigmas);

        boost::shared_ptr<EqBsParametrization> eqSpBsParam = boost::make_shared<EqBsPiecewiseConstantParametrization>(
            USDCurrency(), "SP", spSpotToday, usdEurSpotToday, eqSpTimes, spSigmas, usdYts, eqDivSp);

        boost::shared_ptr<EqBsParametrization> eqLhBsParam = boost::make_shared<EqBsPiecewiseConstantParametrization>(
            EURCurrency(), "LH", lhSpotToday, eurEurSpotToday, eqLhTimes, lhSigmas, eurYts, eqDivLh);

        singleModels.resize(0);
        singleModels.push_back(eurLgmParam);
        singleModels.push_back(usdLgmParam);
        singleModels.push_back(fxUsdEurBsParam);
        singleModels.push_back(eqSpBsParam);
        singleModels.push_back(eqLhBsParam);

        ccLgm = boost::make_shared<CrossAssetModel>(singleModels);

        eurIdx = ccLgm->ccyIndex(EURCurrency());
        usdIdx = ccLgm->ccyIndex(USDCurrency());
        eurUsdIdx = usdIdx - 1;
        eqSpIdx = ccLgm->eqIndex("SP");
        eqLhIdx = ccLgm->eqIndex("LH");

        ccLgm->correlation(IR, eurIdx, IR, usdIdx, -0.2);
        ccLgm->correlation(IR, eurIdx, FX, eurUsdIdx, 0.8);
        ccLgm->correlation(IR, usdIdx, FX, eurUsdIdx, -0.5);
        ccLgm->correlation(EQ, eqSpIdx, EQ, eqLhIdx, 0.6);
        ccLgm->correlation(EQ, eqSpIdx, IR, usdIdx, -0.1);
        ccLgm->correlation(EQ, eqLhIdx, IR, eurIdx, -0.05);
        ccLgm->correlation(EQ, eqSpIdx, FX, eurUsdIdx, 0.1);
    }

    SavedSettings backup;
    Date referenceDate;
    Handle<YieldTermStructure> eurYts, usdYts;
    Handle<YieldTermStructure> eqDivSp, eqDivLh;
    Handle<Quote> usdEurSpotToday, eurEurSpotToday, spSpotToday, lhSpotToday;
    std::vector<boost::shared_ptr<Parametrization> > singleModels;
    boost::shared_ptr<CrossAssetModel> ccLgm;
    Size eurIdx, usdIdx, eurUsdIdx, eqSpIdx, eqLhIdx;
    std::vector<Date> volstepdatesEqSp, volstepdatesEqLh;
};

} // anonymous namespace

namespace testsuite {

void EquityCrossAssetModelTest::testEqLgm5fPayouts() {

    BOOST_TEST_MESSAGE("Testing pricing of equity payouts under domestic "
                       "measure in CrossAsset LGM model...");

    CrossAssetData d;
    Settings::instance().evaluationDate() = d.referenceDate;

    boost::shared_ptr<StochasticProcess> process = d.ccLgm->stateProcess(CrossAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> process2 = d.ccLgm->stateProcess(CrossAssetStateProcess::euler);

    // path generation

    Size n = 500000; // number of paths
    Size seed = 121; // seed
    // maturity of test payoffs
    Time T = 5.0;
    // take large steps, but not only one (since we are testing)
    Size steps = static_cast<Size>(T * 2.0);
    Size steps_euler = static_cast<Size>(T * 52.0);
    TimeGrid grid(T, steps);
    TimeGrid grid_euler(T, steps_euler);
    PseudoRandom::rsg_type sg2 = PseudoRandom::make_sequence_generator(steps, seed);

    MultiPathGeneratorMersenneTwister pg(process, grid, seed, false);
    MultiPathGeneratorMersenneTwister pg2(process2, grid_euler, seed, false);

    // test
    // 1 LH (EUR) forward under CrossAsset numeraire vs. analytic pricing engine
    // 2 SP (USD) forward (converted to EUR) under CrossAsset numeraire vs. analytic pricing engine
    // 3 LH (EUR) EQ option under CrossAsset numeraire vs. analytic pricing engine
    // 4 SP (USD) EQ option under CrossAsset numeraire vs. analytic pricing engine

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > stat1, stat2, stat3a, stat3b, stat4a, stat4b;

    Real strikeLh = 12.7;
    Real strikeSp = 2150.0;

    // same for paths2 since shared time grid
    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path = pg.next();
        // Sample<MultiPath> path = pg2.next();
        Size l = path.value[0].length() - 1;
        Real eurusdfx = std::exp(path.value[2][l]);
        Real zeur = path.value[0][l];
        Real eqsp = std::exp(path.value[3][l]);
        Real eqlh = std::exp(path.value[4][l]);
        Real ccnum = d.ccLgm->numeraire(0, T, zeur);

        // 1 LH forward settled at T deflated with numeraire
        Real lhFwd = eqlh - strikeLh;
        stat1(lhFwd / ccnum);

        // 2 SP forward settled at T (converted to base) deflated with numeraire
        Real spFwd = eurusdfx * (eqsp - strikeSp);
        stat2(spFwd / ccnum);

        // 3 LH option exercise at T deflated with numeraire
        Real lhCall = std::max(lhFwd, 0.0);
        Real lhPut = std::max(-1.0 * lhFwd, 0.0);
        stat3a(lhCall / ccnum);
        stat3b(lhPut / ccnum);

        // 4 SP option exercise at T (converted to base) deflated with numeraire
        Real spCall = std::max(spFwd, 0.0);
        Real spPut = std::max(-1.0 * spFwd, 0.0);
        stat4a(spCall / ccnum);
        stat4b(spPut / ccnum);
    }

    Date tradeMaturity = d.referenceDate + 5 * 365;

    boost::shared_ptr<EquityForward> lhFwdTrade =
        boost::make_shared<EquityForward>("LH", EURCurrency(), Position::Long, 1, tradeMaturity, strikeLh);
    boost::shared_ptr<EquityForward> spFwdTrade =
        boost::make_shared<EquityForward>("SP", USDCurrency(), Position::Long, 1, tradeMaturity, strikeSp);

    boost::shared_ptr<VanillaOption> lhCall =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Call, strikeLh),
                                          boost::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));
    boost::shared_ptr<VanillaOption> lhPut =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Put, strikeLh),
                                          boost::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));
    boost::shared_ptr<VanillaOption> spCall =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Call, strikeSp),
                                          boost::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));
    boost::shared_ptr<VanillaOption> spPut =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Put, strikeSp),
                                          boost::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));

    boost::shared_ptr<DiscountingEquityForwardEngine> lhFwdEngine =
        boost::make_shared<DiscountingEquityForwardEngine>(d.eurYts, d.eqDivLh, d.lhSpotToday, d.eurYts);
    boost::shared_ptr<DiscountingEquityForwardEngine> spFwdEngine =
        boost::make_shared<DiscountingEquityForwardEngine>(d.usdYts, d.eqDivSp, d.spSpotToday, d.usdYts);

    lhFwdTrade->setPricingEngine(lhFwdEngine);
    spFwdTrade->setPricingEngine(spFwdEngine);

    boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> spEqOptionEngine =
        boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgm, d.eqSpIdx, d.ccLgm->ccyIndex(d.ccLgm->eqbs(d.eqSpIdx)->currency()));
    boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> lhEqOptionEngine =
        boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgm, d.eqLhIdx, d.ccLgm->ccyIndex(d.ccLgm->eqbs(d.eqLhIdx)->currency()));

    lhCall->setPricingEngine(lhEqOptionEngine);
    lhPut->setPricingEngine(lhEqOptionEngine);
    spCall->setPricingEngine(spEqOptionEngine);
    spPut->setPricingEngine(spEqOptionEngine);

    Real npv1 = mean(stat1);
    Real error1 = error_of<tag::mean>(stat1);
    Real expected1 = lhFwdTrade->NPV();

    Real npv2 = mean(stat2);
    Real error2 = error_of<tag::mean>(stat2);
    Real expected2 = d.usdEurSpotToday->value() * spFwdTrade->NPV();

    Real npv3a = mean(stat3a);
    Real error3a = error_of<tag::mean>(stat3a);
    Real expected3a = lhCall->NPV();
    Real npv3b = mean(stat3b);
    Real error3b = error_of<tag::mean>(stat3b);
    Real expected3b = lhPut->NPV();

    Real npv4a = mean(stat4a);
    Real error4a = error_of<tag::mean>(stat4a);
    Real expected4a = d.usdEurSpotToday->value() * spCall->NPV();
    Real npv4b = mean(stat4b);
    Real error4b = error_of<tag::mean>(stat4b);
    Real expected4b = d.usdEurSpotToday->value() * spPut->NPV();

    Real tolErrEst = 1.5; // allow absolute diffs to be within 1.5 standard errors
    // TODO : IS THIS STRICT ENOUGH?
    BOOST_CHECK_LE(std::fabs(npv1 - expected1), tolErrEst * error1);
    BOOST_CHECK_LE(std::fabs(npv2 - expected2), tolErrEst * error2);
    BOOST_CHECK_LE(std::fabs(npv3a - expected3a), tolErrEst * error3a);
    BOOST_CHECK_LE(std::fabs(npv3b - expected3b), tolErrEst * error3b);
    BOOST_CHECK_LE(std::fabs(npv4a - expected4a), tolErrEst * error4a);
    BOOST_CHECK_LE(std::fabs(npv4b - expected4b), tolErrEst * error4b);

} // testEqLgm5fPayouts

void EquityCrossAssetModelTest::testLgm5fEqCalibration() {

    BOOST_TEST_MESSAGE("Testing EQ calibration of IR-FX-EQ LGM 5F model...");

    CrossAssetData d;
    Settings::instance().evaluationDate() = d.referenceDate;

    // calibration baskets
    std::vector<boost::shared_ptr<CalibrationHelper> > basketSp, basketLh;

    for (Size i = 0; i < d.volstepdatesEqSp.size(); ++i) {
        Date tmp = i < d.volstepdatesEqSp.size() ? d.volstepdatesEqSp[i] : d.volstepdatesEqSp.back() + 365;
        basketSp.push_back(boost::make_shared<FxEqOptionHelper>(
            tmp, Null<Real>(), d.spSpotToday, Handle<Quote>(boost::make_shared<SimpleQuote>(0.20)), d.usdYts, d.eqDivSp,
            CalibrationHelper::RelativePriceError));
    }
    for (Size i = 0; i < d.volstepdatesEqLh.size(); ++i) {
        Date tmp = i < d.volstepdatesEqLh.size() ? d.volstepdatesEqLh[i] : d.volstepdatesEqLh.back() + 365;
        basketLh.push_back(boost::make_shared<FxEqOptionHelper>(
            tmp, Null<Real>(), d.lhSpotToday, Handle<Quote>(boost::make_shared<SimpleQuote>(0.20)), d.eurYts, d.eqDivLh,
            CalibrationHelper::RelativePriceError));
    }

    // pricing engines
    boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> spEqOptionEngine =
        boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgm, d.eqSpIdx, d.ccLgm->ccyIndex(d.ccLgm->eqbs(d.eqSpIdx)->currency()));
    boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> lhEqOptionEngine =
        boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgm, d.eqLhIdx, d.ccLgm->ccyIndex(d.ccLgm->eqbs(d.eqLhIdx)->currency()));

    // assign engines to calibration instruments
    for (Size i = 0; i < basketSp.size(); ++i) {
        basketSp[i]->setPricingEngine(spEqOptionEngine);
    }
    for (Size i = 0; i < basketLh.size(); ++i) {
        basketLh[i]->setPricingEngine(lhEqOptionEngine);
    }

    // calibrate the model
    LevenbergMarquardt lm(1E-8, 1E-8, 1E-8);
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);

    d.ccLgm->calibrateBsVolatilitiesIterative(CrossAssetModelTypes::EQ, d.eqSpIdx, basketSp, lm, ec);
    d.ccLgm->calibrateBsVolatilitiesIterative(CrossAssetModelTypes::EQ, d.eqLhIdx, basketLh, lm, ec);

    // check the results
    Real tol = 1E-6;

    for (Size i = 0; i < basketSp.size(); ++i) {
        Real model = basketSp[i]->modelValue();
        Real market = basketSp[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in SP basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
    for (Size i = 0; i < basketLh.size(); ++i) {
        Real model = basketLh[i]->modelValue();
        Real market = basketLh[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in LH basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
}

void EquityCrossAssetModelTest::testLgm5fMoments() {

    // TODO : REVIEW TOLERANCES
    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization "
                       "in IR-FX-EQ LGM 5F model...");

    CrossAssetData d;
    Settings::instance().evaluationDate() = d.referenceDate;

    boost::shared_ptr<StochasticProcess> p_exact = d.ccLgm->stateProcess(CrossAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> p_euler = d.ccLgm->stateProcess(CrossAssetStateProcess::euler);

    Real T = 10.0;                                  // horizon at which we compare the moments
    Size steps_euler = static_cast<Size>(T * 50.0); // number of simulation steps
    Size steps_exact = 1;
    Size paths = 25000; // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);
    Matrix v_an_eu = p_euler->covariance(0.0, p_euler->initialValues(), T);

    TimeGrid grid_euler(T, steps_euler);
    TimeGrid grid_exact(T, steps_exact);

    MultiPathGeneratorSobolBrownianBridge pgen(p_euler, grid_euler);
    MultiPathGeneratorSobolBrownianBridge pgen2(p_exact, grid_exact);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > e_eu[5], e_eu2[5];
    accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > v_eu[5][5], v_eu2[5][5];

    for (Size i = 0; i < paths; ++i) {
        Sample<MultiPath> path = pgen.next();
        Sample<MultiPath> path2 = pgen2.next();
        for (Size ii = 0; ii < 5; ++ii) {
            Real cii = path.value[ii].back();
            Real cii2 = path2.value[ii].back();
            e_eu[ii](cii);
            e_eu2[ii](cii2);
            for (Size jj = 0; jj <= ii; ++jj) {
                Real cjj = path.value[jj].back();
                v_eu[ii][jj](cii, covariate1 = cjj);
                Real cjj2 = path2.value[jj].back();
                v_eu2[ii][jj](cii2, covariate1 = cjj2);
            }
        }
    }

    Real errTol[] = { 0.2E-4, 0.2E-4, 10.0E-4, 10.0E-4, 10.0E-4 };

    for (Size i = 0; i < 5; ++i) {
        // check expectation against analytical calculation (Euler)
        if (std::fabs(mean(e_eu[i]) - e_an[i]) > errTol[i]) {
            BOOST_ERROR("analytical expectation for component #"
                        << i << " (" << e_an[i] << ") is inconsistent with numerical value (Euler "
                                                   "discretization, " << mean(e_eu[i]) << "), error is "
                        << e_an[i] - mean(e_eu[i]) << " tolerance is " << errTol[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTol[i]) {
            BOOST_ERROR("analytical expectation for component #"
                        << i << " (" << e_an[i] << ") is inconsistent with numerical value (Exact "
                                                   "discretization, " << mean(e_eu2[i]) << "), error is "
                        << e_an[i] - mean(e_eu2[i]) << " tolerance is " << errTol[i]);
        }
    }

    // we have to deal with different natures of volatility
    // for ir (normal) and fx (ln) so different error
    // tolerances apply
    Real tollNormal = 0.1E-4; // ir-ir
    Real tolMixed = 0.25E-4;  // ir-fx, ir-eq
    Real tolLn = 8.0E-4;      // fx-fx, fx-eq
    Real tolEq = 12.0E-4;     // eq-eq (to account for higher eq vols)
    Real tol;                 // set below

    for (Size i = 0; i < 5; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (i < 2) {
                tol = tollNormal;
            } else if ((i >= 3) && (j >= 3)) {
                tol = tolEq;
            } else {
                if (j < 2) {
                    tol = tolMixed;
                } else {
                    tol = tolLn;
                }
            }
            if (std::fabs(covariance(v_eu[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at (" << i << "," << j << ") (" << v_an[i][j]
                                                         << ") is inconsistent with numerical "
                                                            "value (Euler discretization, " << covariance(v_eu[i][j])
                                                         << "), error is " << v_an[i][j] - covariance(v_eu[i][j])
                                                         << " tolerance is " << tol);
            }
            if (std::fabs(covariance(v_eu2[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at (" << i << "," << j << ") (" << v_an[i][j]
                                                         << ") is inconsistent with numerical "
                                                            "value (Exact discretization, " << covariance(v_eu2[i][j])
                                                         << "), error is " << v_an[i][j] - covariance(v_eu2[i][j])
                                                         << " tolerance is " << tol);
            }
        }
    }

    BOOST_TEST_MESSAGE("Testing correlation matrix recovery in presence of equity simulation");
    Matrix corr_input = d.ccLgm->correlation();
    BOOST_CHECK(corr_input.rows() == corr_input.columns());
    Size dim = corr_input.rows();
    BOOST_CHECK(corr_input.rows() == 5);
    Matrix r1(dim, dim), r2(dim, dim);
    Real dt = 1.0E-6;
    Real tol_corr = 1.0E-7;
    Matrix v_an_dt = p_exact->covariance(0.0, p_exact->initialValues(), dt);
    Matrix v_an_eu_dt = p_euler->covariance(0.0, p_euler->initialValues(), dt);
    BOOST_CHECK(v_an_dt.rows() == v_an_eu_dt.rows());
    BOOST_CHECK(v_an_dt.columns() == v_an_eu_dt.columns());
    BOOST_CHECK(corr_input.rows() == v_an_dt.rows());
    BOOST_CHECK(corr_input.columns() == corr_input.columns());

    for (Size i = 0; i < dim; ++i) {
        for (Size j = 0; j <= i; ++j) {
            r1[i][j] = r1[j][i] = v_an_dt[i][j] / std::sqrt(v_an_dt[i][i] * v_an_dt[j][j]);
            r2[i][j] = r2[j][i] = v_an_eu_dt[i][j] / std::sqrt(v_an_eu_dt[i][i] * v_an_eu_dt[j][j]);
            BOOST_CHECK_MESSAGE(std::fabs(r1[i][j] - corr_input[i][j]) < tol_corr,
                                "failed to recover correlation matrix from "
                                "exact state process (i,j)=("
                                    << i << "," << j << "), input correlation is " << corr_input[i][j] << ", output is "
                                    << r1[i][j] << ", difference " << (corr_input[i][j] - r1[i][j]) << ", tolerance "
                                    << tol_corr);
            BOOST_CHECK_MESSAGE(std::fabs(r2[i][j] - corr_input[i][j]) < tol_corr,
                                "failed to recover correlation matrix from "
                                "Euler state process (i,j)=("
                                    << i << "," << j << "), input correlation is " << corr_input[i][j] << ", output is "
                                    << r2[i][j] << ", difference " << (corr_input[i][j] - r2[i][j]) << ", tolerance "
                                    << tol_corr);
        }
    }
    /*
    for (Size i = 0; i < 5; ++i) {
        Real meu = mean(e_eu[i]);
        Real s_meu = error_of<tag::mean>(e_eu[i]);
        BOOST_TEST_MESSAGE(i << ";EULER;" << e_an[i] << ";" << meu << ";" << s_meu);
        Real meu2 = mean(e_eu2[i]);
        Real s_meu2 = error_of<tag::mean>(e_eu2[i]);
        BOOST_TEST_MESSAGE(i << ";EXACT;" << e_an[i] << ";" << meu2 << ";" << s_meu2);
    }
    for (Size i = 0; i < 5; ++i) {
        for (Size j = 0; j <= i; ++j) {
            Real cov = covariance(v_eu[i][j]);
            BOOST_TEST_MESSAGE(i << ";" << j << ";" << "EULER;" << v_an[i][j] << ";" << cov);
            Real cov2 = covariance(v_eu2[i][j]);
            BOOST_TEST_MESSAGE(i << ";" << j << ";" << "EXACT;" << v_an[i][j] << ";" << cov2);
        }
    }
    */
} // testLgm5fMoments

test_suite* EquityCrossAssetModelTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("EquityCrossAssetModelTests");
    suite->add(QUANTLIB_TEST_CASE(&EquityCrossAssetModelTest::testEqLgm5fPayouts));
    suite->add(QUANTLIB_TEST_CASE(&EquityCrossAssetModelTest::testLgm5fEqCalibration));
    suite->add(QUANTLIB_TEST_CASE(&EquityCrossAssetModelTest::testLgm5fMoments));
    return suite;
}
}
