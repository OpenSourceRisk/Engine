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

} // anonymous namespace

namespace testsuite {

void EquityCrossAssetModelTest::testEqLgm5fPayouts() {

    BOOST_TEST_MESSAGE("Testing pricing of equity payouts under domestic "
                       "measure in CrossAsset LGM model...");

    SavedSettings backup;

    Date referenceDate(30, July, 2015);

    Settings::instance().evaluationDate() = referenceDate;

    Handle<YieldTermStructure> eurYts(boost::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    Handle<YieldTermStructure> usdYts(boost::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed()));

    // use different grids for each of the individual processes
    // to test the piecewise numerical integration ...

    std::vector<Date> volstepdatesEur, volstepdatesUsd, volstepdatesFx, volstepdatesEqSp, volstepdatesEqLh;

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

    volstepdatesEqSp.push_back(Date(13, April, 2016)); // shared with USD
    volstepdatesEqSp.push_back(Date(15, October, 2016)); // shared with FX
    volstepdatesEqSp.push_back(Date(15, March, 2017));
    volstepdatesEqSp.push_back(Date(13, October, 2017)); 
    volstepdatesEqSp.push_back(Date(15, July, 2018));      //  shared with EUR and USD
    volstepdatesEqSp.push_back(Date(13, October, 2018));

    volstepdatesEqLh.push_back(Date(13, June, 2016));
    volstepdatesEqLh.push_back(Date(15, September, 2016));
    volstepdatesEqLh.push_back(Date(15, April, 2017));
    volstepdatesEqLh.push_back(Date(13, October, 2017));
    volstepdatesEqLh.push_back(Date(15, July, 2018));      //  shared with EUR and USD
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

    boost::shared_ptr<IrLgm1fParametrization> eurLgmParam = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
        EURCurrency(), eurYts, alphaTimesEur, alphaEur, kappaTimesEur, kappaEur);

    boost::shared_ptr<IrLgm1fParametrization> usdLgmParam = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
        USDCurrency(), usdYts, alphaTimesUsd, alphaUsd, kappaTimesUsd, kappaUsd);

    // USD per EUR (foreign per domestic)
    Handle<Quote> usdEurSpotToday(boost::make_shared<SimpleQuote>(0.90));
    // EUR per EUR (dummy
    Handle<Quote> eurEurSpotToday(boost::make_shared<SimpleQuote>(1.0));
    // S&P spot price (note S&P is US listed)
    Handle<Quote> spSpotToday(boost::make_shared<SimpleQuote>(2100.0));
    // Lufthansa spot price (note LH is EU listed)
    Handle<Quote> lhSpotToday(boost::make_shared<SimpleQuote>(12.50));

    Handle<YieldTermStructure> eqDivSp(boost::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> eqDivLh(boost::make_shared<FlatForward>(referenceDate, 0.0075, Actual365Fixed()));

    boost::shared_ptr<FxBsParametrization> fxUsdEurBsParam =
        boost::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), usdEurSpotToday, fxTimes, fxSigmas);

    boost::shared_ptr<EqBsParametrization> eqSpBsParam = 
        boost::make_shared<EqBsPiecewiseConstantParametrization>(
            USDCurrency(), "SP", spSpotToday, usdEurSpotToday, 
            eqSpTimes, spSigmas, usdYts, eqDivSp);

    boost::shared_ptr<EqBsParametrization> eqLhBsParam =
        boost::make_shared<EqBsPiecewiseConstantParametrization>(
            EURCurrency(), "LH", lhSpotToday, eurEurSpotToday,
            eqLhTimes, lhSigmas, eurYts, eqDivLh);

    std::vector<boost::shared_ptr<Parametrization> > singleModels;
    singleModels.push_back(eurLgmParam);
    singleModels.push_back(usdLgmParam);
    singleModels.push_back(fxUsdEurBsParam);
    singleModels.push_back(eqSpBsParam);
    singleModels.push_back(eqLhBsParam);

    boost::shared_ptr<CrossAssetModel> ccLgm = boost::make_shared<CrossAssetModel>(singleModels);

    Size eurIdx = ccLgm->ccyIndex(EURCurrency());
    Size usdIdx = ccLgm->ccyIndex(USDCurrency());
    Size eurUsdIdx = usdIdx - 1;
    Size eqSpIdx = ccLgm->eqIndex("SP");
    Size eqLhIdx = ccLgm->eqIndex("LH");

    ccLgm->correlation(IR, eurIdx, IR, usdIdx, -0.2);
    ccLgm->correlation(IR, eurIdx, FX, eurUsdIdx, 0.8);
    ccLgm->correlation(IR, usdIdx, FX, eurUsdIdx, -0.5);
    ccLgm->correlation(EQ, eqSpIdx, EQ, eqLhIdx, 0.6);
    ccLgm->correlation(EQ, eqSpIdx, IR, usdIdx, -0.1);
    ccLgm->correlation(EQ, eqLhIdx, IR, eurIdx, -0.05);

    boost::shared_ptr<LinearGaussMarkovModel> eurLgm = boost::make_shared<LinearGaussMarkovModel>(eurLgmParam);
    boost::shared_ptr<LinearGaussMarkovModel> usdLgm = boost::make_shared<LinearGaussMarkovModel>(usdLgmParam);

    boost::shared_ptr<StochasticProcess> process = ccLgm->stateProcess(CrossAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> process2 = ccLgm->stateProcess(CrossAssetStateProcess::euler);

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
        //Sample<MultiPath> path = pg2.next();
        Size l = path.value[0].length() - 1;
        Real eurusdfx = std::exp(path.value[2][l]);
        Real zeur = path.value[0][l];
        Real zusd = path.value[1][l];
        Real eqsp = std::exp(path.value[3][l]);
        Real eqlh = std::exp(path.value[4][l]);
        Real ccnum = ccLgm->numeraire(0, T, zeur);

        // 1 LH forward settled at T deflated with numeraire
        Real lhFwd = eqlh - strikeLh;
        stat1(lhFwd / ccnum);

        // 2 SP forward settled at T (converted to base) deflated with numeraire
        Real spFwd = eurusdfx * (eqsp - strikeSp);
        stat2(spFwd / ccnum);

        // 3 LH option exercise at T deflated with numeraire
        Real lhCall = std::max(lhFwd,0.0);
        Real lhPut = std::max(-1.0*lhFwd, 0.0);
        stat3a(lhCall / ccnum);
        stat3b(lhPut / ccnum);

        // 4 SP option exercise at T (converted to base) deflated with numeraire
        Real spCall = std::max(spFwd, 0.0);
        Real spPut = std::max(-1.0*spFwd, 0.0);
        stat4a(spCall / ccnum);
        stat4b(spPut / ccnum);
    }

    Date tradeMaturity = referenceDate + 5 * 365;

    boost::shared_ptr<EquityForward> lhFwdTrade =
        boost::make_shared<EquityForward>("LH", EURCurrency(), Position::Long, 1, tradeMaturity, strikeLh);
    boost::shared_ptr<EquityForward> spFwdTrade =
        boost::make_shared<EquityForward>("SP", USDCurrency(), Position::Long, 1, tradeMaturity, strikeSp);

    boost::shared_ptr<VanillaOption> lhCall =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Call, strikeLh),
                                          boost::make_shared<EuropeanExercise>(referenceDate + 5 * 365));
    boost::shared_ptr<VanillaOption> lhPut =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Put, strikeLh),
            boost::make_shared<EuropeanExercise>(referenceDate + 5 * 365));
    boost::shared_ptr<VanillaOption> spCall =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Call, strikeSp),
            boost::make_shared<EuropeanExercise>(referenceDate + 5 * 365));
    boost::shared_ptr<VanillaOption> spPut =
        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Put, strikeSp),
            boost::make_shared<EuropeanExercise>(referenceDate + 5 * 365));

    boost::shared_ptr<DiscountingEquityForwardEngine> lhFwdEngine =
        boost::make_shared<DiscountingEquityForwardEngine>(eurYts,eqDivLh,lhSpotToday,eurYts);
    boost::shared_ptr<DiscountingEquityForwardEngine> spFwdEngine =
        boost::make_shared<DiscountingEquityForwardEngine>(usdYts, eqDivSp, spSpotToday, usdYts);

    lhFwdTrade->setPricingEngine(lhFwdEngine);
    spFwdTrade->setPricingEngine(spFwdEngine);

    boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> spEqOptionEngine =
        boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(ccLgm, eqSpIdx, ccLgm->ccyIndex(ccLgm->eqbs(eqSpIdx)->currency()));
    boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> lhEqOptionEngine =
        boost::make_shared<AnalyticXAssetLgmEquityOptionEngine>(ccLgm, eqLhIdx, ccLgm->ccyIndex(ccLgm->eqbs(eqLhIdx)->currency()));

    lhCall->setPricingEngine(lhEqOptionEngine);
    lhPut->setPricingEngine(lhEqOptionEngine);
    spCall->setPricingEngine(spEqOptionEngine);
    spPut->setPricingEngine(spEqOptionEngine);

    Real npv1 = mean(stat1);
    Real error1 = error_of<tag::mean>(stat1);
    Real expected1 = lhFwdTrade->NPV();

    Real npv2 = mean(stat2);
    Real error2 = error_of<tag::mean>(stat2);
    Real expected2 = usdEurSpotToday->value() * spFwdTrade->NPV();

    Real npv3a = mean(stat3a);
    Real error3a = error_of<tag::mean>(stat3a);
    Real expected3a = lhCall->NPV();
    Real npv3b = mean(stat3b);
    Real error3b = error_of<tag::mean>(stat3b);
    Real expected3b = lhPut->NPV();

    Real npv4a = mean(stat4a);
    Real error4a = error_of<tag::mean>(stat4a);
    Real expected4a = usdEurSpotToday->value() * spCall->NPV();
    Real npv4b = mean(stat4b);
    Real error4b = error_of<tag::mean>(stat4b);
    Real expected4b = usdEurSpotToday->value() * spPut->NPV();

    /*
    BOOST_TEST_MESSAGE("TEST1: res= " << npv1 << ": exp = " << expected1 << ": err = " << error1);
    BOOST_TEST_MESSAGE("TEST2: res= " << npv2 << ": exp = " << expected2 << ": err = " << error2);
    BOOST_TEST_MESSAGE("TEST3a: res= " << npv3a << ": exp = " << expected3a << ": err = " << error3a);
    BOOST_TEST_MESSAGE("TEST3b: res= " << npv3b << ": exp = " << expected3b << ": err = " << error3b);
    BOOST_TEST_MESSAGE("TEST4a: res= " << npv4a << ": exp = " << expected4a << ": err = " << error4a);
    BOOST_TEST_MESSAGE("TEST4b: res= " << npv4b << ": exp = " << expected4b << ": err = " << error4b);
    */

    Real tolErrEst = 1.5; // allow absolute diffs to be within 1.5 standard errors
    // TODO : IS THIS STRICT ENOUGH?
    BOOST_CHECK_LE(std::fabs(npv1-expected1),tolErrEst*error1);
    BOOST_CHECK_LE(std::fabs(npv2 - expected2), tolErrEst*error2);
    BOOST_CHECK_LE(std::fabs(npv3a - expected3a), tolErrEst*error3a);
    BOOST_CHECK_LE(std::fabs(npv3b - expected3b), tolErrEst*error3b);
    BOOST_CHECK_LE(std::fabs(npv4a - expected4a), tolErrEst*error4a);
    BOOST_CHECK_LE(std::fabs(npv4b - expected4b), tolErrEst*error4b);

} // testEqLgm5fPayouts

namespace {

struct Lgm5fTestData {
    Lgm5fTestData()
        : referenceDate(30, July, 2015), eurYts(boost::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed())),
          usdYts(boost::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed())),
          gbpYts(boost::make_shared<FlatForward>(referenceDate, 0.04, Actual365Fixed())),
          fxEurUsd(boost::make_shared<SimpleQuote>(0.90)), fxEurGbp(boost::make_shared<SimpleQuote>(1.35)), c(5, 5) {

        Settings::instance().evaluationDate() = referenceDate;
        volstepdates.push_back(Date(15, July, 2016));
        volstepdates.push_back(Date(15, July, 2017));
        volstepdates.push_back(Date(15, July, 2018));
        volstepdates.push_back(Date(15, July, 2019));
        volstepdates.push_back(Date(15, July, 2020));

        volstepdatesFx.push_back(Date(15, July, 2016));
        volstepdatesFx.push_back(Date(15, October, 2016));
        volstepdatesFx.push_back(Date(15, May, 2017));
        volstepdatesFx.push_back(Date(13, September, 2017));
        volstepdatesFx.push_back(Date(15, July, 2018));

        volsteptimes_a = Array(volstepdates.size());
        volsteptimesFx_a = Array(volstepdatesFx.size());
        for (Size i = 0; i < volstepdates.size(); ++i) {
            volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
        }
        for (Size i = 0; i < volstepdatesFx.size(); ++i) {
            volsteptimesFx_a[i] = eurYts->timeFromReference(volstepdatesFx[i]);
        }

        for (Size i = 0; i < volstepdates.size() + 1; ++i) {
            eurVols.push_back(0.0050 + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdates.size() + 1; ++i) {
            usdVols.push_back(0.0030 + (0.0110 - 0.0030) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdates.size() + 1; ++i) {
            gbpVols.push_back(0.0070 + (0.0095 - 0.0070) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
            fxSigmasUsd.push_back(0.15 + (0.20 - 0.15) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
            fxSigmasGbp.push_back(0.10 + (0.15 - 0.10) * std::exp(-0.3 * static_cast<double>(i)));
        }
        eurVols_a = Array(eurVols.begin(), eurVols.end());
        usdVols_a = Array(usdVols.begin(), usdVols.end());
        gbpVols_a = Array(gbpVols.begin(), gbpVols.end());
        fxSigmasUsd_a = Array(fxSigmasUsd.begin(), fxSigmasUsd.end());
        fxSigmasGbp_a = Array(fxSigmasGbp.begin(), fxSigmasGbp.end());

        notimes_a = Array(0);
        eurKappa_a = Array(1, 0.02);
        usdKappa_a = Array(1, 0.03);
        gbpKappa_a = Array(1, 0.04);

        eurLgm_p = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYts, volsteptimes_a,
                                                                               eurVols_a, notimes_a, eurKappa_a);
        usdLgm_p = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(USDCurrency(), usdYts, volsteptimes_a,
                                                                               usdVols_a, notimes_a, usdKappa_a);
        gbpLgm_p = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(GBPCurrency(), gbpYts, volsteptimes_a,
                                                                               gbpVols_a, notimes_a, gbpKappa_a);

        fxUsd_p = boost::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), fxEurUsd, volsteptimesFx_a,
                                                                           fxSigmasUsd_a);
        fxGbp_p = boost::make_shared<FxBsPiecewiseConstantParametrization>(GBPCurrency(), fxEurGbp, volsteptimesFx_a,
                                                                           fxSigmasGbp_a);

        singleModels.push_back(eurLgm_p);
        singleModels.push_back(usdLgm_p);
        singleModels.push_back(gbpLgm_p);
        singleModels.push_back(fxUsd_p);
        singleModels.push_back(fxGbp_p);

        //     EUR           USD           GBP         FX USD-EUR      FX GBP-EUR
        c[0][0] = 1.0;
        c[0][1] = 0.6;
        c[0][2] = 0.3;
        c[0][3] = 0.2;
        c[0][4] = 0.3; // EUR
        c[1][0] = 0.6;
        c[1][1] = 1.0;
        c[1][2] = 0.1;
        c[1][3] = -0.2;
        c[1][4] = -0.1; // USD
        c[2][0] = 0.3;
        c[2][1] = 0.1;
        c[2][2] = 1.0;
        c[2][3] = 0.0;
        c[2][4] = 0.1; // GBP
        c[3][0] = 0.2;
        c[3][1] = -0.2;
        c[3][2] = 0.0;
        c[3][3] = 1.0;
        c[3][4] = 0.3; // FX USD-EUR
        c[4][0] = 0.3;
        c[4][1] = -0.1;
        c[4][2] = 0.1;
        c[4][3] = 0.3;
        c[4][4] = 1.0; // FX GBP-EUR

        ccLgm = boost::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None);
    }

    SavedSettings backup;
    Date referenceDate;
    Handle<YieldTermStructure> eurYts, usdYts, gbpYts;
    std::vector<Date> volstepdates, volstepdatesFx;
    Array volsteptimes_a, volsteptimesFx_a;
    std::vector<Real> eurVols, usdVols, gbpVols, fxSigmasUsd, fxSigmasGbp;
    Handle<Quote> fxEurUsd, fxEurGbp;
    Array eurVols_a, usdVols_a, gbpVols_a, fxSigmasUsd_a, fxSigmasGbp_a;
    Array notimes_a, eurKappa_a, usdKappa_a, gbpKappa_a;
    boost::shared_ptr<IrLgm1fParametrization> eurLgm_p, usdLgm_p, gbpLgm_p;
    boost::shared_ptr<FxBsParametrization> fxUsd_p, fxGbp_p;
    std::vector<boost::shared_ptr<Parametrization> > singleModels;
    Matrix c;
    boost::shared_ptr<CrossAssetModel> ccLgm;
};

} // anonymous namespace


void EquityCrossAssetModelTest::testLgm7fFullCalibration() {

    BOOST_ERROR("NOT YET IMPLEMENTED");
    BOOST_TEST_MESSAGE("Testing full calibration of Ccy LGM 5F model...");

    Lgm5fTestData d;

    // calibration baskets

    std::vector<boost::shared_ptr<CalibrationHelper> > basketEur, basketUsd, basketGbp, basketEurUsd, basketEurGbp;

    boost::shared_ptr<IborIndex> euribor6m = boost::make_shared<Euribor>(6 * Months, d.eurYts);
    boost::shared_ptr<IborIndex> usdLibor3m = boost::make_shared<USDLibor>(3 * Months, d.usdYts);
    boost::shared_ptr<IborIndex> gbpLibor3m = boost::make_shared<GBPLibor>(3 * Months, d.gbpYts);

    for (Size i = 0; i <= d.volstepdates.size(); ++i) {
        Date tmp = i < d.volstepdates.size() ? d.volstepdates[i] : d.volstepdates.back() + 365;
        // EUR: atm+200bp, 150bp normal vol
        basketEur.push_back(boost::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(boost::make_shared<SimpleQuote>(0.015)), euribor6m, 1 * Years, Thirty360(),
            Actual360(), d.eurYts, CalibrationHelper::RelativePriceError, 0.04, 1.0, Normal)));
        // USD: atm, 20%, lognormal vol
        basketUsd.push_back(boost::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(boost::make_shared<SimpleQuote>(0.30)), usdLibor3m, 1 * Years, Thirty360(),
            Actual360(), d.usdYts, CalibrationHelper::RelativePriceError, Null<Real>(), 1.0, ShiftedLognormal, 0.0)));
        // GBP: atm-200bp, 10%, shifted lognormal vol with shift = 2%
        basketGbp.push_back(boost::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(boost::make_shared<SimpleQuote>(0.30)), gbpLibor3m, 1 * Years, Thirty360(),
            Actual360(), d.usdYts, CalibrationHelper::RelativePriceError, 0.02, 1.0, ShiftedLognormal, 0.02)));
    }

    for (Size i = 0; i < d.volstepdatesFx.size(); ++i) {
        Date tmp = i < d.volstepdatesFx.size() ? d.volstepdatesFx[i] : d.volstepdatesFx.back() + 365;
        // EUR-USD: atm, 30% (lognormal) vol
        basketEurUsd.push_back(boost::make_shared<FxOptionHelper>(
            tmp, Null<Real>(), d.fxEurUsd, Handle<Quote>(boost::make_shared<SimpleQuote>(0.20)), d.eurYts, d.usdYts,
            CalibrationHelper::RelativePriceError));
        // EUR-GBP: atm, 10% (lognormal) vol
        basketEurGbp.push_back(boost::make_shared<FxOptionHelper>(
            tmp, Null<Real>(), d.fxEurGbp, Handle<Quote>(boost::make_shared<SimpleQuote>(0.20)), d.eurYts, d.gbpYts,
            CalibrationHelper::RelativePriceError));
    }

    // pricing engines

    boost::shared_ptr<PricingEngine> eurSwEng = boost::make_shared<AnalyticLgmSwaptionEngine>(d.ccLgm, 0);
    boost::shared_ptr<PricingEngine> usdSwEng = boost::make_shared<AnalyticLgmSwaptionEngine>(d.ccLgm, 1);
    boost::shared_ptr<PricingEngine> gbpSwEng = boost::make_shared<AnalyticLgmSwaptionEngine>(d.ccLgm, 2);

    boost::shared_ptr<AnalyticCcLgmFxOptionEngine> eurUsdFxoEng =
        boost::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgm, 0);
    boost::shared_ptr<AnalyticCcLgmFxOptionEngine> eurGbpFxoEng =
        boost::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgm, 1);

    eurUsdFxoEng->cache();
    eurGbpFxoEng->cache();

    // assign engines to calibration instruments
    for (Size i = 0; i < basketEur.size(); ++i) {
        basketEur[i]->setPricingEngine(eurSwEng);
    }
    for (Size i = 0; i < basketUsd.size(); ++i) {
        basketUsd[i]->setPricingEngine(usdSwEng);
    }
    for (Size i = 0; i < basketGbp.size(); ++i) {
        basketGbp[i]->setPricingEngine(gbpSwEng);
    }
    for (Size i = 0; i < basketEurUsd.size(); ++i) {
        basketEurUsd[i]->setPricingEngine(eurUsdFxoEng);
    }
    for (Size i = 0; i < basketEurGbp.size(); ++i) {
        basketEurGbp[i]->setPricingEngine(eurGbpFxoEng);
    }

    // calibrate the model

    LevenbergMarquardt lm(1E-8, 1E-8, 1E-8);
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);

    d.ccLgm->calibrateIrLgm1fVolatilitiesIterative(0, basketEur, lm, ec);
    d.ccLgm->calibrateIrLgm1fVolatilitiesIterative(1, basketUsd, lm, ec);
    d.ccLgm->calibrateIrLgm1fVolatilitiesIterative(2, basketGbp, lm, ec);

    d.ccLgm->calibrateBsVolatilitiesIterative(CrossAssetModelTypes::FX, 0, basketEurUsd, lm, ec);
    d.ccLgm->calibrateBsVolatilitiesIterative(CrossAssetModelTypes::FX, 1, basketEurGbp, lm, ec);

    // check the results

    Real tol = 1E-6;

    for (Size i = 0; i < basketEur.size(); ++i) {
        Real model = basketEur[i]->modelValue();
        Real market = basketEur[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in EUR basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
    for (Size i = 0; i < basketUsd.size(); ++i) {
        Real model = basketUsd[i]->modelValue();
        Real market = basketUsd[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in USD basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
    for (Size i = 0; i < basketGbp.size(); ++i) {
        Real model = basketGbp[i]->modelValue();
        Real market = basketGbp[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in GBP basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
    for (Size i = 0; i < basketEurUsd.size(); ++i) {
        Real model = basketEurUsd[i]->modelValue();
        Real market = basketEurUsd[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in EUR-USD basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
    for (Size i = 0; i < basketEurUsd.size(); ++i) {
        Real model = basketEurGbp[i]->modelValue();
        Real market = basketEurGbp[i]->marketValue();
        if (std::abs((model - market) / market) > tol)
            BOOST_ERROR("calibration failed for instrument #"
                        << i << " in EUR-GBP basket, model value is " << model << " market value is " << market
                        << " relative error " << std::abs((model - market) / market) << " tolerance " << tol);
    }
}

void EquityCrossAssetModelTest::testLgm7fMoments() {

    BOOST_ERROR("NOT YET IMPLEMENTED");
    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization "
                       "in Ccy LGM 5F model...");

    Lgm5fTestData d;

    boost::shared_ptr<StochasticProcess> p_exact = d.ccLgm->stateProcess(CrossAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> p_euler = d.ccLgm->stateProcess(CrossAssetStateProcess::euler);

    Real T = 10.0;                            // horizon at which we compare the moments
    Size steps = static_cast<Size>(T * 10.0); // number of simulation steps
    Size paths = 25000;                       // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);

    TimeGrid grid(T, steps);

    MultiPathGeneratorSobolBrownianBridge pgen(p_euler, grid);
    MultiPathGeneratorSobolBrownianBridge pgen2(p_exact, grid);

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

    Real errTolLd[] = { 0.2E-4, 0.2E-4, 0.2E-4, 10.0E-4, 10.0E-4 };

    for (Size i = 0; i < 5; ++i) {
        // check expectation against analytical calculation (Euler)
        if (std::fabs(mean(e_eu[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #"
                        << i << " (" << e_an[i] << ") is inconsistent with numerical value (Euler "
                                                   "discretization, " << mean(e_eu[i]) << "), error is "
                        << e_an[i] - mean(e_eu[i]) << " tolerance is " << errTolLd[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #"
                        << i << " (" << e_an[i] << ") is inconsistent with numerical value (Exact "
                                                   "discretization, " << mean(e_eu2[i]) << "), error is "
                        << e_an[i] - mean(e_eu2[i]) << " tolerance is " << errTolLd[i]);
        }
    }

    // we have to deal with different natures of volatility
    // for ir (normal) and fx (ln) so different error
    // tolerances apply
    Real tollNormal = 0.1E-4; // ir-ir
    Real tolMixed = 0.25E-4;  // ir-fx
    Real tolLn = 8.0E-4;      // fx-fx
    Real tol;                 // set below

    for (Size i = 0; i < 5; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (i < 3) {
                tol = tollNormal;
            } else {
                if (j < 3) {
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

} // testLgm5fMoments

test_suite* EquityCrossAssetModelTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("EquityCrossAssetModelTests");
    suite->add(QUANTLIB_TEST_CASE(&EquityCrossAssetModelTest::testEqLgm5fPayouts));
    suite->add(QUANTLIB_TEST_CASE(&EquityCrossAssetModelTest::testLgm7fFullCalibration));
    suite->add(QUANTLIB_TEST_CASE(&EquityCrossAssetModelTest::testLgm7fMoments));
    return suite;
}
}
