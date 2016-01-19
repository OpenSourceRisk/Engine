/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include "xassetmodel.hpp"
#include "utilities.hpp"

#include <qle/methods/multipathgenerator.hpp>
#include <qle/models/all.hpp>
#include <qle/processes/all.hpp>
#include <qle/pricingengines/all.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/currencies/america.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/quotes/simplequote.hpp>

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

struct BermudanTestData {
    BermudanTestData()
        : evalDate(12, January, 2015), yts(boost::make_shared<FlatForward>(
                                           evalDate, 0.02, Actual365Fixed())),
          euribor6m(boost::make_shared<Euribor>(6 * Months, yts)),
          effectiveDate(TARGET().advance(evalDate, 2 * Days)),
          startDate(TARGET().advance(effectiveDate, 1 * Years)),
          maturityDate(TARGET().advance(startDate, 9 * Years)),
          fixedSchedule(startDate, maturityDate, 1 * Years, TARGET(),
                        ModifiedFollowing, ModifiedFollowing,
                        DateGeneration::Forward, false),
          floatingSchedule(startDate, maturityDate, 6 * Months, TARGET(),
                           ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false),
          underlying(boost::make_shared<VanillaSwap>(VanillaSwap(
              VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(),
              floatingSchedule, euribor6m, 0.0, Actual360()))),
          reversion(0.03) {
        Settings::instance().evaluationDate() = evalDate;
        for (Size i = 0; i < 9; ++i) {
            exerciseDates.push_back(
                TARGET().advance(fixedSchedule[i], -2 * Days));
        }
        exercise = boost::make_shared<BermudanExercise>(exerciseDates, false);

        swaption = boost::make_shared<Swaption>(underlying, exercise);
        stepDates =
            std::vector<Date>(exerciseDates.begin(), exerciseDates.end() - 1);
        sigmas = std::vector<Real>(stepDates.size() + 1);
        for (Size i = 0; i < sigmas.size(); ++i) {
            sigmas[i] =
                0.0050 +
                (0.0080 - 0.0050) * std::exp(-0.2 * static_cast<double>(i));
        }
        stepTimes_a = Array(stepDates.size());
        for (Size i = 0; i < stepDates.size(); ++i) {
            stepTimes_a[i] = yts->timeFromReference(stepDates[i]);
        }
        sigmas_a = Array(sigmas.begin(), sigmas.end());
        kappas_a = Array(sigmas_a.size(), reversion);
    }
    SavedSettings backup;
    Date evalDate;
    Handle<YieldTermStructure> yts;
    boost::shared_ptr<IborIndex> euribor6m;
    Date effectiveDate, startDate, maturityDate;
    Schedule fixedSchedule, floatingSchedule;
    boost::shared_ptr<VanillaSwap> underlying;
    std::vector<Date> exerciseDates, stepDates;
    std::vector<Real> sigmas;
    boost::shared_ptr<Exercise> exercise;
    boost::shared_ptr<Swaption> swaption;
    Array stepTimes_a, sigmas_a, kappas_a;
    Real reversion;
}; // BermudanTestData

} // anonymous namespace

void XAssetModelTest::testBermudanLgm1fGsr() {

    BOOST_TEST_MESSAGE("Testing consistency of Bermudan swaption pricing in "
                       "LGM 1F and GSR models...");

    BermudanTestData d;

    // we use the Hull White adaptor for the LGM parametrization
    // which should lead to equal Bermudan swaption prices
    boost::shared_ptr<IrLgm1fParametrization> lgm_p = boost::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), d.yts, d.stepTimes_a, d.sigmas_a, d.kappas_a);

    // fix any T forward measure
    boost::shared_ptr<Gsr> gsr = boost::make_shared<Gsr>(
        d.yts, d.stepDates, d.sigmas, d.reversion, 50.0);

    boost::shared_ptr<Lgm> lgm = boost::make_shared<Lgm>(lgm_p);

    boost::shared_ptr<Gaussian1dModel> lgm_g1d =
        boost::make_shared<Gaussian1dXAssetAdaptor>(lgm);

    boost::shared_ptr<PricingEngine> swaptionEngineGsr =
        boost::make_shared<Gaussian1dSwaptionEngine>(gsr, 64, 7.0, true, false);

    boost::shared_ptr<PricingEngine> swaptionEngineLgm =
        boost::make_shared<Gaussian1dSwaptionEngine>(lgm_g1d, 64, 7.0, true,
                                                     false);

    d.swaption->setPricingEngine(swaptionEngineGsr);
    Real npvGsr = d.swaption->NPV();
    d.swaption->setPricingEngine(swaptionEngineLgm);
    Real npvLgm = d.swaption->NPV();

    Real tol = 0.2E-4; // basis point tolerance

    if (std::fabs(npvGsr - npvLgm) > tol)
        BOOST_ERROR("Failed to verify consistency of Bermudan swaption price "
                    "in IrLgm1f ("
                    << npvLgm << ") and Gsr (" << npvGsr
                    << ") models, tolerance is " << tol);
}

void XAssetModelTest::testBermudanLgmInvariances() {

    BOOST_TEST_MESSAGE("Testing LGM model invariances for Bermudan "
                       "swaption pricing...");

    BermudanTestData d;

    boost::shared_ptr<IrLgm1fParametrization> lgm_p2 =
        boost::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            EURCurrency(), d.yts, d.stepTimes_a, d.sigmas_a, d.kappas_a);

    boost::shared_ptr<Lgm> lgm2 = boost::make_shared<Lgm>(lgm_p2);

    boost::shared_ptr<Gaussian1dModel> lgm_g1d2 =
        boost::make_shared<Gaussian1dXAssetAdaptor>(lgm2);

    boost::shared_ptr<PricingEngine> swaptionEngineLgm2 =
        boost::make_shared<Gaussian1dSwaptionEngine>(lgm_g1d2, 64, 7.0, true, false);

    d.swaption->setPricingEngine(swaptionEngineLgm2);
    Real npvLgm = d.swaption->NPV();

    lgm_p2->shift() = -5.0;
    lgm_p2->scaling() = 3.0;

    // parametrizations are not observed, so we have to call update ourselves
    lgm2->update();

    Real npvLgm2 = d.swaption->NPV();

    Real tol = 1.0E-5;

    if (std::fabs(npvLgm - npvLgm2) > tol)
        BOOST_ERROR("Failed to verify consistency of Bermudan swaption price "
                    "under LGM model invariances, difference is "
                    << (npvLgm - npvLgm2));

} // testBermudanLgm1fGsr

void XAssetModelTest::testLgm1fCalibration() {

    BOOST_TEST_MESSAGE("Testing calibration of LGM 1F model (analytic engine) "
                       "against GSR parameters...");

    // for fixed kappa != 0.0 we calibrate sigma via
    // the Hull White Adaptor

    SavedSettings backup;

    Date evalDate(12, January, 2015);
    Settings::instance().evaluationDate() = evalDate;
    Handle<YieldTermStructure> yts(
        boost::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed()));
    boost::shared_ptr<IborIndex> euribor6m =
        boost::make_shared<Euribor>(6 * Months, yts);

    // coterminal basket 1y-9y, 2y-8y, ... 9y-1y

    std::vector<boost::shared_ptr<CalibrationHelper> > basket;
    Real impliedVols[] = {0.4, 0.39, 0.38, 0.35, 0.35, 0.34, 0.33, 0.32, 0.31};
    std::vector<Date> expiryDates;

    for (Size i = 0; i < 9; ++i) {
        boost::shared_ptr<CalibrationHelper> helper =
            boost::make_shared<SwaptionHelper>(
                (i + 1) * Years, (9 - i) * Years,
                Handle<Quote>(boost::make_shared<SimpleQuote>(impliedVols[i])),
                euribor6m, 1 * Years, Thirty360(), Actual360(), yts);
        basket.push_back(helper);
        expiryDates.push_back(boost::static_pointer_cast<SwaptionHelper>(helper)
                                  ->swaption()
                                  ->exercise()
                                  ->dates()
                                  .back());
    }

    std::vector<Date> stepDates(expiryDates.begin(), expiryDates.end() - 1);

    Array stepTimes_a(stepDates.size());
    for (Size i = 0; i < stepDates.size(); ++i) {
        stepTimes_a[i] = yts->timeFromReference(stepDates[i]);
    }

    Real kappa = 0.05;

    std::vector<Real> gsrInitialSigmas(stepDates.size() + 1, 0.0050);
    std::vector<Real> lgmInitialSigmas2(stepDates.size() + 1, 0.0050);

    Array lgmInitialSigmas2_a(lgmInitialSigmas2.begin(),
                              lgmInitialSigmas2.end());
    Array kappas_a(lgmInitialSigmas2_a.size(), kappa);

    boost::shared_ptr<IrLgm1fParametrization> lgm_p =
        boost::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            EURCurrency(), yts, stepTimes_a, lgmInitialSigmas2_a, kappas_a);

    // fix any T forward measure
    boost::shared_ptr<Gsr> gsr =
        boost::make_shared<Gsr>(yts, stepDates, gsrInitialSigmas, kappa, 50.0);

    boost::shared_ptr<Lgm> lgm = boost::make_shared<Lgm>(lgm_p);

    boost::shared_ptr<PricingEngine> swaptionEngineGsr =
        boost::make_shared<Gaussian1dSwaptionEngine>(gsr, 64, 7.0, true, false);

    boost::shared_ptr<PricingEngine> swaptionEngineLgm =
        boost::make_shared<AnalyticLgmSwaptionEngine>(lgm);

    // calibrate GSR

    LevenbergMarquardt lm(1E-8, 1E-8, 1E-8);
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);

    for (Size i = 0; i < basket.size(); ++i) {
        basket[i]->setPricingEngine(swaptionEngineGsr);
    }

    gsr->calibrateVolatilitiesIterative(basket, lm, ec);

    Array gsrSigmas = gsr->volatility();

    // calibrate LGM

    for (Size i = 0; i < basket.size(); ++i) {
        basket[i]->setPricingEngine(swaptionEngineLgm);
    }

    lgm->calibrateVolatilitiesIterative(basket, lm, ec);

    Array lgmSigmas = lgm->parametrization()->parameterValues(0);

    Real tol0 = 1E-8;
    Real tol = 2E-5;

    for (Size i = 0; i < gsrSigmas.size(); ++i) {
        // check calibration itself, we should match the market prices
        // rather exactly (note that this tests the lgm calibration,
        // not the gsr calibration)
        if (std::fabs(basket[i]->modelValue() - basket[i]->marketValue()) >
            tol0)
            BOOST_ERROR("Failed to calibrate to market swaption #"
                        << i << ", market price is " << basket[i]->marketValue()
                        << " while model price is " << basket[i]->modelValue());
        // compare calibrated model parameters
        if (std::fabs(gsrSigmas[i] - lgmSigmas[i]) > tol)
            BOOST_ERROR(
                "Failed to verify LGM's sigma from Hull White adaptor (#"
                << i << "), which is " << lgmSigmas[i]
                << " while GSR's sigma is " << gsrSigmas[i] << ")");
    }

    // calibrate LGM as component of XAssetModel

    // create a second set of parametrization ...
    boost::shared_ptr<IrLgm1fParametrization> lgm_p21 =
        boost::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            USDCurrency(), yts, stepTimes_a, lgmInitialSigmas2_a, kappas_a);
    boost::shared_ptr<IrLgm1fParametrization> lgm_p22 =
        boost::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            EURCurrency(), yts, stepTimes_a, lgmInitialSigmas2_a, kappas_a);

    // ... and a fx parametrization ...
    Array notimes_a(0);
    Array sigma_a(1, 0.10);
    boost::shared_ptr<FxBsParametrization> fx_p =
        boost::make_shared<FxBsPiecewiseConstantParametrization>(
            EURCurrency(), Handle<Quote>(boost::make_shared<SimpleQuote>(1.00)),
            notimes_a, sigma_a);

    // ... and set up an xasset model with USD as domestic currency ...
    std::vector<boost::shared_ptr<Parametrization> > parametrizations;
    parametrizations.push_back(lgm_p21);
    parametrizations.push_back(lgm_p22);
    parametrizations.push_back(fx_p);
    Matrix rho(3, 3, 0.0);
    rho[0][0] = rho[1][1] = rho[2][2] = 1.0;
    boost::shared_ptr<XAssetModel> xmodel = boost::make_shared<XAssetModel>(
        parametrizations, rho, SalvagingAlgorithm::None);

    // .. whose EUR component we calibrate as before and compare the
    // result against the 1d case and as well check that the USD
    // component was not touched by the calibration.

    boost::shared_ptr<PricingEngine> swaptionEngineLgm2 =
        boost::make_shared<AnalyticLgmSwaptionEngine>(xmodel, 1);

    for (Size i = 0; i < basket.size(); ++i) {
        basket[i]->setPricingEngine(swaptionEngineLgm2);
    }

    xmodel->calibrateIrLgm1fVolatilitiesIterative(1, basket, lm, ec);

    Array lgmSigmas2eur = xmodel->irlgm1f(1)->parameterValues(0);
    Array lgmSigmas2usd = xmodel->irlgm1f(0)->parameterValues(0);

    for (Size i = 0; i < gsrSigmas.size(); ++i) {
        // compare calibrated model parameters against 1d calibration before
        if (!close_enough(lgmSigmas2eur[i], lgmSigmas[i]))
            BOOST_ERROR("Failed to verify xasset LGM1F component calibration "
                        "at parameter #"
                        << i << " against 1d calibration, which is "
                        << lgmSigmas2eur[i] << " while 1d calibration was "
                        << lgmSigmas[i] << ")");
        // compare usd component against start values (since it was not
        // calibrated, so should not have changed)
        if (!close_enough(lgmSigmas2usd[i], lgmInitialSigmas2[i]))
            BOOST_ERROR("Non calibrated xasset LGM1F component was changed by "
                        "other's component calibration at #"
                        << i << ", the new value is " << lgmSigmas2usd[i]
                        << " while the initial value was "
                        << lgmInitialSigmas2[i]);
    }

} // testLgm1fCalibration

void XAssetModelTest::testCcyLgm3fForeignPayouts() {

    BOOST_TEST_MESSAGE("Testing pricing of foreign payouts under domestic "
                       "measure in Ccy LGM 3F model...");

    SavedSettings backup;

    Date referenceDate(30, July, 2015);

    Settings::instance().evaluationDate() = referenceDate;

    Handle<YieldTermStructure> eurYts(
        boost::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    Handle<YieldTermStructure> usdYts(
        boost::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed()));

    // use different grids for the EUR and USD  models and the FX volatility
    // process to test the piecewise numerical integration ...

    std::vector<Date> volstepdatesEur, volstepdatesUsd, volstepdatesFx;

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
    volstepdatesFx.push_back(Date(15, July, 2018)); //  shared with EUR and USD

    std::vector<Real> eurVols, usdVols, fxVols;

    for (Size i = 0; i < volstepdatesEur.size() + 1; ++i) {
        eurVols.push_back(0.0050 +
                          (0.0080 - 0.0050) *
                              std::exp(-0.3 * static_cast<double>(i)));
    }
    for (Size i = 0; i < volstepdatesUsd.size() + 1; ++i) {
        usdVols.push_back(0.0030 +
                          (0.0110 - 0.0030) *
                              std::exp(-0.3 * static_cast<double>(i)));
    }
    for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
        fxVols.push_back(
            0.15 + (0.20 - 0.15) * std::exp(-0.3 * static_cast<double>(i)));
    }

    Array alphaTimesEur(volstepdatesEur.size()),
        alphaEur(eurVols.begin(), eurVols.end()), kappaTimesEur(0),
        kappaEur(1, 0.02);
    Array alphaTimesUsd(volstepdatesUsd.size()),
        alphaUsd(usdVols.begin(), usdVols.end()), kappaTimesUsd(0),
        kappaUsd(1, 0.04);
    Array fxTimes(volstepdatesFx.size()),
        fxSigmas(fxVols.begin(), fxVols.end());

    for (Size i = 0; i < alphaTimesEur.size(); ++i) {
        alphaTimesEur[i] = eurYts->timeFromReference(volstepdatesEur[i]);
    }
    for (Size i = 0; i < alphaTimesUsd.size(); ++i) {
        alphaTimesUsd[i] = eurYts->timeFromReference(volstepdatesUsd[i]);
    }
    for (Size i = 0; i < fxTimes.size(); ++i) {
        fxTimes[i] = eurYts->timeFromReference(volstepdatesFx[i]);
    }

    boost::shared_ptr<IrLgm1fParametrization> eurLgmParam =
        boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            EURCurrency(), eurYts, alphaTimesEur, alphaEur, kappaTimesEur,
            kappaEur);

    boost::shared_ptr<IrLgm1fParametrization> usdLgmParam =
        boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            USDCurrency(), usdYts, alphaTimesUsd, alphaUsd, kappaTimesUsd,
            kappaUsd);

    // USD per EUR (foreign per domestic)
    Handle<Quote> usdEurSpotToday(boost::make_shared<SimpleQuote>(0.90));

    boost::shared_ptr<FxBsParametrization> fxUsdEurBsParam =
        boost::make_shared<FxBsPiecewiseConstantParametrization>(
            USDCurrency(), usdEurSpotToday, fxTimes, fxSigmas);

    std::vector<boost::shared_ptr<Parametrization> > singleModels;
    singleModels.push_back(eurLgmParam);
    singleModels.push_back(usdLgmParam);
    singleModels.push_back(fxUsdEurBsParam);

    Matrix c(3, 3);
    //  EUR            USD              FX
    c[0][0] = 1.0;  c[0][1] = -0.2; c[0][2] = 0.8; // EUR
    c[1][0] = -0.2; c[1][1] = 1.0;  c[1][2] = -0.5; // USD
    c[2][0] = 0.8;  c[2][1] = -0.5; c[2][2] = 1.0; // FX

    boost::shared_ptr<XAssetModel> ccLgm = boost::make_shared<XAssetModel>(
        singleModels, c, SalvagingAlgorithm::None);

    boost::shared_ptr<Lgm> eurLgm = boost::make_shared<Lgm>(eurLgmParam);
    boost::shared_ptr<Lgm> usdLgm = boost::make_shared<Lgm>(usdLgmParam);

    boost::shared_ptr<StochasticProcess> process =
        ccLgm->stateProcess(XAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> usdProcess = usdLgm->stateProcess();

    // path generation

    Size n = 500000; // number of paths
    Size seed = 121; // seed
    // maturity of test payoffs
    Time T = 5.0;
    // take large steps, but not only one (since we are testing)
    Size steps = static_cast<Size>(T * 2.0);
    TimeGrid grid(T, steps);
    PseudoRandom::rsg_type sg =
        PseudoRandom::make_sequence_generator(3 * steps, seed);
    PseudoRandom::rsg_type sg2 =
        PseudoRandom::make_sequence_generator(steps, seed);

    QuantExt::MultiPathGenerator<PseudoRandom::rsg_type> pg(process, grid, sg,
                                                            false);
    PathGenerator<PseudoRandom::rsg_type> pg2(usdProcess, grid, sg2, false);

    // test
    // 1 deterministic USD cashflow under EUR numeraire vs. price on USD curve
    // 2 zero bond option USD under EUR numeraire vs. USD numeraire
    // 3 fx option USD-EUR under EUR numeraire vs. analytical price

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > stat1,
        stat2a, stat2b, stat3;

    // same for paths2 since shared time grid
    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path = pg.next();
        Sample<Path> path2 = pg2.next();
        Size l = path.value[0].length() - 1;
        Real fx = std::exp(path.value[2][l]);
        Real zeur = path.value[0][l];
        Real zusd = path.value[1][l];
        Real zusd2 = path2.value[l];

        // 1 USD paid at T deflated with EUR numeraire
        stat1(1.0 * fx / eurLgm->numeraire(T, zeur));

        // 2 USD zero bond option at T on P(T,T+10) strike 0.5 ...
        // ... under EUR numeraire ...
        Real zbOpt =
            std::max(usdLgm->discountBond(T, T + 10.0, zusd) - 0.5, 0.0);
        stat2a(zbOpt * fx / eurLgm->numeraire(T, zeur));
        // ... and under USD numeraire ...
        Real zbOpt2 =
            std::max(usdLgm->discountBond(T, T + 10.0, zusd2) - 0.5, 0.0);
        stat2b(zbOpt2 / usdLgm->numeraire(T, zusd2));

        // 3 USD-EUR fx option @0.9
        stat3(std::max(fx - 0.9, 0.0) / eurLgm->numeraire(T, zeur));
    }

    boost::shared_ptr<VanillaOption> fxOption =
        boost::make_shared<VanillaOption>(
            boost::make_shared<PlainVanillaPayoff>(Option::Call, 0.9),
            boost::make_shared<EuropeanExercise>(referenceDate + 5 * 365));

    boost::shared_ptr<PricingEngine> ccLgmFxOptionEngine =
        boost::make_shared<AnalyticCcLgmFxOptionEngine>(ccLgm, 0);

    fxOption->setPricingEngine(ccLgmFxOptionEngine);

    Real npv1 = mean(stat1);
    Real error1 = error_of<tag::mean>(stat1);
    Real expected1 = usdYts->discount(5.0) * usdEurSpotToday->value();
    Real npv2a = mean(stat2a);
    Real error2a = error_of<tag::mean>(stat2a);
    Real npv2b = mean(stat2b) * usdEurSpotToday->value();
    ;
    Real error2b = error_of<tag::mean>(stat2b) * usdEurSpotToday->value();
    Real npv3 = mean(stat3);
    Real error3 = error_of<tag::mean>(stat3);

    // accept this relative difference in error estimates
    Real tolError = 0.2;
    // accept tolErrEst*errorEstimate as absolute difference
    Real tolErrEst = 1.0;

    if (std::fabs((error1 - 4E-4) / 4E-4) > tolError)
        BOOST_ERROR("error estimate deterministic "
                    "cashflow pricing can not be "
                    "reproduced, is "
                    << error1 << ", expected 4E-4, relative tolerance "
                    << tolError);
    if (std::fabs((error2a - 1E-4) / 1E-4) > tolError)
        BOOST_ERROR("error estimate zero bond "
                    "option pricing (foreign measure) can "
                    "not be reproduced, is "
                    << error2a << ", expected 1E-4, relative tolerance "
                    << tolError);
    if (std::fabs((error2b - 7E-5) / 7E-5) > tolError)
        BOOST_ERROR("error estimate zero bond "
                    "option pricing (domestic measure) can "
                    "not be reproduced, is "
                    << error2b << ", expected 7E-5, relative tolerance "
                    << tolError);
    if (std::fabs((error3 - 2.7E-4) / 2.7E-4) > tolError)
        BOOST_ERROR(
            "error estimate fx option pricing can not be reproduced, is "
            << error3 << ", expected 2.7E-4, relative tolerance " << tolError);

    if (std::fabs(npv1 - expected1) > tolErrEst * error1)
        BOOST_ERROR("can no reproduce deterministic cashflow pricing, is "
                    << npv1 << ", expected " << expected1 << ", tolerance "
                    << tolErrEst << "*" << error1);

    if (std::fabs(npv2a - npv2b) >
        tolErrEst * std::sqrt(error2a * error2a + error2b * error2b))
        BOOST_ERROR("can no reproduce zero bond option pricing, domestic "
                    "measure result is "
                    << npv2a << ", foreign measure result is " << npv2b
                    << ", tolerance " << tolErrEst << "*"
                    << std::sqrt(error2a * error2a + error2b * error2b));

    if (std::fabs(npv3 - fxOption->NPV()) > tolErrEst * std::sqrt(error3))
        BOOST_ERROR("can no reproduce fx option pricing, monte carlo result is "
                    << npv3 << ", analytical pricing result is "
                    << fxOption->NPV() << ", tolerance is " << tolErrEst << "*"
                    << error3);

} // testCcyLgm3fForeignPayouts

namespace {

struct Lgm5fTestData {
    Lgm5fTestData()
        : referenceDate(30, July, 2015),
          eurYts(boost::make_shared<FlatForward>(referenceDate, 0.02,
                                                 Actual365Fixed())),
          usdYts(boost::make_shared<FlatForward>(referenceDate, 0.05,
                                                 Actual365Fixed())),
          gbpYts(boost::make_shared<FlatForward>(referenceDate, 0.04,
                                                 Actual365Fixed())),
          fxEurUsd(boost::make_shared<SimpleQuote>(0.90)),
          fxEurGbp(boost::make_shared<SimpleQuote>(1.35)), c(5, 5) {

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
            eurVols.push_back(0.0050 +
                              (0.0080 - 0.0050) *
                                  std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdates.size() + 1; ++i) {
            usdVols.push_back(0.0030 +
                              (0.0110 - 0.0030) *
                                  std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdates.size() + 1; ++i) {
            gbpVols.push_back(0.0070 +
                              (0.0095 - 0.0070) *
                                  std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
            fxSigmasUsd.push_back(
                0.15 + (0.20 - 0.15) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
            fxSigmasGbp.push_back(
                0.10 + (0.15 - 0.10) * std::exp(-0.3 * static_cast<double>(i)));
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

        eurLgm_p = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            EURCurrency(), eurYts, volsteptimes_a, eurVols_a, notimes_a,
            eurKappa_a);
        usdLgm_p = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            USDCurrency(), usdYts, volsteptimes_a, usdVols_a, notimes_a,
            usdKappa_a);
        gbpLgm_p = boost::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            GBPCurrency(), gbpYts, volsteptimes_a, gbpVols_a, notimes_a,
            gbpKappa_a);

        fxUsd_p = boost::make_shared<FxBsPiecewiseConstantParametrization>(
            USDCurrency(), fxEurUsd, volsteptimesFx_a, fxSigmasUsd_a);
        fxGbp_p = boost::make_shared<FxBsPiecewiseConstantParametrization>(
            GBPCurrency(), fxEurGbp, volsteptimesFx_a, fxSigmasGbp_a);

        singleModels.push_back(eurLgm_p);
        singleModels.push_back(usdLgm_p);
        singleModels.push_back(gbpLgm_p);
        singleModels.push_back(fxUsd_p);
        singleModels.push_back(fxGbp_p);

        //     EUR           USD           GBP         FX USD-EUR      FX GBP-EUR
        c[0][0] = 1.0; c[0][1] = 0.6;  c[0][2] = 0.3;  c[0][3] = 0.2;  c[0][4] = 0.3; // EUR
        c[1][0] = 0.6; c[1][1] = 1.0;  c[1][2] = 0.1;  c[1][3] = -0.2; c[1][4] =-0.1; // USD
        c[2][0] = 0.3; c[2][1] = 0.1;  c[2][2] = 1.0;  c[2][3] = 0.0;  c[2][4] = 0.1; // GBP
        c[3][0] = 0.2; c[3][1] = -0.2; c[3][2] = 0.0;  c[3][3] = 1.0;  c[3][4] = 0.3; // FX USD-EUR
        c[4][0] = 0.3; c[4][1] = -0.1; c[4][2] = 0.1;  c[4][3] = 0.3;  c[4][4] = 1.0; // FX GBP-EUR

        ccLgm = boost::make_shared<XAssetModel>(singleModels, c,
                                                SalvagingAlgorithm::None);
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
    boost::shared_ptr<XAssetModel> ccLgm;
};

} // anonymous namespace

void XAssetModelTest::testLgm5fFxCalibration() {

    BOOST_TEST_MESSAGE("Testing fx calibration in Ccy LGM 5F model...");

    Lgm5fTestData d;

    // we test the 5f model against the 3f model eur-gbp
    std::vector<boost::shared_ptr<Parametrization> > singleModelsProjected;
    singleModelsProjected.push_back(d.eurLgm_p);
    singleModelsProjected.push_back(d.gbpLgm_p);
    singleModelsProjected.push_back(d.fxGbp_p);

    Matrix cProjected(3, 3);
    for (Size i = 0, ii = 0; i < 5; ++i) {
        if (i != 0 && i != 3) {
            for (Size j = 0, jj = 0; j < 5; ++j) {
                if (j != 0 && j != 3)
                    cProjected[ii][jj++] = d.c[i][j];
            }
            ++ii;
        }
    }

    boost::shared_ptr<XAssetModel> ccLgmProjected =
        boost::make_shared<XAssetModel>(singleModelsProjected, cProjected,
                                        SalvagingAlgorithm::None);

    boost::shared_ptr<PricingEngine> ccLgmFxOptionEngineUsd =
        boost::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgm, 0);

    boost::shared_ptr<PricingEngine> ccLgmFxOptionEngineGbp =
        boost::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgm, 1);

    boost::shared_ptr<PricingEngine> ccLgmProjectedFxOptionEngineGbp =
        boost::make_shared<AnalyticCcLgmFxOptionEngine>(ccLgmProjected, 0);

    // while the initial fx vol starts at 0.2 for usd and 0.15 for gbp
    // we calibrate to helpers with 0.15 and 0.2 target implied vol
    std::vector<boost::shared_ptr<CalibrationHelper> > helpersUsd, helpersGbp;
    for (Size i = 0; i <= d.volstepdatesFx.size(); ++i) {
        boost::shared_ptr<CalibrationHelper> tmpUsd =
            boost::make_shared<FxOptionHelper>(
                i < d.volstepdatesFx.size() ? d.volstepdatesFx[i]
                                          : d.volstepdatesFx.back() + 365,
                0.90, d.fxEurUsd,
                Handle<Quote>(boost::make_shared<SimpleQuote>(0.15)),
                d.ccLgm->irlgm1f(0)->termStructure(),
                d.ccLgm->irlgm1f(1)->termStructure());
        boost::shared_ptr<CalibrationHelper> tmpGbp =
            boost::make_shared<FxOptionHelper>(
                i < d.volstepdatesFx.size() ? d.volstepdatesFx[i]
                                          : d.volstepdatesFx.back() + 365,
                1.35, d.fxEurGbp,
                Handle<Quote>(boost::make_shared<SimpleQuote>(0.20)),
                d.ccLgm->irlgm1f(0)->termStructure(),
                d.ccLgm->irlgm1f(2)->termStructure());
        tmpUsd->setPricingEngine(ccLgmFxOptionEngineUsd);
        tmpGbp->setPricingEngine(ccLgmFxOptionEngineGbp);
        helpersUsd.push_back(tmpUsd);
        helpersGbp.push_back(tmpGbp);
    }

    LevenbergMarquardt lm(1E-8, 1E-8, 1E-8);
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);

    // calibrate USD-EUR FX volatility
    d.ccLgm->calibrateFxBsVolatilitiesIterative(0, helpersUsd, lm, ec);
    // calibrate GBP-EUR FX volatility
    d.ccLgm->calibrateFxBsVolatilitiesIterative(1, helpersGbp, lm, ec);

    Real tol = 1E-6;
    for (Size i = 0; i < helpersUsd.size(); ++i) {
        Real market = helpersUsd[i]->marketValue();
        Real model = helpersUsd[i]->modelValue();
        Real calibratedVol = d.ccLgm->fxbs(0)->parameterValues(0)[i];
        if (std::fabs(market - model) > tol) {
            BOOST_ERROR("calibration for fx option helper #"
                        << i << " (USD) failed, market premium is " << market
                        << " while model premium is " << model);
        }
        // the stochastic rates produce some noise, but do not have a huge
        // impact on the effective volatility, so we check that they are
        // in line with a cached example (note that the analytic fx option
        // pricing engine was checked against MC in another test case)
        if (std::fabs(calibratedVol - 0.143) > 0.01) {
            BOOST_ERROR(
                "calibrated fx volatility #"
                << i
                << " (USD) seems off, expected to be 0.143 +- 0.01, but is "
                << calibratedVol);
        }
    }
    for (Size i = 0; i < helpersGbp.size(); ++i) {
        Real market = helpersGbp[i]->marketValue();
        Real model = helpersGbp[i]->modelValue();
        Real calibratedVol = d.ccLgm->fxbs(1)->parameterValues(0)[i];
        if (std::fabs(market - model) > tol)
            BOOST_ERROR("calibration for fx option helper #"
                        << i << " (GBP) failed, market premium is " << market
                        << " while model premium is " << model);
        // see above
        if (std::fabs(calibratedVol - 0.193) > 0.01)
            BOOST_ERROR(
                "calibrated fx volatility #"
                << i
                << " (USD) seems off, expected to be 0.193 +- 0.01, but is "
                << calibratedVol);
    }

    // calibrate the projected model

    for (Size i = 0; i < helpersGbp.size(); ++i) {
        helpersGbp[i]->setPricingEngine(ccLgmProjectedFxOptionEngineGbp);
    }

    ccLgmProjected->calibrateFxBsVolatilitiesIterative(0, helpersGbp, lm, ec);

    for (Size i = 0; i < helpersGbp.size(); ++i) {
        Real fullModelVol = d.ccLgm->fxbs(1)->parameterValues(0)[i];
        Real projectedModelVol = ccLgmProjected->fxbs(0)->parameterValues(0)[i];
        if (std::fabs(fullModelVol - projectedModelVol) > tol)
            BOOST_ERROR(
                "calibrated fx volatility of full model @"
                << i << " (" << fullModelVol
                << ") is inconsistent with that of the projected model ("
                << projectedModelVol << ")");
    }

} // testLgm5fFxCalibration

void XAssetModelTest::testLgm5fMoments() {

    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler discretization "
                       "in Ccy LGM 5F model...");

    Lgm5fTestData d;

    boost::shared_ptr<StochasticProcess> p_exact =
        d.ccLgm->stateProcess(XAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> p_euler =
        d.ccLgm->stateProcess(XAssetStateProcess::euler);

    Real T = 10.0;         // horizon at which we compare the moments
    Size steps = T * 10.0; // number of simulation steps
    Size paths = 25000;    // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);

    Size seed = 1847263;
    TimeGrid grid(T, steps);

    LowDiscrepancy::rsg_type sg =
        LowDiscrepancy::make_sequence_generator(steps * 5, seed);
    QuantExt::MultiPathGenerator<LowDiscrepancy::rsg_type> pgen(p_euler, grid,
                                                                sg, true);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > >
        e_eu[5];
    accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > >
        v_eu[5][5];

    for (Size i = 0; i < paths; ++i) {
        Sample<MultiPath> path = pgen.next();
        for (Size ii = 0; ii < 5; ++ii) {
            Real cii = path.value[ii].back();
            e_eu[ii](cii);
            for (Size jj = 0; jj <= ii; ++jj) {
                Real cjj = path.value[jj].back();
                v_eu[ii][jj](cii, covariate1 = cjj);
            }
        }
    }

    Real errTolLd[] = {0.2E-4, 0.2E-4, 0.2E-4, 10.0E-4, 10.0E-4};

    for (Size i = 0; i < 5; ++i) {
        // check expectation against analytical calculation
        if (std::fabs(mean(e_eu[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #"
                        << i << " (" << e_an[i]
                        << ") is inconsistent with numerical value (Euler "
                           "discretization, "
                        << mean(e_eu[i]) << "), error is "
                        << e_an[i] - mean(e_eu[i]) << " tolerance is "
                        << errTolLd[i]);
        }
    }

    // we have to deal with different natures of volatility
    // for ir (normal) and fx (ln) so different error
    // tolerances apply
    Real tollNormal = 0.1E-4; // ir-ir
    Real tolMixed = 0.25E-4;  // ir-fx
    Real tolLn = 8.0E-4;      // fx-fx
    Real tol; // set below

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
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Euler discretization, "
                            << covariance(v_eu[i][j]) << "), error is "
                            << v_an[i][j] - covariance(v_eu[i][j])
                            << " tolerance is " << tol);
            }
        }
    }

} // testLgm5fMoments

void XAssetModelTest::testLgmGsrEquivalence() {

    BOOST_TEST_MESSAGE("Testing equivalence of GSR and LGM models...");

    SavedSettings backup;

    Date evalDate(12, January, 2015);
    Settings::instance().evaluationDate() = evalDate;
    Handle<YieldTermStructure> yts(
        boost::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed()));

    Real T[] = {10.0, 20.0, 50.0, 100.0};
    Real sigma[] = {0.0050, 0.01, 0.02};
    Real kappa[] = {-0.02, -0.01, 0.0, 0.03, 0.07};

    for (Size i = 0; i < LENGTH(T); ++i) {
        for (Size j = 0; j < LENGTH(sigma); ++j) {
            for (Size k = 0; k < LENGTH(kappa); ++k) {

                std::vector<Date> stepDates;
                std::vector<Real> sigmas(1, sigma[j]);

                boost::shared_ptr<Gsr> gsr = boost::make_shared<Gsr>(
                    yts, stepDates, sigmas, kappa[k], T[i]);

                Array stepTimes_a(0);
                Array sigmas_a(1, sigma[j]);
                Array kappas_a(1, kappa[k]);

                // for shift = -H(T) we change the LGM measure to the T forward
                // measure effectively
                Real shift =
                    close_enough(kappa[k], 0.0)
                        ? -T[i]
                        : -(1.0 - std::exp(-kappa[k] * T[i])) / kappa[k];
                boost::shared_ptr<IrLgm1fParametrization> lgm_p =
                    boost::make_shared<
                        IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
                        EURCurrency(), yts, stepTimes_a, sigmas_a, kappas_a);
                lgm_p->shift() = shift;

                boost::shared_ptr<Lgm> lgm = boost::make_shared<Lgm>(lgm_p);

                boost::shared_ptr<StochasticProcess1D> gsr_process =
                    gsr->stateProcess();
                boost::shared_ptr<StochasticProcess1D> lgm_process =
                    lgm->stateProcess();

                Size N = 10000; // number of paths
                Size seed = 123456;
                Size steps = 1;       // one large step
                Real T2 = T[i] - 5.0; // we check a distribution at this time

                TimeGrid grid(T2, steps);

                PseudoRandom::rsg_type sg =
                    PseudoRandom::make_sequence_generator(steps * 1, seed);
                PathGenerator<PseudoRandom::rsg_type> pgen_gsr(gsr_process,
                                                               grid, sg, false);
                PathGenerator<PseudoRandom::rsg_type> pgen_lgm(lgm_process,
                                                               grid, sg, false);

                accumulator_set<double,
                                stats<tag::mean, tag::error_of<tag::mean>,
                                      tag::variance> > stat_lgm,
                    stat_gsr;

                for (Size i = 0; i < N; ++i) {
                    Sample<Path> path_lgm = pgen_lgm.next();
                    Sample<Path> path_gsr = pgen_gsr.next();
                    Real yGsr = (path_gsr.value.back() -
                                 gsr_process->expectation(0.0, 0.0, T2)) /
                                gsr_process->stdDeviation(0.0, 0.0, T2);
                    Real xLgm = path_lgm.value.back();
                    Real gsrRate = -std::log(gsr->zerobond(T2 + 1.0, T2, yGsr));
                    // it's nice to have uniform interfaces in all models ...
                    Real lgmRate =
                        -std::log(lgm->discountBond(T2, T2 + 1.0, xLgm));
                    stat_gsr(gsrRate);
                    stat_lgm(lgmRate);
                }

                // effectively we are checking a pathwise identity
                // here, but the statistics seems to better summarize
                // a possible problem ...
                Real tol = 1.0E-12;
                if (std::fabs(mean(stat_gsr) - mean(stat_lgm)) > tol ||
                    std::fabs(variance(stat_gsr) - variance(stat_lgm)) > tol) {
                    BOOST_ERROR("failed to verify LGM-GSR equivalence, "
                                "(mean,variance) of zero rate is ("
                                << mean(stat_gsr) << "," << variance(stat_gsr)
                                << ") for GSR, (" << mean(stat_lgm) << ","
                                << variance(stat_lgm) << ") for LGM, for T="
                                << T[i] << ", sigma=" << sigma[j] << ", kappa="
                                << kappa[k] << ", shift=" << shift);
                }
            }
        }
    }

} // testLgmGsrEquivalence

void XAssetModelTest::testLgmMcWithShift() {
    BOOST_TEST_MESSAGE(
        "Testing LGM1F Monte Carlo simulation with shifted H...");

    // cashflow time
    Real T = 50.0;

    // shift horizons
    Real T_shift[] = {0.0, 10.0, 20.0, 30.0, 40.0, 50.0};

    // tolerances for error of mean
    Real eom_tol[] = {0.17, 0.05, 0.02, 0.01, 0.005, 1.0E-12};

    Handle<YieldTermStructure> yts(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.02, Actual365Fixed()));

    boost::shared_ptr<IrLgm1fParametrization> lgm =
        boost::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), yts,
                                                           0.01, 0.01);
    boost::shared_ptr<StochasticProcess> p =
        boost::make_shared<IrLgm1fStateProcess>(lgm);

    boost::shared_ptr<Lgm> model = boost::make_shared<Lgm>(lgm);

    Size steps = 1;
    Size paths = 10000;
    Size seed = 42;
    TimeGrid grid(T, steps);

    PseudoRandom::rsg_type sg =
        PseudoRandom::make_sequence_generator(steps * 1, seed);
    QuantExt::MultiPathGenerator<PseudoRandom::rsg_type> pgen(p, grid, sg,
                                                              true);

    for (Size ii = 0; ii < LENGTH(T_shift); ++ii) {

        lgm->shift() = -(1.0 - exp(-0.01 * T_shift[ii])) / 0.01;

        accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > >
            e_eu;

        for (Size i = 0; i < paths; ++i) {
            Sample<MultiPath> path = pgen.next();
            Sample<MultiPath> path_a = pgen.antithetic();
            e_eu(1.0 / model->numeraire(T, path.value[0].back()));
            e_eu(1.0 / model->numeraire(T, path_a.value[0].back()));
        }

        Real discount = yts->discount(T);

        if (error_of<tag::mean>(e_eu) / discount > eom_tol[ii]) {
            BOOST_ERROR(
                "estimated error of mean for shifted mc simulation with shift "
                << T_shift[ii] << " can not be verified ("
                << error_of<tag::mean>(e_eu) / discount
                << "), tolerance is 1E-8");
        }

        if (std::fabs(mean(e_eu) / discount - 1.0) > eom_tol[ii]) {
            BOOST_ERROR("estimated error for shifted mc simulation with shift "
                        << T_shift[ii] << " can not "
                                          "be verified ("
                        << mean(e_eu) / discount - 1.0
                        << "), tolerance is 1E-8");
        }
    }

} // testLgmMcWithShift

void XAssetModelTest::testCorrelationRecovery() {

    BOOST_TEST_MESSAGE(
        "Test if random correlation input is recovered for small dt...");

    class PseudoCurrency : public Currency {
      public:
        PseudoCurrency(const Size id) {
            std::ostringstream ln, sn;
            ln << "Dummy " << id;
            sn << "DUM " << id;
            data_ = boost::make_shared<Data>(ln.str(), sn.str(), id, sn.str(),
                                             "", 100, Rounding(), "%3% %1$.2f");
        }
    };

    Real dt = 1.0E-6;
    Real tol = 1.0E-7;

    // for ir-fx this fully specifies the correlation matrix
    // for new asset classes add other possible combinations as well
    Size currencies[] = {1, 2, 3, 4, 5, 10, 20, 50, 100};

    MersenneTwisterUniformRng mt(42);

    Handle<YieldTermStructure> yts(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.01, Actual365Fixed()));

    Handle<Quote> fxspot(boost::make_shared<SimpleQuote>(1.00));

    Array notimes(0);
    Array fxsigma(1, 0.10);

    for (Size ii = 0; ii < LENGTH(currencies); ++ii) {

        std::vector<Currency> pseudoCcy;
        for (Size i = 0; i < currencies[ii]; ++i) {
            PseudoCurrency tmp(i);
            pseudoCcy.push_back(tmp);
        }

        Size dim = 2 * currencies[ii] - 1;

        // generate random correlation matrix
        Matrix b(dim, dim);
        Size maxTries = 100;
        bool valid = true;
        do {
            Matrix a(dim, dim);
            for (Size i = 0; i < dim; ++i) {
                for (Size j = 0; j <= i; ++j) {
                    a[i][j] = a[j][i] = mt.nextReal() - 0.5;
                }
            }
            b = a * transpose(a);
            for (Size i = 0; i < dim; ++i) {
                if (b[i][i] < 1E-5)
                    valid = false;
            }
        } while (!valid && --maxTries > 0);

        if (maxTries == 0) {
            BOOST_ERROR("could no generate random matrix");
            return;
        }

        Matrix c(dim, dim);
        for (Size i = 0; i < dim; ++i) {
            for (Size j = 0; j <= i; ++j) {
                c[i][j] = c[j][i] = b[i][j] / std::sqrt(b[i][i] * b[j][j]);
            }
        }

        // set up model

        std::vector<boost::shared_ptr<Parametrization> > parametrizations;

        // IR
        for (Size i = 0; i < currencies[ii]; ++i) {
            parametrizations.push_back(
                boost::make_shared<IrLgm1fConstantParametrization>(
                    pseudoCcy[i], yts, 0.01, 0.01));
        }
        // FX
        for (Size i = 0; i < currencies[ii] - 1; ++i) {
            parametrizations.push_back(
                boost::make_shared<FxBsPiecewiseConstantParametrization>(
                    pseudoCcy[i + 1], fxspot, notimes, fxsigma));
        }

        boost::shared_ptr<XAssetModel> model = boost::make_shared<XAssetModel>(
            parametrizations, c, SalvagingAlgorithm::None);

        boost::shared_ptr<StochasticProcess> peuler =
            model->stateProcess(XAssetStateProcess::euler);
        boost::shared_ptr<StochasticProcess> pexact =
            model->stateProcess(XAssetStateProcess::exact);

        Matrix c1 = peuler->covariance(0.0, peuler->initialValues(), dt);
        Matrix c2 = pexact->covariance(0.0, peuler->initialValues(), dt);

        Matrix r1(dim, dim), r2(dim, dim);

        for (Size i = 0; i < dim; ++i) {
            for (Size j = 0; j <= i; ++j) {
                r1[i][j] = r1[j][i] = c1[i][j] / std::sqrt(c1[i][i] * c1[j][j]);
                r2[i][j] = r2[j][i] = c2[i][j] / std::sqrt(c2[i][i] * c2[j][j]);
                if (std::fabs(r1[i][j] - c[i][j]) > tol) {
                    BOOST_ERROR("failed to recover correlation matrix from "
                                "Euler state process (i,j)=("
                                << i << "," << j << "), input correlation is "
                                << c[i][j] << ", output is " << r1[i][j]
                                << ", difference " << (c[i][j] - r1[i][j])
                                << ", tolerance " << tol);
                }
                if (std::fabs(r2[i][j] - c[i][j]) > tol) {
                    BOOST_ERROR("failed to recover correlation matrix from "
                                "exact state process (i,j)=("
                                << i << "," << j << "), input correlation is "
                                << c[i][j] << ", output is " << r2[i][j]
                                << ", difference " << (c[i][j] - r2[i][j])
                                << ", tolerance " << tol);
                }
            }
        }

    } // for currencies

} // test correlation recovery

test_suite *XAssetModelTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("XAsset model tests");
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testBermudanLgm1fGsr));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testBermudanLgmInvariances));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testLgm1fCalibration));
    suite->add(
        QUANTEXT_TEST_CASE(&XAssetModelTest::testCcyLgm3fForeignPayouts));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testLgm5fFxCalibration));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testLgm5fMoments));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testLgmGsrEquivalence));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testLgmMcWithShift));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testCorrelationRecovery));
    return suite;
}
