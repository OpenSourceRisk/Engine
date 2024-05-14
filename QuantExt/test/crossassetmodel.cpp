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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/cdsoptionhelper.hpp>
#include <qle/models/cirppconstantfellerparametrization.hpp>
#include <qle/models/commodityschwartzmodel.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/crlgm1fparametrization.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/models/crossassetanalyticsbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetmodelimpliedeqvoltermstructure.hpp>
#include <qle/models/crossassetmodelimpliedfxvoltermstructure.hpp>
#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>
#include <qle/models/dkimpliedzeroinflationtermstructure.hpp>
#include <qle/models/eqbsconstantparametrization.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/eqbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/lgmimplieddefaulttermstructure.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>
#include <qle/models/pseudoparameter.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/pricingengines/analyticlgmcdsoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>
#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingcommodityforwardengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingequityforwardengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include <qle/processes/commodityschwartzstateprocess.hpp>
#include <qle/processes/crossassetstateprocess.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>
#include <qle/termstructures/pricecurve.hpp>

#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/gbplibor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/inflation/interpolatedzeroinflationcurve.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

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
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;
namespace bdata = boost::unit_test::data;
using std::vector;

namespace {

struct BermudanTestData {
    BermudanTestData()
        : evalDate(12, January, 2015), yts(QuantLib::ext::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed())),
          euribor6m(QuantLib::ext::make_shared<Euribor>(6 * Months, yts)), effectiveDate(TARGET().advance(evalDate, 2 * Days)),
          startDate(TARGET().advance(effectiveDate, 1 * Years)), maturityDate(TARGET().advance(startDate, 9 * Years)),
          fixedSchedule(startDate, maturityDate, 1 * Years, TARGET(), ModifiedFollowing, ModifiedFollowing,
                        DateGeneration::Forward, false),
          floatingSchedule(startDate, maturityDate, 6 * Months, TARGET(), ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false),
          underlying(
              QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                                          floatingSchedule, euribor6m, 0.0, Actual360()))),
          reversion(0.03) {
        Settings::instance().evaluationDate() = evalDate;
        for (Size i = 0; i < 9; ++i) {
            exerciseDates.push_back(TARGET().advance(fixedSchedule[i], -2 * Days));
        }
        exercise = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates, false);

        swaption = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
        stepDates = std::vector<Date>(exerciseDates.begin(), exerciseDates.end() - 1);
        sigmas = std::vector<Real>(stepDates.size() + 1);
        for (Size i = 0; i < sigmas.size(); ++i) {
            sigmas[i] = 0.0050 + (0.0080 - 0.0050) * std::exp(-0.2 * static_cast<double>(i));
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
    QuantLib::ext::shared_ptr<IborIndex> euribor6m;
    Date effectiveDate, startDate, maturityDate;
    Schedule fixedSchedule, floatingSchedule;
    QuantLib::ext::shared_ptr<VanillaSwap> underlying;
    std::vector<Date> exerciseDates, stepDates;
    std::vector<Real> sigmas;
    QuantLib::ext::shared_ptr<Exercise> exercise;
    QuantLib::ext::shared_ptr<Swaption> swaption;
    Array stepTimes_a, sigmas_a, kappas_a;
    Real reversion;
}; // BermudanTestData

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CrossAssetModelTest)

BOOST_AUTO_TEST_CASE(testBermudanLgm1fGsr) {

    BOOST_TEST_MESSAGE("Testing consistency of Bermudan swaption pricing in "
                       "LGM 1F and GSR models...");

    BermudanTestData d;

    // we use the Hull White adaptor for the LGM parametrization
    // which should lead to equal Bermudan swaption prices
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), d.yts, d.stepTimes_a, d.sigmas_a, d.stepTimes_a, d.kappas_a);

    // fix any T forward measure
    QuantLib::ext::shared_ptr<Gsr> gsr = QuantLib::ext::make_shared<Gsr>(d.yts, d.stepDates, d.sigmas, d.reversion, 50.0);

    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm_p);

    QuantLib::ext::shared_ptr<Gaussian1dModel> lgm_g1d = QuantLib::ext::make_shared<Gaussian1dCrossAssetAdaptor>(lgm);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineGsr =
        QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(gsr, 64, 7.0, true, false);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm =
        QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(lgm_g1d, 64, 7.0, true, false);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm2 =
        QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);

    d.swaption->setPricingEngine(swaptionEngineGsr);
    Real npvGsr = d.swaption->NPV();
    d.swaption->setPricingEngine(swaptionEngineLgm);
    Real npvLgm = d.swaption->NPV();
    d.swaption->setPricingEngine(swaptionEngineLgm2);
    Real npvLgm2 = d.swaption->NPV();

    Real tol = 0.2E-4; // basis point tolerance

    if (std::fabs(npvGsr - npvLgm) > tol)
        BOOST_ERROR("Failed to verify consistency of Bermudan swaption price "
                    "in IrLgm1f / Gaussian1d adaptor engine ("
                    << npvLgm << ") and Gsr (" << npvGsr << ") models, tolerance is " << tol);

    if (std::fabs(npvGsr - npvLgm2) > tol)
        BOOST_ERROR("Failed to verify consistency of Bermudan swaption price "
                    "in IrLgm1f / Numeric LGM engine ("
                    << npvLgm2 << ") and Gsr (" << npvGsr << ") models, tolerance is " << tol);
}

BOOST_AUTO_TEST_CASE(testBermudanLgmInvariances) {

    BOOST_TEST_MESSAGE("Testing LGM model invariances for Bermudan "
                       "swaption pricing...");

    BermudanTestData d;

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p2 = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), d.yts, d.stepTimes_a, d.sigmas_a, d.stepTimes_a, d.kappas_a);

    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm2 = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm_p2);

    QuantLib::ext::shared_ptr<Gaussian1dModel> lgm_g1d2 = QuantLib::ext::make_shared<Gaussian1dCrossAssetAdaptor>(lgm2);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm2 =
        QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(lgm_g1d2, 64, 7.0, true, false);

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

BOOST_AUTO_TEST_CASE(testNonstandardBermudanSwaption) {

    BOOST_TEST_MESSAGE("Testing numeric LGM swaption engine for non-standard swaption...");

    BermudanTestData d;

    QuantLib::ext::shared_ptr<NonstandardSwaption> ns_swaption = QuantLib::ext::make_shared<NonstandardSwaption>(*d.swaption);

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), d.yts, d.stepTimes_a, d.sigmas_a, d.stepTimes_a, d.kappas_a);

    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm_p);

    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);
    QuantLib::ext::shared_ptr<PricingEngine> ns_engine =
        QuantLib::ext::make_shared<NumericLgmNonstandardSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);

    d.swaption->setPricingEngine(engine);
    ns_swaption->setPricingEngine(ns_engine);

    Real npv = d.swaption->NPV();
    Real ns_npv = d.swaption->NPV();

    Real tol = 1.0E-12;
    if (std::fabs(npv - ns_npv) >= tol)
        BOOST_ERROR("Failed to verify consistency of Bermudan swaption price ("
                    << npv << ") and Bermudan nonstandard swaption price (" << ns_npv << "), difference is "
                    << (npv - ns_npv) << ", tolerance is " << tol);
} // testNonstandardBermudanSwaption

BOOST_AUTO_TEST_CASE(testLgm1fCalibration) {

    BOOST_TEST_MESSAGE("Testing calibration of LGM 1F model (analytic engine) "
                       "against GSR parameters...");

    // for fixed kappa != 0.0 we calibrate sigma via
    // the Hull White Adaptor

    SavedSettings backup;

    Date evalDate(12, January, 2015);
    Settings::instance().evaluationDate() = evalDate;
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed()));
    QuantLib::ext::shared_ptr<IborIndex> euribor6m = QuantLib::ext::make_shared<Euribor>(6 * Months, yts);

    // coterminal basket 1y-9y, 2y-8y, ... 9y-1y

    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > basket;
    Real impliedVols[] = { 0.4, 0.39, 0.38, 0.35, 0.35, 0.34, 0.33, 0.32, 0.31 };
    std::vector<Date> expiryDates;

    for (Size i = 0; i < 9; ++i) {
        QuantLib::ext::shared_ptr<BlackCalibrationHelper> helper = QuantLib::ext::make_shared<SwaptionHelper>(
            (i + 1) * Years, (9 - i) * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(impliedVols[i])), euribor6m,
            1 * Years, Thirty360(Thirty360::BondBasis), Actual360(), yts);
        basket.push_back(helper);
        expiryDates.push_back(
            QuantLib::ext::static_pointer_cast<SwaptionHelper>(helper)->swaption()->exercise()->dates().back());
    }

    std::vector<Date> stepDates(expiryDates.begin(), expiryDates.end() - 1);

    Array stepTimes_a(stepDates.size());
    for (Size i = 0; i < stepDates.size(); ++i) {
        stepTimes_a[i] = yts->timeFromReference(stepDates[i]);
    }

    Real kappa = 0.05;

    std::vector<Real> gsrInitialSigmas(stepDates.size() + 1, 0.0050);
    std::vector<Real> lgmInitialSigmas2(stepDates.size() + 1, 0.0050);

    Array lgmInitialSigmas2_a(lgmInitialSigmas2.begin(), lgmInitialSigmas2.end());
    Array kappas_a(lgmInitialSigmas2_a.size(), kappa);

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), yts, stepTimes_a, lgmInitialSigmas2_a, stepTimes_a, kappas_a);

    // fix any T forward measure
    QuantLib::ext::shared_ptr<Gsr> gsr = QuantLib::ext::make_shared<Gsr>(yts, stepDates, gsrInitialSigmas, kappa, 50.0);

    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm_p);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineGsr =
        QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(gsr, 64, 7.0, true, false);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(lgm);

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
        if (std::fabs(basket[i]->modelValue() - basket[i]->marketValue()) > tol0)
            BOOST_ERROR("Failed to calibrate to market swaption #"
                        << i << ", market price is " << basket[i]->marketValue() << " while model price is "
                        << basket[i]->modelValue());
        // compare calibrated model parameters
        if (std::fabs(gsrSigmas[i] - lgmSigmas[i]) > tol)
            BOOST_ERROR("Failed to verify LGM's sigma from Hull White adaptor (#"
                        << i << "), which is " << lgmSigmas[i] << " while GSR's sigma is " << gsrSigmas[i] << ")");
    }

    // calibrate LGM as component of CrossAssetModel

    // create a second set of parametrization ...
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p21 = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        USDCurrency(), yts, stepTimes_a, lgmInitialSigmas2_a, stepTimes_a, kappas_a);
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p22 = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), yts, stepTimes_a, lgmInitialSigmas2_a, stepTimes_a, kappas_a);

    // ... and a fx parametrization ...
    Array notimes_a(0);
    Array sigma_a(1, 0.10);
    QuantLib::ext::shared_ptr<FxBsParametrization> fx_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(
        EURCurrency(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.00)), notimes_a, sigma_a);

    // ... and set up an crossasset model with USD as domestic currency ...
    std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;
    parametrizations.push_back(lgm_p21);
    parametrizations.push_back(lgm_p22);
    parametrizations.push_back(fx_p);
    Matrix rho(3, 3, 0.0);
    rho[0][0] = rho[1][1] = rho[2][2] = 1.0;
    QuantLib::ext::shared_ptr<CrossAssetModel> xmodel =
        QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, rho, SalvagingAlgorithm::None);

    // .. whose EUR component we calibrate as before and compare the
    // result against the 1d case and as well check that the USD
    // component was not touched by the calibration.

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm2 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(xmodel, 1);

    for (Size i = 0; i < basket.size(); ++i) {
        basket[i]->setPricingEngine(swaptionEngineLgm2);
    }

    xmodel->calibrateIrLgm1fVolatilitiesIterative(1, basket, lm, ec);

    Array lgmSigmas2eur = xmodel->irlgm1f(1)->parameterValues(0);
    Array lgmSigmas2usd = xmodel->irlgm1f(0)->parameterValues(0);

    for (Size i = 0; i < gsrSigmas.size(); ++i) {
        // compare calibrated model parameters against 1d calibration before
        if (!close_enough(lgmSigmas2eur[i], lgmSigmas[i]))
            BOOST_ERROR("Failed to verify crossasset LGM1F component calibration "
                        "at parameter #"
                        << i << " against 1d calibration, which is " << lgmSigmas2eur[i] << " while 1d calibration was "
                        << lgmSigmas[i] << ")");
        // compare usd component against start values (since it was not
        // calibrated, so should not have changed)
        if (!close_enough(lgmSigmas2usd[i], lgmInitialSigmas2[i]))
            BOOST_ERROR("Non calibrated crossasset LGM1F component was changed by "
                        "other's component calibration at #"
                        << i << ", the new value is " << lgmSigmas2usd[i] << " while the initial value was "
                        << lgmInitialSigmas2[i]);
    }

} // testLgm1fCalibration

BOOST_AUTO_TEST_CASE(testCcyLgm3fForeignPayouts) {

    BOOST_TEST_MESSAGE("Testing pricing of foreign payouts under domestic "
                       "measure in Ccy LGM 3F model...");

    SavedSettings backup;

    Date referenceDate(30, July, 2015);

    Settings::instance().evaluationDate() = referenceDate;

    Handle<YieldTermStructure> eurYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    Handle<YieldTermStructure> usdYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed()));

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
    volstepdatesFx.push_back(Date(15, July, 2018));      //  shared with EUR and USD

    std::vector<Real> eurVols, usdVols, fxVols;

    for (Size i = 0; i < volstepdatesEur.size() + 1; ++i) {
        eurVols.push_back(0.0050 + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));
    }
    for (Size i = 0; i < volstepdatesUsd.size() + 1; ++i) {
        usdVols.push_back(0.0030 + (0.0110 - 0.0030) * std::exp(-0.3 * static_cast<double>(i)));
    }
    for (Size i = 0; i < volstepdatesFx.size() + 1; ++i) {
        fxVols.push_back(0.15 + (0.20 - 0.15) * std::exp(-0.3 * static_cast<double>(i)));
    }

    Array alphaTimesEur(volstepdatesEur.size()), alphaEur(eurVols.begin(), eurVols.end()), kappaTimesEur(0),
        kappaEur(1, 0.02);
    Array alphaTimesUsd(volstepdatesUsd.size()), alphaUsd(usdVols.begin(), usdVols.end()), kappaTimesUsd(0),
        kappaUsd(1, 0.04);
    Array fxTimes(volstepdatesFx.size()), fxSigmas(fxVols.begin(), fxVols.end());

    for (Size i = 0; i < alphaTimesEur.size(); ++i) {
        alphaTimesEur[i] = eurYts->timeFromReference(volstepdatesEur[i]);
    }
    for (Size i = 0; i < alphaTimesUsd.size(); ++i) {
        alphaTimesUsd[i] = eurYts->timeFromReference(volstepdatesUsd[i]);
    }
    for (Size i = 0; i < fxTimes.size(); ++i) {
        fxTimes[i] = eurYts->timeFromReference(volstepdatesFx[i]);
    }

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> eurLgmParam = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(
        EURCurrency(), eurYts, alphaTimesEur, alphaEur, kappaTimesEur, kappaEur);

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> usdLgmParam = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(
        USDCurrency(), usdYts, alphaTimesUsd, alphaUsd, kappaTimesUsd, kappaUsd);

    // USD per EUR (foreign per domestic)
    Handle<Quote> usdEurSpotToday(QuantLib::ext::make_shared<SimpleQuote>(0.90));

    QuantLib::ext::shared_ptr<FxBsParametrization> fxUsdEurBsParam =
        QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), usdEurSpotToday, fxTimes, fxSigmas);

    std::vector<QuantLib::ext::shared_ptr<Parametrization> > singleModels;
    singleModels.push_back(eurLgmParam);
    singleModels.push_back(usdLgmParam);
    singleModels.push_back(fxUsdEurBsParam);

    QuantLib::ext::shared_ptr<CrossAssetModel> ccLgm = QuantLib::ext::make_shared<CrossAssetModel>(singleModels);

    Size eurIdx = ccLgm->ccyIndex(EURCurrency());
    Size usdIdx = ccLgm->ccyIndex(USDCurrency());
    Size eurUsdIdx = usdIdx - 1;

    ccLgm->setCorrelation(CrossAssetModel::AssetType::IR, eurIdx, CrossAssetModel::AssetType::IR, usdIdx, -0.2);
    ccLgm->setCorrelation(CrossAssetModel::AssetType::IR, eurIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, 0.8);
    ccLgm->setCorrelation(CrossAssetModel::AssetType::IR, usdIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, -0.5);

    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> eurLgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(eurLgmParam);
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> usdLgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(usdLgmParam);

    QuantLib::ext::shared_ptr<StochasticProcess> process = ccLgm->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> usdProcess = usdLgm->stateProcess();

    // path generation

    Size n = 500000; // number of paths
    Size seed = 121; // seed
    // maturity of test payoffs
    Time T = 5.0;
    // take large steps, but not only one (since we are testing)
    Size steps = static_cast<Size>(T * 2.0);
    TimeGrid grid(T, steps);
    PseudoRandom::rsg_type sg2 = PseudoRandom::make_sequence_generator(steps, seed);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(grid.size() - 1);
    }
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(usdProcess)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorMersenneTwister pg(process, grid, seed, false);
    PathGenerator<PseudoRandom::rsg_type> pg2(usdProcess, grid, sg2, false);

    // test
    // 1 deterministic USD cashflow under EUR numeraire vs. price on USD curve
    // 2 zero bond option USD under EUR numeraire vs. USD numeraire
    // 3 fx option USD-EUR under EUR numeraire vs. analytical price

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > stat1, stat2a, stat2b, stat3;

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
        Real zbOpt = std::max(usdLgm->discountBond(T, T + 10.0, zusd) - 0.5, 0.0);
        stat2a(zbOpt * fx / eurLgm->numeraire(T, zeur));
        // ... and under USD numeraire ...
        Real zbOpt2 = std::max(usdLgm->discountBond(T, T + 10.0, zusd2) - 0.5, 0.0);
        stat2b(zbOpt2 / usdLgm->numeraire(T, zusd2));

        // 3 USD-EUR fx option @0.9
        stat3(std::max(fx - 0.9, 0.0) / eurLgm->numeraire(T, zeur));
    }

    QuantLib::ext::shared_ptr<VanillaOption> fxOption =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, 0.9),
                                          QuantLib::ext::make_shared<EuropeanExercise>(referenceDate + 5 * 365));

    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> ccLgmFxOptionEngine =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(ccLgm, 0);

    ccLgmFxOptionEngine->cache();

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
                    << error1 << ", expected 4E-4, relative tolerance " << tolError);
    if (std::fabs((error2a - 1E-4) / 1E-4) > tolError)
        BOOST_ERROR("error estimate zero bond "
                    "option pricing (foreign measure) can "
                    "not be reproduced, is "
                    << error2a << ", expected 1E-4, relative tolerance " << tolError);
    if (std::fabs((error2b - 7E-5) / 7E-5) > tolError)
        BOOST_ERROR("error estimate zero bond "
                    "option pricing (domestic measure) can "
                    "not be reproduced, is "
                    << error2b << ", expected 7E-5, relative tolerance " << tolError);
    if (std::fabs((error3 - 2.7E-4) / 2.7E-4) > tolError)
        BOOST_ERROR("error estimate fx option pricing can not be reproduced, is "
                    << error3 << ", expected 2.7E-4, relative tolerance " << tolError);

    if (std::fabs(npv1 - expected1) > tolErrEst * error1)
        BOOST_ERROR("can no reproduce deterministic cashflow pricing, is "
                    << npv1 << ", expected " << expected1 << ", tolerance " << tolErrEst << "*" << error1);

    if (std::fabs(npv2a - npv2b) > tolErrEst * std::sqrt(error2a * error2a + error2b * error2b))
        BOOST_ERROR("can no reproduce zero bond option pricing, domestic "
                    "measure result is "
                    << npv2a << ", foreign measure result is " << npv2b << ", tolerance " << tolErrEst << "*"
                    << std::sqrt(error2a * error2a + error2b * error2b));

    if (std::fabs(npv3 - fxOption->NPV()) > tolErrEst * error3)
        BOOST_ERROR("can no reproduce fx option pricing, monte carlo result is "
                    << npv3 << ", analytical pricing result is " << fxOption->NPV() << ", tolerance is " << tolErrEst
                    << "*" << error3);

} // testCcyLgm3fForeignPayouts

namespace {

struct Lgm5fTestData {
    Lgm5fTestData()
        : referenceDate(30, July, 2015), eurYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed())),
          usdYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed())),
          gbpYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.04, Actual365Fixed())),
          fxEurUsd(QuantLib::ext::make_shared<SimpleQuote>(0.90)), fxEurGbp(QuantLib::ext::make_shared<SimpleQuote>(1.35)), c(5, 5) {

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

        eurLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYts, volsteptimes_a,
                                                                               eurVols_a, notimes_a, eurKappa_a);
        usdLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(USDCurrency(), usdYts, volsteptimes_a,
                                                                               usdVols_a, notimes_a, usdKappa_a);
        gbpLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(GBPCurrency(), gbpYts, volsteptimes_a,
                                                                               gbpVols_a, notimes_a, gbpKappa_a);

        fxUsd_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), fxEurUsd, volsteptimesFx_a,
                                                                           fxSigmasUsd_a);
        fxGbp_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(GBPCurrency(), fxEurGbp, volsteptimesFx_a,
                                                                           fxSigmasGbp_a);

        singleModels.push_back(eurLgm_p);
        singleModels.push_back(usdLgm_p);
        singleModels.push_back(gbpLgm_p);
        singleModels.push_back(fxUsd_p);
        singleModels.push_back(fxGbp_p);

        // clang-format off
        //     EUR           USD           GBP         FX USD-EUR      FX GBP-EUR
        c[0][0] = 1.0; c[0][1] = 0.6;  c[0][2] = 0.3; c[0][3] = 0.2;  c[0][4] = 0.3;  // EUR
        c[1][0] = 0.6; c[1][1] = 1.0;  c[1][2] = 0.1; c[1][3] = -0.2; c[1][4] = -0.1; // USD
        c[2][0] = 0.3; c[2][1] = 0.1;  c[2][2] = 1.0; c[2][3] = 0.0;  c[2][4] = 0.1;  // GBP
        c[3][0] = 0.2; c[3][1] = -0.2; c[3][2] = 0.0; c[3][3] = 1.0;  c[3][4] = 0.3;  // FX USD-EUR
        c[4][0] = 0.3; c[4][1] = -0.1; c[4][2] = 0.1; c[4][3] = 0.3;  c[4][4] = 1.0;  // FX GBP-EUR
        // clang-format on

        ccLgmExact = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);
        ccLgmEuler = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Euler);
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
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> eurLgm_p, usdLgm_p, gbpLgm_p;
    QuantLib::ext::shared_ptr<FxBsParametrization> fxUsd_p, fxGbp_p;
    std::vector<QuantLib::ext::shared_ptr<Parametrization> > singleModels;
    Matrix c;
    QuantLib::ext::shared_ptr<CrossAssetModel> ccLgmExact, ccLgmEuler;
}; // LGM5FTestData

struct IrFxCrModelTestData {
    IrFxCrModelTestData(bool includeCirr = false)
        : referenceDate(30, July, 2015), N(includeCirr ? 11 : 8),
          eurYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed())),
          usdYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed())),
          gbpYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.04, Actual365Fixed())),
          fxEurUsd(QuantLib::ext::make_shared<SimpleQuote>(0.90)), fxEurGbp(QuantLib::ext::make_shared<SimpleQuote>(1.35)),
          n1Ts(QuantLib::ext::make_shared<FlatHazardRate>(referenceDate, 0.01, Actual365Fixed())),
          n2Ts(QuantLib::ext::make_shared<FlatHazardRate>(referenceDate, 0.05, Actual365Fixed())),
          n3Ts(QuantLib::ext::make_shared<FlatHazardRate>(referenceDate, 0.10, Actual365Fixed())), n1Alpha(0.01), n1Kappa(0.01),
          n2Alpha(0.015), n2Kappa(0.015), n3Alpha(0.0050), n3Kappa(0.0050),
          cirppKappa(0.206), cirppTheta(0.04), cirppSigma(sqrt(2 * cirppKappa * cirppTheta) - 1E-10),
          cirppY0(cirppTheta), c(N, N, 0.0) {

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

        eurLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYts, volsteptimes_a,
                                                                               eurVols_a, notimes_a, eurKappa_a);
        usdLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(USDCurrency(), usdYts, volsteptimes_a,
                                                                               usdVols_a, notimes_a, usdKappa_a);
        gbpLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(GBPCurrency(), gbpYts, volsteptimes_a,
                                                                               gbpVols_a, notimes_a, gbpKappa_a);

        fxUsd_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), fxEurUsd, volsteptimesFx_a,
                                                                           fxSigmasUsd_a);
        fxGbp_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(GBPCurrency(), fxEurGbp, volsteptimesFx_a,
                                                                           fxSigmasGbp_a);

        // credit LGM
        n1_p = QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(EURCurrency(), n1Ts, n1Alpha, n1Kappa);
        n2_p = QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(EURCurrency(), n2Ts, n2Alpha, n2Kappa);
        n3_p = QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(EURCurrency(), n3Ts, n3Alpha, n3Kappa);

        // credit cir++ (shifted = true), parameters are taken from CrCirppModelTest
        n1_cirpp = QuantLib::ext::make_shared<CrCirppConstantWithFellerParametrization>(
            EURCurrency(), n1Ts, cirppKappa, cirppTheta, cirppSigma, cirppY0, true);
        n2_cirpp = QuantLib::ext::make_shared<CrCirppConstantWithFellerParametrization>(
            USDCurrency(), n2Ts, cirppKappa, cirppTheta, cirppSigma, cirppY0, true);
        n3_cirpp = QuantLib::ext::make_shared<CrCirppConstantWithFellerParametrization>(
            GBPCurrency(), n3Ts, cirppKappa, cirppTheta, cirppSigma, cirppY0, true);

        singleModels.push_back(eurLgm_p);
        singleModels.push_back(usdLgm_p);
        singleModels.push_back(gbpLgm_p);
        singleModels.push_back(fxUsd_p);
        singleModels.push_back(fxGbp_p);
        singleModels.push_back(n1_p);
        singleModels.push_back(n2_p);
        singleModels.push_back(n3_p);
        if (includeCirr) {
            singleModels.push_back(n1_cirpp);
            singleModels.push_back(n2_cirpp);
            singleModels.push_back(n3_cirpp);
        }

        std::vector<std::vector<Real>> tmp;
        if (includeCirr) {
            tmp = {
                // EUR USD  GBP  FX1  FX2   N1   N2   N3  NC1  NC2  NC3
                { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // EUR
                { 0.6, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // USD
                { 0.3, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // GBP
                { 0.2, 0.2, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // FX1
                { 0.3, 0.1, 0.1, 0.3, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // FX2
                { 0.8, 0.2, 0.1, 0.4, 0.2, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // N1
                { 0.6, 0.1, 0.2, 0.2, 0.5, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0 }, // N2
                { 0.3, 0.2, 0.1, 0.1, 0.3, 0.4, 0.2, 1.0, 0.0, 0.0, 0.0 }, // N3
                { 0.0, 0.2, 0.1, 0.4, 0.2, 0.5, 0.3, 0.2, 1.0, 0.0, 0.0 }, // N1 (CIR++)
                { 0.0, 0.1, 0.2, 0.0, 0.5, 0.4, 0.2, 0.1, 0.4, 1.0, 0.0 }, // N2 (CIR++)
                { 0.0, 0.2, 0.1, 0.1, 0.0, 0.3, 0.2, 0.2, 0.3, 0.5, 1.0 } // N3 (CIR++)
            };
        } else {
            tmp = {
                // EUR USD  GBP  FX1  FX2   N1   N2   N3
                { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // EUR
                { 0.6, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // USD
                { 0.3, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // GBP
                { 0.2, 0.2, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0 }, // FX1
                { 0.3, 0.1, 0.1, 0.3, 1.0, 0.0, 0.0, 0.0 }, // FX2
                { 0.8, 0.2, 0.1, 0.4, 0.2, 1.0, 0.0, 0.0 }, // N1
                { 0.6, 0.1, 0.2, 0.2, 0.5, 0.5, 1.0, 0.0 }, // N2
                { 0.3, 0.2, 0.1, 0.1, 0.3, 0.4, 0.2, 1.0 } // N3
            };
        };

        for (Size i = 0; i < N; ++i) {
            for (Size j = 0; j <= i; ++j) {
                c[i][j] = c[j][i] = tmp[i][j];
            }
        }

        BOOST_TEST_MESSAGE("input correlation matrix is\n" << c);
        Matrix ctmp = pseudoSqrt(c, SalvagingAlgorithm::Spectral);
        Matrix cs = ctmp * transpose(ctmp);
        // this leads technically to non-zero correlations EUR/CIR++ and FX/CIR++, but
        // these are small, so that the missing drift adjustment in the state process
        // evolve method can be ignored
        BOOST_TEST_MESSAGE("salvaged correlation matrix is\n" << cs);
        if (includeCirr)
            modelExact = modelEuler =
                QuantLib::ext::make_shared<CrossAssetModel>(singleModels, cs, SalvagingAlgorithm::None, IrModel::Measure::LGM,
                                                    CrossAssetModel::Discretization::Euler);
        else {
            modelExact =
                QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None, IrModel::Measure::LGM,
                                                    CrossAssetModel::Discretization::Exact);
            modelEuler =
                QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None, IrModel::Measure::LGM,
                                                    CrossAssetModel::Discretization::Euler);
        }
        BOOST_TEST_MESSAGE("cam+ model built.");
    }

    SavedSettings backup;
    Date referenceDate;
    Size N;
    // ir-fx
    Handle<YieldTermStructure> eurYts, usdYts, gbpYts;
    std::vector<Date> volstepdates, volstepdatesFx;
    Array volsteptimes_a, volsteptimesFx_a;
    std::vector<Real> eurVols, usdVols, gbpVols, fxSigmasUsd, fxSigmasGbp;
    Handle<Quote> fxEurUsd, fxEurGbp;
    Array eurVols_a, usdVols_a, gbpVols_a, fxSigmasUsd_a, fxSigmasGbp_a;
    Array notimes_a, eurKappa_a, usdKappa_a, gbpKappa_a;
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> eurLgm_p, usdLgm_p, gbpLgm_p;
    QuantLib::ext::shared_ptr<FxBsParametrization> fxUsd_p, fxGbp_p;
    // cr
    Handle<DefaultProbabilityTermStructure> n1Ts, n2Ts, n3Ts;
    // LGM
    QuantLib::ext::shared_ptr<CrLgm1fParametrization> n1_p, n2_p, n3_p;
    Real n1Alpha, n1Kappa, n2Alpha, n2Kappa, n3Alpha, n3Kappa;
    // CIR++
    QuantLib::ext::shared_ptr<CrCirppParametrization> n1_cirpp, n2_cirpp, n3_cirpp;
    Real cirppKappa, cirppTheta, cirppSigma, cirppY0;
    // model
    std::vector<QuantLib::ext::shared_ptr<Parametrization> > singleModels;
    Matrix c;
    QuantLib::ext::shared_ptr<CrossAssetModel> modelExact, modelEuler;
}; // IrFxCrModelTestData

} // anonymous namespace

BOOST_AUTO_TEST_CASE(testLgm5fFxCalibration) {

    BOOST_TEST_MESSAGE("Testing fx calibration in Ccy LGM 5F model...");

    Lgm5fTestData d;

    // we test the 5f model against the 3f model eur-gbp
    std::vector<QuantLib::ext::shared_ptr<Parametrization> > singleModelsProjected;
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

    QuantLib::ext::shared_ptr<CrossAssetModel> ccLgmProjected =
        QuantLib::ext::make_shared<CrossAssetModel>(singleModelsProjected, cProjected, SalvagingAlgorithm::None);

    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> ccLgmFxOptionEngineUsd =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgmExact, 0);

    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> ccLgmFxOptionEngineGbp =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgmExact, 1);

    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> ccLgmProjectedFxOptionEngineGbp =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(ccLgmProjected, 0);

    ccLgmFxOptionEngineUsd->cache();
    ccLgmFxOptionEngineGbp->cache();
    ccLgmProjectedFxOptionEngineGbp->cache();

    // while the initial fx vol starts at 0.2 for usd and 0.15 for gbp
    // we calibrate to helpers with 0.15 and 0.2 target implied vol
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > helpersUsd, helpersGbp;
    for (Size i = 0; i <= d.volstepdatesFx.size(); ++i) {
        QuantLib::ext::shared_ptr<BlackCalibrationHelper> tmpUsd = QuantLib::ext::make_shared<FxEqOptionHelper>(
            i < d.volstepdatesFx.size() ? d.volstepdatesFx[i] : d.volstepdatesFx.back() + 365, 0.90, d.fxEurUsd,
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.15)), d.ccLgmExact->irlgm1f(0)->termStructure(),
            d.ccLgmExact->irlgm1f(1)->termStructure());
        QuantLib::ext::shared_ptr<BlackCalibrationHelper> tmpGbp = QuantLib::ext::make_shared<FxEqOptionHelper>(
            i < d.volstepdatesFx.size() ? d.volstepdatesFx[i] : d.volstepdatesFx.back() + 365, 1.35, d.fxEurGbp,
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.20)), d.ccLgmExact->irlgm1f(0)->termStructure(),
            d.ccLgmExact->irlgm1f(2)->termStructure());
        tmpUsd->setPricingEngine(ccLgmFxOptionEngineUsd);
        tmpGbp->setPricingEngine(ccLgmFxOptionEngineGbp);
        helpersUsd.push_back(tmpUsd);
        helpersGbp.push_back(tmpGbp);
    }

    LevenbergMarquardt lm(1E-8, 1E-8, 1E-8);
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);

    // calibrate USD-EUR FX volatility
    d.ccLgmExact->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::FX, 0, helpersUsd, lm, ec);
    // calibrate GBP-EUR FX volatility
    d.ccLgmExact->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::FX, 1, helpersGbp, lm, ec);

    Real tol = 1E-6;
    for (Size i = 0; i < helpersUsd.size(); ++i) {
        Real market = helpersUsd[i]->marketValue();
        Real model = helpersUsd[i]->modelValue();
        Real calibratedVol = d.ccLgmExact->fxbs(0)->parameterValues(0)[i];
        if (std::fabs(market - model) > tol) {
            BOOST_ERROR("calibration for fx option helper #" << i << " (USD) failed, market premium is " << market
                                                             << " while model premium is " << model);
        }
        // the stochastic rates produce some noise, but do not have a huge
        // impact on the effective volatility, so we check that they are
        // in line with a cached example (note that the analytic fx option
        // pricing engine was checked against MC in another test case)
        if (std::fabs(calibratedVol - 0.143) > 0.01) {
            BOOST_ERROR("calibrated fx volatility #" << i << " (USD) seems off, expected to be 0.143 +- 0.01, but is "
                                                     << calibratedVol);
        }
    }
    for (Size i = 0; i < helpersGbp.size(); ++i) {
        Real market = helpersGbp[i]->marketValue();
        Real model = helpersGbp[i]->modelValue();
        Real calibratedVol = d.ccLgmExact->fxbs(1)->parameterValues(0)[i];
        if (std::fabs(market - model) > tol)
            BOOST_ERROR("calibration for fx option helper #" << i << " (GBP) failed, market premium is " << market
                                                             << " while model premium is " << model);
        // see above
        if (std::fabs(calibratedVol - 0.193) > 0.01)
            BOOST_ERROR("calibrated fx volatility #" << i << " (USD) seems off, expected to be 0.193 +- 0.01, but is "
                                                     << calibratedVol);
    }

    // calibrate the projected model

    for (Size i = 0; i < helpersGbp.size(); ++i) {
        helpersGbp[i]->setPricingEngine(ccLgmProjectedFxOptionEngineGbp);
    }

    ccLgmProjected->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::FX, 0, helpersGbp, lm, ec);

    for (Size i = 0; i < helpersGbp.size(); ++i) {
        Real fullModelVol = d.ccLgmExact->fxbs(1)->parameterValues(0)[i];
        Real projectedModelVol = ccLgmProjected->fxbs(0)->parameterValues(0)[i];
        if (std::fabs(fullModelVol - projectedModelVol) > tol)
            BOOST_ERROR("calibrated fx volatility of full model @"
                        << i << " (" << fullModelVol << ") is inconsistent with that of the projected model ("
                        << projectedModelVol << ")");
    }

} // testLgm5fFxCalibration

BOOST_AUTO_TEST_CASE(testLgm5fFullCalibration) {

    BOOST_TEST_MESSAGE("Testing full calibration of Ccy LGM 5F model...");

    Lgm5fTestData d;

    // calibration baskets

    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > basketEur, basketUsd, basketGbp, basketEurUsd, basketEurGbp;

    QuantLib::ext::shared_ptr<IborIndex> euribor6m = QuantLib::ext::make_shared<Euribor>(6 * Months, d.eurYts);
    QuantLib::ext::shared_ptr<IborIndex> usdLibor3m = QuantLib::ext::make_shared<USDLibor>(3 * Months, d.usdYts);
    QuantLib::ext::shared_ptr<IborIndex> gbpLibor3m = QuantLib::ext::make_shared<GBPLibor>(3 * Months, d.gbpYts);

    for (Size i = 0; i <= d.volstepdates.size(); ++i) {
        Date tmp = i < d.volstepdates.size() ? d.volstepdates[i] : d.volstepdates.back() + 365;
        // EUR: atm+200bp, 150bp normal vol
        basketEur.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.015)), euribor6m, 1 * Years, Thirty360(Thirty360::BondBasis),
            Actual360(), d.eurYts, BlackCalibrationHelper::RelativePriceError, 0.04, 1.0, Normal)));
        // USD: atm, 20%, lognormal vol
        basketUsd.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(
            new SwaptionHelper(tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.30)), usdLibor3m,
                               1 * Years, Thirty360(Thirty360::BondBasis), Actual360(), d.usdYts,
                               BlackCalibrationHelper::RelativePriceError, Null<Real>(), 1.0, ShiftedLognormal, 0.0)));
        // GBP: atm-200bp, 10%, shifted lognormal vol with shift = 2%
        basketGbp.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.30)), gbpLibor3m, 1 * Years, Thirty360(Thirty360::BondBasis),
            Actual360(), d.usdYts, BlackCalibrationHelper::RelativePriceError, 0.02, 1.0, ShiftedLognormal, 0.02)));
    }

    for (Size i = 0; i < d.volstepdatesFx.size(); ++i) {
        Date tmp = i < d.volstepdatesFx.size() ? d.volstepdatesFx[i] : d.volstepdatesFx.back() + 365;
        // EUR-USD: atm, 30% (lognormal) vol
        basketEurUsd.push_back(QuantLib::ext::make_shared<FxEqOptionHelper>(
            tmp, Null<Real>(), d.fxEurUsd, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.20)), d.eurYts, d.usdYts,
            BlackCalibrationHelper::RelativePriceError));
        // EUR-GBP: atm, 10% (lognormal) vol
        basketEurGbp.push_back(QuantLib::ext::make_shared<FxEqOptionHelper>(
            tmp, Null<Real>(), d.fxEurGbp, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.20)), d.eurYts, d.gbpYts,
            BlackCalibrationHelper::RelativePriceError));
    }

    // pricing engines

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(d.ccLgmExact, 0);
    QuantLib::ext::shared_ptr<PricingEngine> usdSwEng = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(d.ccLgmExact, 1);
    QuantLib::ext::shared_ptr<PricingEngine> gbpSwEng = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(d.ccLgmExact, 2);

    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> eurUsdFxoEng =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgmExact, 0);
    QuantLib::ext::shared_ptr<AnalyticCcLgmFxOptionEngine> eurGbpFxoEng =
        QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(d.ccLgmExact, 1);

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

    d.ccLgmExact->calibrateIrLgm1fVolatilitiesIterative(0, basketEur, lm, ec);
    d.ccLgmExact->calibrateIrLgm1fVolatilitiesIterative(1, basketUsd, lm, ec);
    d.ccLgmExact->calibrateIrLgm1fVolatilitiesIterative(2, basketGbp, lm, ec);

    d.ccLgmExact->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::FX, 0, basketEurUsd, lm, ec);
    d.ccLgmExact->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::FX, 1, basketEurGbp, lm, ec);

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

BOOST_AUTO_TEST_CASE(testLgm5fMoments) {

    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization "
                       "in Ccy LGM 5F model...");

    Lgm5fTestData d;

    QuantLib::ext::shared_ptr<StochasticProcess> p_exact = d.ccLgmExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> p_euler = d.ccLgmEuler->stateProcess();

    Real T = 10.0;                            // horizon at which we compare the moments
    Size steps = static_cast<Size>(T * 10.0); // number of simulation steps
    Size paths = 25000;                       // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);

    TimeGrid grid(T, steps);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_euler)) {
        tmp->resetCache(grid.size() - 1);
    }
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_exact)) {
        tmp->resetCache(grid.size() - 1);
    }
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
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (Euler "
                                                                    "discretization, "
                                                                 << mean(e_eu[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu[i]) << " tolerance is "
                                                                 << errTolLd[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (Exact "
                                                                    "discretization, "
                                                                 << mean(e_eu2[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu2[i]) << " tolerance is "
                                                                 << errTolLd[i]);
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
            if (std::fabs(boost::accumulators::covariance(v_eu[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Euler discretization, "
                            << boost::accumulators::covariance(v_eu[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu[i][j]) << " tolerance is " << tol);
            }
            if (std::fabs(boost::accumulators::covariance(v_eu2[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Exact discretization, "
                            << boost::accumulators::covariance(v_eu2[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu2[i][j]) << " tolerance is " << tol);
            }
        }
    }

} // testLgm5fMoments

BOOST_AUTO_TEST_CASE(testLgmGsrEquivalence) {

    BOOST_TEST_MESSAGE("Testing equivalence of GSR and LGM models...");

    SavedSettings backup;

    Date evalDate(12, January, 2015);
    Settings::instance().evaluationDate() = evalDate;
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed()));

    Real T[] = { 10.0, 20.0, 50.0, 100.0 };
    Real sigma[] = { 0.0050, 0.01, 0.02 };
    Real kappa[] = { -0.02, -0.01, 0.0, 0.03, 0.07 };

    for (Size i = 0; i < LENGTH(T); ++i) {
        for (Size j = 0; j < LENGTH(sigma); ++j) {
            for (Size k = 0; k < LENGTH(kappa); ++k) {

                std::vector<Date> stepDates;
                std::vector<Real> sigmas(1, sigma[j]);

                QuantLib::ext::shared_ptr<Gsr> gsr = QuantLib::ext::make_shared<Gsr>(yts, stepDates, sigmas, kappa[k], T[i]);

                Array stepTimes_a(0);
                Array sigmas_a(1, sigma[j]);
                Array kappas_a(1, kappa[k]);

                // for shift = -H(T) we change the LGM measure to the T forward
                // measure effectively
                Real shift = close_enough(kappa[k], 0.0) ? -T[i] : -(1.0 - std::exp(-kappa[k] * T[i])) / kappa[k];
                QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm_p =
                    QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), yts, stepTimes_a,
                                                                                 sigmas_a, stepTimes_a, kappas_a);
                lgm_p->shift() = shift;

                QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm_p);

                QuantLib::ext::shared_ptr<StochasticProcess1D> gsr_process = gsr->stateProcess();
                QuantLib::ext::shared_ptr<StochasticProcess1D> lgm_process =
                    QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(lgm->stateProcess());

                Size N = 10000; // number of paths
                Size seed = 123456;
                Size steps = 1;       // one large step
                Real T2 = T[i] - 5.0; // we check a distribution at this time

                TimeGrid grid(T2, steps);

                PseudoRandom::rsg_type sg = PseudoRandom::make_sequence_generator(steps * 1, seed);
                PathGenerator<PseudoRandom::rsg_type> pgen_gsr(gsr_process, grid, sg, false);
                PathGenerator<PseudoRandom::rsg_type> pgen_lgm(lgm_process, grid, sg, false);

                accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean>, tag::variance> > stat_lgm, stat_gsr;

                Real tol = 1.0E-12;
                for (Size ii = 0; ii < N; ++ii) {
                    Sample<Path> path_lgm = pgen_lgm.next();
                    Sample<Path> path_gsr = pgen_gsr.next();
                    Real yGsr = (path_gsr.value.back() - gsr_process->expectation(0.0, 0.0, T2)) /
                                gsr_process->stdDeviation(0.0, 0.0, T2);
                    Real xLgm = path_lgm.value.back();
                    Real gsrRate = -std::log(gsr->zerobond(T2 + 1.0, T2, yGsr));
                    // it's nice to have uniform interfaces in all models ...
                    Real lgmRate = -std::log(lgm->discountBond(T2, T2 + 1.0, xLgm));
                    stat_gsr(gsrRate);
                    stat_lgm(lgmRate);
                    // check pathwise identity
                    if (std::fabs(gsrRate - lgmRate) >= tol) {
                        BOOST_ERROR("lgm rate (" << lgmRate << ") deviates from gsr rate (" << gsrRate << ") on path #"
                                                 << i);
                    }
                }

                // effectively we are checking a pathwise identity
                // here as well, but the statistics seems to better
                // summarize a possible problem, so we output differences
                // in the mean as well
                if (std::fabs(mean(stat_gsr) - mean(stat_lgm)) > tol ||
                    std::fabs(boost::accumulators::variance(stat_gsr) - boost::accumulators::variance(stat_lgm)) >
                        tol) {
                    BOOST_ERROR("failed to verify LGM-GSR equivalence, "
                                "(mean,variance) of zero rate is ("
                                << mean(stat_gsr) << "," << boost::accumulators::variance(stat_gsr) << ") for GSR, ("
                                << mean(stat_lgm) << "," << boost::accumulators::variance(stat_lgm)
                                << ") for LGM, for T=" << T[i] << ", sigma=" << sigma[j] << ", kappa=" << kappa[k]
                                << ", shift=" << shift);
                }
            }
        }
    }

} // testLgmGsrEquivalence

BOOST_AUTO_TEST_CASE(testLgmMcWithShift) {
    BOOST_TEST_MESSAGE("Testing LGM1F Monte Carlo simulation with shifted H...");

    // cashflow time
    Real T = 50.0;

    // shift horizons
    Real T_shift[] = { 0.0, 10.0, 20.0, 30.0, 40.0, 50.0 };

    // tolerances for error of mean
    Real eom_tol[] = { 0.17, 0.05, 0.02, 0.01, 0.005, 1.0E-12 };

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgm =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), yts, 0.01, 0.01);
    QuantLib::ext::shared_ptr<StochasticProcess> p = QuantLib::ext::make_shared<IrLgm1fStateProcess>(lgm);

    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> model = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm);

    Size steps = 1;
    Size paths = 10000;
    Size seed = 42;
    TimeGrid grid(T, steps);

    MultiPathGeneratorMersenneTwister pgen(p, grid, seed, true);

    for (Size ii = 0; ii < LENGTH(T_shift); ++ii) {

        lgm->shift() = -(1.0 - exp(-0.01 * T_shift[ii])) / 0.01;

        accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > e_eu;

        for (Size i = 0; i < paths; ++i) {
            Sample<MultiPath> path = pgen.next();
            Sample<MultiPath> path_a = pgen.next();
            e_eu(1.0 / model->numeraire(T, path.value[0].back()));
            e_eu(1.0 / model->numeraire(T, path_a.value[0].back()));
        }

        Real discount = yts->discount(T);

        if (error_of<tag::mean>(e_eu) / discount > eom_tol[ii]) {
            BOOST_ERROR("estimated error of mean for shifted mc simulation with shift "
                        << T_shift[ii] << " can not be verified (" << error_of<tag::mean>(e_eu) / discount
                        << "), tolerance is 1E-8");
        }

        if (std::fabs(mean(e_eu) / discount - 1.0) > eom_tol[ii]) {
            BOOST_ERROR("estimated error for shifted mc simulation with shift " << T_shift[ii]
                                                                                << " can not "
                                                                                   "be verified ("
                                                                                << mean(e_eu) / discount - 1.0
                                                                                << "), tolerance is 1E-8");
        }
    }

} // testLgmMcWithShift

BOOST_AUTO_TEST_CASE(testIrFxCrCirppMartingaleProperty) {

    BOOST_TEST_MESSAGE("Testing martingale property in ir-fx-cr(lgm)-cf(cir++) model for "
                       "Euler and exact discretizations...");

    IrFxCrModelTestData d(false);
    IrFxCrModelTestData d_cirpp(true);

    QuantLib::ext::shared_ptr<StochasticProcess> process1 = d.modelExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> process2 = d_cirpp.modelEuler->stateProcess();

    Size n = 10000;                         // number of paths
    Size seed = 18;                         // rng seed
    Time T = 2.0;                          // maturity of payoff
    Time T2 = 20.0;                         // zerobond maturity
    Size steps = static_cast<Size>(T * 24); // number of steps taken (euler / Brigo-Alfonsi)

    LowDiscrepancy::rsg_type sg1 = LowDiscrepancy::make_sequence_generator(process1->factors() * 1, seed);
    LowDiscrepancy::rsg_type sg2 = LowDiscrepancy::make_sequence_generator(process2->factors() * steps, seed);

    TimeGrid grid1(T, 1);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process1)) {
        tmp->resetCache(grid1.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg1(process1, grid1, sg1, false);
    TimeGrid grid2(T, steps);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process2)) {
        tmp->resetCache(grid2.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg2(process2, grid2, sg2, false);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > eurzb1, usdzb1, gbpzb1, n1eur1, n2usd1,
        n3gbp1;
    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > eurzb2, usdzb2, gbpzb2, n1eur2, n2usd2,
        n3gbp2, n1cir2, n2cir2, n3cir2;

    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path1 = pg1.next();
        Sample<MultiPath> path2 = pg2.next();
        Size l1 = path1.value[0].length() - 1;
        Size l2 = path2.value[0].length() - 1;
        Real zeur1 = path1.value[0][l1];
        Real zusd1 = path1.value[1][l1];
        Real zgbp1 = path1.value[2][l1];
        Real fxusd1 = std::exp(path1.value[3][l1]);
        Real fxgbp1 = std::exp(path1.value[4][l1]);
        Real crzn11 = path1.value[5][l1];
        Real cryn11 = path1.value[6][l1];
        Real crzn21 = path1.value[7][l1];
        Real cryn21 = path1.value[8][l1];
        Real crzn31 = path1.value[9][l1];
        Real cryn31 = path1.value[10][l1];
        Real zeur2 = path2.value[0][l2];
        Real zusd2 = path2.value[1][l2];
        Real zgbp2 = path2.value[2][l2];
        Real fxusd2 = std::exp(path2.value[3][l2]);
        Real fxgbp2 = std::exp(path2.value[4][l2]);
        Real crzn12 = path2.value[5][l2]; // CR LGM
        Real cryn12 = path2.value[6][l2];
        Real crzn22 = path2.value[7][l2];
        Real cryn22 = path2.value[8][l2];
        Real crzn32 = path2.value[9][l2];
        Real cryn32 = path2.value[10][l2];
        Real ciry12 = path2.value[11][l2]; // CR CIR++
        Real cirn12 = path2.value[12][l2];
        Real ciry22 = path2.value[13][l2];
        Real cirn22 = path2.value[14][l2];
        Real ciry32 = path2.value[15][l2];
        Real cirn32 = path2.value[16][l2];

        // EUR zerobond
        eurzb1(d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));
        // USD zerobond
        usdzb1(d.modelExact->discountBond(1, T, T2, zusd1) * fxusd1 / d.modelExact->numeraire(0, T, zeur1));
        // GBP zerobond
        gbpzb1(d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 / d.modelExact->numeraire(0, T, zeur1));
        // EUR defaultable zerobond for name 1
        std::pair<Real, Real> sn11 = d.modelExact->crlgm1fS(0, 0, T, T2, crzn11, cryn11);
        n1eur1(sn11.first * sn11.second * d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));
        // USD defaultable zerobond for name 2
        std::pair<Real, Real> sn21 = d.modelExact->crlgm1fS(1, 1, T, T2, crzn21, cryn21);
        n2usd1(sn21.first * sn21.second * d.modelExact->discountBond(1, T, T2, zusd1) * fxusd1 /
               d.modelExact->numeraire(0, T, zeur1));
        // GBP defaultable zerobond for name 3
        std::pair<Real, Real> sn31 = d.modelExact->crlgm1fS(2, 2, T, T2, crzn31, cryn31);
        n3gbp1(sn31.first * sn31.second * d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 /
               d.modelExact->numeraire(0, T, zeur1));

        // EUR zerobond
        eurzb2(d_cirpp.modelEuler->discountBond(0, T, T2, zeur2) / d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // USD zerobond
        usdzb2(d_cirpp.modelEuler->discountBond(1, T, T2, zusd2) * fxusd2 / d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // GBP zerobond
        gbpzb2(d_cirpp.modelEuler->discountBond(2, T, T2, zgbp2) * fxgbp2 / d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // EUR defaultable zerobond for name 1
        std::pair<Real, Real> sn12 = d_cirpp.modelEuler->crlgm1fS(0, 0, T, T2, crzn12, cryn12);
        n1eur2(sn12.first * sn12.second * d_cirpp.modelEuler->discountBond(0, T, T2, zeur2) / d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // USD defaultable zerobond for name 2
        std::pair<Real, Real> sn22 = d_cirpp.modelEuler->crlgm1fS(1, 1, T, T2, crzn22, cryn22);
        n2usd2(sn22.first * sn22.second * d_cirpp.modelEuler->discountBond(1, T, T2, zusd2) * fxusd2 /
               d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // GBP defaultable zerobond for name 3
        std::pair<Real, Real> sn32 = d_cirpp.modelEuler->crlgm1fS(2, 2, T, T2, crzn32, cryn32);
        n3gbp2(sn32.first * sn32.second * d_cirpp.modelEuler->discountBond(2, T, T2, zgbp2) * fxgbp2 /
               d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // EUR defaultable zerobond for name 1 (CIR++)
        std::pair<Real, Real> sc12 = d_cirpp.modelEuler->crcirppS(3, T, T2, ciry12, cirn12);
        n1cir2(sc12.first * sc12.second * d_cirpp.modelEuler->discountBond(0, T, T2, zeur2) / d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // USD defaultable zerobond for name 2 (CIR++)
        std::pair<Real, Real> sc22 = d_cirpp.modelEuler->crcirppS(4, T, T2, ciry22, cirn22);
        n2cir2(sc22.first * sc22.second * d_cirpp.modelEuler->discountBond(1, T, T2, zusd2) * fxusd2 /
               d_cirpp.modelEuler->numeraire(0, T, zeur2));
        // GBP defaultable zerobond for name 3 (CIR++)
        std::pair<Real, Real> sc32 = d_cirpp.modelEuler->crcirppS(5, T, T2, ciry32, cirn32);
        n3cir2(sc32.first * sc32.second * d_cirpp.modelEuler->discountBond(2, T, T2, zgbp2) * fxgbp2 /
               d_cirpp.modelEuler->numeraire(0, T, zeur2));
    }

    BOOST_TEST_MESSAGE("EXACT:");
    BOOST_TEST_MESSAGE("EUR zb = " << mean(eurzb1) << " +- " << error_of<tag::mean>(eurzb1) << " vs analytical "
                                   << d.eurYts->discount(T2));
    BOOST_TEST_MESSAGE("USD zb = " << mean(usdzb1) << " +- " << error_of<tag::mean>(usdzb1) << " vs analytical "
                                   << d.usdYts->discount(T2) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("GBP zb = " << mean(gbpzb1) << " +- " << error_of<tag::mean>(gbpzb1) << " vs analytical "
                                   << d.gbpYts->discount(T2) * d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1eur1) << " +- " << error_of<tag::mean>(n1eur1) << " vs analytical "
                                      << d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N2 zb USD = " << mean(n2usd1) << " +- " << error_of<tag::mean>(n2usd1) << " vs analytical "
                                      << d.fxEurUsd->value() * d.usdYts->discount(T2) *
                                             d.n2Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N3 zb GBP = " << mean(n3gbp1) << " +- " << error_of<tag::mean>(n3gbp1) << " vs analytical "
                                      << d.fxEurGbp->value() * d.gbpYts->discount(T2) *
                                             d.n3Ts->survivalProbability(T2));

    BOOST_TEST_MESSAGE("\nEULER:");
    BOOST_TEST_MESSAGE("EUR zb = " << mean(eurzb2) << " +- " << error_of<tag::mean>(eurzb2) << " vs analytical "
                                   << d_cirpp.eurYts->discount(T2));
    BOOST_TEST_MESSAGE("USD zb = " << mean(usdzb2) << " +- " << error_of<tag::mean>(usdzb2) << " vs analytical "
                                   << d_cirpp.usdYts->discount(T2) * d_cirpp.fxEurUsd->value());
    BOOST_TEST_MESSAGE("GBP zb = " << mean(gbpzb2) << " +- " << error_of<tag::mean>(gbpzb2) << " vs analytical "
                                   << d_cirpp.gbpYts->discount(T2) * d_cirpp.fxEurGbp->value());
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1eur2) << " +- " << error_of<tag::mean>(n1eur2) << " vs analytical "
                                      << d_cirpp.eurYts->discount(T2) * d_cirpp.n1Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N2 zb USD = " << mean(n2usd2) << " +- " << error_of<tag::mean>(n2usd2) << " vs analytical "
                                      << d_cirpp.fxEurUsd->value() * d_cirpp.usdYts->discount(T2) *
                                             d_cirpp.n2Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N3 zb GBP = " << mean(n3gbp2) << " +- " << error_of<tag::mean>(n3gbp2) << " vs analytical "
                                      << d_cirpp.fxEurGbp->value() * d_cirpp.gbpYts->discount(T2) *
                                             d_cirpp.n3Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1cir2) << " +- " << error_of<tag::mean>(n1cir2) << " vs analytical "
                                      << d_cirpp.eurYts->discount(T2) * d_cirpp.n1Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N2 zb USD = " << mean(n2cir2) << " +- " << error_of<tag::mean>(n2cir2) << " vs analytical "
                                      << d_cirpp.fxEurUsd->value() * d_cirpp.usdYts->discount(T2) *
                                             d_cirpp.n2Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("N3 zb GBP = " << mean(n3cir2) << " +- " << error_of<tag::mean>(n3cir2) << " vs analytical "
                                      << d_cirpp.fxEurGbp->value() * d_cirpp.gbpYts->discount(T2) *
                                             d_cirpp.n3Ts->survivalProbability(T2));

    Real tol1 = 2.0E-4;  // EXACT
    Real tol2 = 12.0E-4; // EULER

    Real ev = d.eurYts->discount(T2);
    if (std::abs(mean(eurzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.), expected " << ev << ", got " << mean(eurzb1)
                                                                                 << ", tolerance " << tol1);
    ev = d.usdYts->discount(T2) * d.fxEurUsd->value();
    if (std::abs(mean(usdzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for usdzb (exact discr.), expected " << ev << ", got " << mean(usdzb1)
                                                                                 << ", tolerance " << tol1);
    ev = d.gbpYts->discount(T2) * d.fxEurGbp->value();
    if (std::abs(mean(gbpzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for gbpzb (exact discr.), expected " << ev << ", got " << mean(gbpzb1)
                                                                                 << ", tolerance " << tol1);
    ev = d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1eur1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for n1eur (exact discr.), expected " << ev << ", got " << mean(n1eur1)
                                                                                 << ", tolerance " << tol1);
    ev = d.fxEurUsd->value() * d.usdYts->discount(T2) * d.n2Ts->survivalProbability(T2);
    if (std::abs(mean(n2usd1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for n2usd (exact discr.), expected " << ev << ", got " << mean(n2usd1)
                                                                                 << ", tolerance " << tol1);
    ev = d.fxEurGbp->value() * d.gbpYts->discount(T2) * d.n3Ts->survivalProbability(T2);
    if (std::abs(mean(n3gbp1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for n3gbp (exact discr.), expected " << ev << ", got " << mean(n3gbp1)
                                                                                 << ", tolerance " << tol1);

    ev = d_cirpp.eurYts->discount(T2);
    if (std::abs(mean(eurzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (Euler discr.), expected " << ev << ", got " << mean(eurzb2)
                                                                                 << ", tolerance " << tol2);
    ev = d_cirpp.usdYts->discount(T2) * d_cirpp.fxEurUsd->value();
    if (std::abs(mean(usdzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for usdzb (Euler discr.), expected "
                   << ev << ", got " << mean(usdzb2) << ", tolerance " << tol2 * error_of<tag::mean>(usdzb2));
    ev = d_cirpp.gbpYts->discount(T2) * d_cirpp.fxEurGbp->value();
    if (std::abs(mean(gbpzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for gbpzb (Euler discr.), expected " << ev << ", got " << mean(gbpzb2)
                                                                                 << ", tolerance " << tol2);

    // CR LGM
    ev = d_cirpp.eurYts->discount(T2) * d_cirpp.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1eur2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for n1eur (Euler discr.), expected " << ev << ", got " << mean(n1eur2)
                                                                                 << ", tolerance " << tol2);
    ev = d_cirpp.fxEurUsd->value() * d_cirpp.usdYts->discount(T2) * d_cirpp.n2Ts->survivalProbability(T2);
    if (std::abs(mean(n2usd2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for n2usd (Euler discr.), expected " << ev << ", got " << mean(n2usd2)
                                                                                 << ", tolerance " << tol2);
    ev = d_cirpp.fxEurGbp->value() * d_cirpp.gbpYts->discount(T2) * d_cirpp.n3Ts->survivalProbability(T2);
    if (std::abs(mean(n3gbp2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for n3gbp (Euler discr.), expected " << ev << ", got " << mean(n3gbp2)
                                                                                 << ", tolerance " << tol2);

    // CR CIR
    ev = d_cirpp.eurYts->discount(T2) * d_cirpp.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1cir2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for n1cir (Euler discr.), expected " << ev << ", got " << mean(n1cir2)
                                                                                 << ", tolerance " << tol2);
    ev = d_cirpp.fxEurUsd->value() * d_cirpp.usdYts->discount(T2) * d_cirpp.n2Ts->survivalProbability(T2);
    if (std::abs(mean(n2cir2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for n2cir2 (Euler discr.), expected " << ev << ", got " << mean(n2cir2)
                                                                                 << ", tolerance " << tol2);
    ev = d_cirpp.fxEurGbp->value() * d_cirpp.gbpYts->discount(T2) * d_cirpp.n3Ts->survivalProbability(T2);
    if (std::abs(mean(n3cir2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for n3cir2 (Euler discr.), expected " << ev << ", got " << mean(n3cir2)
                                                                                 << ", tolerance " << tol2);

} // testIrFxCrMartingaleProperty

BOOST_AUTO_TEST_CASE(testIrFxCrMoments) {

    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization "
                       "in ir-fx-cr model...");

    IrFxCrModelTestData d;

    QuantLib::ext::shared_ptr<StochasticProcess> p_exact = d.modelExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> p_euler = d.modelEuler->stateProcess();

    Real T = 2.0;                           // horizon at which we compare the moments
    Size steps = static_cast<Size>(T * 10); // number of simulation steps (Euler and exact)
    Size paths = 50000;                     // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);

    Size seed = 18;
    TimeGrid grid(T, steps);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_exact)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorSobolBrownianBridge pgen(p_euler, grid, SobolBrownianGenerator::Diagonal, seed,
                                               SobolRsg::JoeKuoD7);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_euler)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorSobolBrownianBridge pgen2(p_exact, grid, SobolBrownianGenerator::Diagonal, seed,
                                                SobolRsg::JoeKuoD7);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > e_eu[11], e_eu2[11];
    accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > v_eu[11][11], v_eu2[11][11];

    for (Size i = 0; i < paths; ++i) {
        Sample<MultiPath> path = pgen.next();
        Sample<MultiPath> path2 = pgen2.next();
        for (Size ii = 0; ii < 11; ++ii) {
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

    for (Size i = 0; i < 11; ++i) {
        BOOST_TEST_MESSAGE("E_" << i << " " << e_an[i] << " " << mean(e_eu[i]) << " " << mean(e_eu2[i]));
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("one step analytical");
    for (Size i = 0; i < 11; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << v_an[i][j] << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("euler");
    for (Size i = 0; i < 11; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << boost::accumulators::covariance(v_eu[i][j]) << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("exact");
    for (Size i = 0; i < 11; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << boost::accumulators::covariance(v_eu2[i][j]) << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    Real errTolLd[] = { 0.5E-4, 0.5E-4, 0.5E-4, 10.0E-4, 10.0E-4, 0.7E-4, 0.7E-4, 0.7E-4, 0.7E-4, 0.7E-4, 0.7E-4 };

    for (Size i = 0; i < 11; ++i) {
        // check expectation against analytical calculation (Euler)
        if (std::fabs(mean(e_eu[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (Euler "
                                                                    "discretization, "
                                                                 << mean(e_eu[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu[i]) << " tolerance is "
                                                                 << errTolLd[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (exact "
                                                                    "discretization, "
                                                                 << mean(e_eu2[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu2[i]) << " tolerance is "
                                                                 << errTolLd[i]);
        }
    }

    // this is a bit rough compared to the more differentiated test
    // of the IR-FX model ...
    Real tol = 10.0E-4;

    for (Size i = 0; i < 11; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (std::fabs(boost::accumulators::covariance(v_eu[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Euler discretization, "
                            << boost::accumulators::covariance(v_eu[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu[i][j]) << " tolerance is " << tol);
            }
            if (std::fabs(boost::accumulators::covariance(v_eu2[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (exact discretization, "
                            << boost::accumulators::covariance(v_eu2[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu2[i][j]) << " tolerance is " << tol);
            }
        }
    }

} // testIrFxCrMoments

BOOST_AUTO_TEST_CASE(testIrFxCrCorrelationRecovery) {

    BOOST_TEST_MESSAGE("Test if random correlation input is recovered for "
                       "small dt in ir-fx-cr model...");

    class PseudoCurrency : public Currency {
    public:
        PseudoCurrency(const Size id) {
            std::ostringstream ln, sn;
            ln << "Dummy " << id;
            sn << "DUM " << id;
            data_ = QuantLib::ext::make_shared<Data>(ln.str(), sn.str(), id, sn.str(), "", 100, Rounding(), "%3% %1$.2f");
        }
    };

    Real dt = 1.0E-6;
    Real tol = 1.0E-7;

    // for ir-fx this fully specifies the correlation matrix
    // for new asset classes add other possible combinations as well
    Size currencies[] = { 1, 2, 3, 4, 5, 10, 20 };
    Size creditnames[] = { 0, 1, 5, 10 };

    MersenneTwisterUniformRng mt(42);

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));

    Handle<DefaultProbabilityTermStructure> hts(
        QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.01, Actual365Fixed()));

    Handle<Quote> fxspot(QuantLib::ext::make_shared<SimpleQuote>(1.00));

    Array notimes(0);
    Array fxsigma(1, 0.10);

    for (Size ii = 0; ii < LENGTH(currencies); ++ii) {
        for (Size jj = 0; jj < LENGTH(creditnames); ++jj) {

            std::vector<Currency> pseudoCcy;
            for (Size i = 0; i < currencies[ii]; ++i) {
                PseudoCurrency tmp(i);
                pseudoCcy.push_back(tmp);
            }

            Size dim = 2 * currencies[ii] - 1 + creditnames[jj];

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

            std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;

            // IR
            for (Size i = 0; i < currencies[ii]; ++i) {
                parametrizations.push_back(
                    QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(pseudoCcy[i], yts, 0.01, 0.01));
            }
            // FX
            for (Size i = 0; i < currencies[ii] - 1; ++i) {
                parametrizations.push_back(QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(
                    pseudoCcy[i + 1], fxspot, notimes, fxsigma));
            }
            // CR
            for (Size i = 0; i < creditnames[jj]; ++i) {
                parametrizations.push_back(
                    QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(pseudoCcy[0], hts, 0.01, 0.01));
            }

            // get QuantLib::Error: negative eigenvalue(s) (-3.649315e-16) with SalvagingAlgorithm::None
            QuantLib::ext::shared_ptr<CrossAssetModel> modelExact =
                QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, c, SalvagingAlgorithm::Spectral,
                                                    IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);
            QuantLib::ext::shared_ptr<CrossAssetModel> modelEuler =
                QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, c, SalvagingAlgorithm::Spectral,
                                                    IrModel::Measure::LGM, CrossAssetModel::Discretization::Euler);

            QuantLib::ext::shared_ptr<StochasticProcess> peuler = modelEuler->stateProcess();
            QuantLib::ext::shared_ptr<StochasticProcess> pexact = modelExact->stateProcess();

            Matrix c1 = peuler->covariance(dt, peuler->initialValues(), dt);
            Matrix c2 = pexact->covariance(0.0, peuler->initialValues(), dt);

            Matrix r1(dim, dim), r2(dim, dim);

            for (Size i = 0; i < dim; ++i) {
                for (Size j = 0; j <= i; ++j) {
                    // there are two state variables per credit name,
                    Size subi = i < 2 * currencies[ii] - 1 ? 1 : 2;
                    Size subj = j < 2 * currencies[ii] - 1 ? 1 : 2;
                    for (Size k1 = 0; k1 < subi; ++k1) {
                        for (Size k2 = 0; k2 < subj; ++k2) {
                            Size i0 = i < 2 * currencies[ii] - 1
                                          ? i
                                          : 2 * currencies[ii] - 1 + 2 * (i - (2 * currencies[ii] - 1)) + k1;
                            Size j0 = j < 2 * currencies[ii] - 1
                                          ? j
                                          : 2 * currencies[ii] - 1 + 2 * (j - (2 * currencies[ii] - 1)) + k2;
                            r1[i][j] = r1[j][i] = c1[i0][j0] / std::sqrt(c1[i0][i0] * c1[j0][j0]);
                            r2[i][j] = r2[j][i] = c2[i0][j0] / std::sqrt(c2[i0][i0] * c2[j0][j0]);
                            if (std::fabs(r1[i][j] - c[i][j]) > tol) {
                                BOOST_ERROR("failed to recover correlation matrix from "
                                            "Euler state process (i,j)=("
                                            << i << "," << j << "), (i0,j0)=(" << i0 << "," << j0
                                            << "), input correlation is " << c[i][j] << ", output is " << r1[i][j]
                                            << ", difference " << (c[i][j] - r1[i][j]) << ", tolerance " << tol
                                            << " test configuration is " << currencies[ii] << " currencies and "
                                            << creditnames[jj] << " credit names");
                            }
                            if (k1 == k2) {
                                if (std::fabs(r2[i][j] - c[i][j]) > tol) {
                                    BOOST_ERROR("failed to recover correlation matrix "
                                                "from "
                                                "exact state process (i,j)=("
                                                << i << "," << j << "), (i0,j0)=(" << i0 << "," << j0
                                                << "), input correlation is " << c[i][j] << ", output is " << r2[i][j]
                                                << ", difference " << (c[i][j] - r2[i][j]) << ", tolerance " << tol
                                                << " test configuration is " << currencies[ii] << " currencies and "
                                                << creditnames[jj] << " credit names");
                                }
                            }
                        }
                    }
                }
            }
        } // for creditnames
    }     // for currencies

} // testIrFxCrCorrelationRecovery

namespace {

// arbitrary commodity price curve
std::vector<Period> comTerms = { 1*Days, 1*Years, 2*Years, 3*Years, 5*Years, 10*Years, 15*Years, 20*Years, 30*Years };
std::vector<Real> comPrices { 100, 101, 102, 103, 105, 110, 115, 120, 130 };

struct IrFxInfCrComModelTestData {
    
    IrFxInfCrComModelTestData(bool infEurIsDK = true, bool infGbpIsDK = true, bool flatVols = false, bool driftFreeState = false)
        : referenceDate(30, July, 2015),
          dc(Actual365Fixed()),
          eurYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, dc)),
          usdYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.05, dc)),
          gbpYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.04, dc)),
          fxEurUsd(QuantLib::ext::make_shared<SimpleQuote>(0.90)),
          fxEurGbp(QuantLib::ext::make_shared<SimpleQuote>(1.35)),
          n1Ts(QuantLib::ext::make_shared<FlatHazardRate>(referenceDate, 0.01, dc)),
          comTS(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(comTerms, comPrices, dc, USDCurrency())) {
        
        Settings::instance().evaluationDate() = referenceDate;
        
        // Store the individual models that will be fed to the CAM.
        vector<QuantLib::ext::shared_ptr<Parametrization> > singleModels;

        // IR parameterisations
        addSingleIrModel(flatVols, EURCurrency(), eurYts, 0.02, 0.0050, 0.0080, singleModels);
        addSingleIrModel(flatVols, USDCurrency(), usdYts, 0.03, 0.0030, 0.0110, singleModels);
        addSingleIrModel(flatVols, GBPCurrency(), gbpYts, 0.04, 0.0070, 0.0095, singleModels);

        // FX parameterisations
        addSingleFxModel(flatVols, USDCurrency(), fxEurUsd, 0.15, 0.20, singleModels);
        addSingleFxModel(flatVols, GBPCurrency(), fxEurGbp, 0.10, 0.15, singleModels);

        // Add the inflation parameterisations.
        vector<Date> infDates{ Date(30, April, 2015), Date(30, July, 2015) };
        vector<Real> infRates{ 0.01, 0.01 };
        
        infEurTs = Handle<ZeroInflationTermStructure>(QuantLib::ext::make_shared<ZeroInflationCurve>(
            referenceDate, TARGET(), dc, 3 * Months, Monthly, infDates, infRates));
        infEurTs->enableExtrapolation();
        
        infLag = inflationYearFraction(Monthly, false, dc, infEurTs->baseDate(), infEurTs->referenceDate());

        Real infEurAlpha = 0.01;
        Real infEurKappa = 0.01;
        if (infEurIsDK) {
            singleModels.push_back(QuantLib::ext::make_shared<InfDkConstantParametrization>(
                EURCurrency(), infEurTs, infEurAlpha, infEurKappa));
        } else {
            Real infEurSigma = 0.15;
            Handle<Quote> baseCpiQuote(QuantLib::ext::make_shared<SimpleQuote>(1.0));
            auto index = QuantLib::ext::make_shared<EUHICP>(false);
            auto realRateParam = QuantLib::ext::make_shared<Lgm1fConstantParametrization<ZeroInflationTermStructure>>(
                EURCurrency(), infEurTs, infEurAlpha, infEurKappa);
            auto indexParam = QuantLib::ext::make_shared<FxBsConstantParametrization>(
                EURCurrency(), baseCpiQuote, infEurSigma);
            singleModels.push_back(QuantLib::ext::make_shared<InfJyParameterization>(realRateParam, indexParam, index));
        }

        infGbpTs = Handle<ZeroInflationTermStructure>(QuantLib::ext::make_shared<ZeroInflationCurve>(referenceDate,
            UnitedKingdom(), dc, 3 * Months, Monthly, infDates, infRates));
        infGbpTs->enableExtrapolation();

        Real infGbpAlpha = 0.01;
        Real infGbpKappa = 0.01;
        if (infGbpIsDK) {
            singleModels.push_back(QuantLib::ext::make_shared<InfDkConstantParametrization>(
                GBPCurrency(), infGbpTs, infGbpAlpha, infGbpKappa));
        } else {
            Real infGbpSigma = 0.10;
            Handle<Quote> baseCpiQuote(QuantLib::ext::make_shared<SimpleQuote>(1.0));
            auto index = QuantLib::ext::make_shared<UKRPI>(false);
            auto realRateParam = QuantLib::ext::make_shared<Lgm1fConstantParametrization<ZeroInflationTermStructure>>(
                GBPCurrency(), infGbpTs, infGbpAlpha, infGbpKappa);
            auto indexParam = QuantLib::ext::make_shared<FxBsConstantParametrization>(
                GBPCurrency(), baseCpiQuote, infGbpSigma);
            singleModels.push_back(QuantLib::ext::make_shared<InfJyParameterization>(realRateParam, indexParam, index));
        }

        // Add the credit parameterisation.
        singleModels.push_back(QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(
            EURCurrency(), n1Ts, 0.01, 0.01));

        // Add commodity parameterisations
        bool df = driftFreeState;
        comParametrizationA = QuantLib::ext::make_shared<CommoditySchwartzParametrization>(USDCurrency(), "WTI", comTS, fxEurUsd, 0.1, 0.05, df);
        comParametrizationB = QuantLib::ext::make_shared<CommoditySchwartzParametrization>(USDCurrency(), "NG", comTS, fxEurUsd, 0.15, 0.05, df);
        comModelA = QuantLib::ext::make_shared<CommoditySchwartzModel>(comParametrizationA);
        comModelB = QuantLib::ext::make_shared<CommoditySchwartzModel>(comParametrizationB);
        singleModels.push_back(comParametrizationA);
        singleModels.push_back(comParametrizationB);

        // Create the correlation matrix.
        Matrix c = createCorrelation(infEurIsDK, infGbpIsDK);
        BOOST_TEST_MESSAGE("correlation matrix is\n" << c);

        BOOST_TEST_MESSAGE("creating CAM with exact discretization");
        modelExact = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);
        BOOST_TEST_MESSAGE("creating CAM with Euler discretization");
        modelEuler = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Euler);
        BOOST_TEST_MESSAGE("test date done");
    }

    // Add an IR model with flat or time depending volatilities.
    void addSingleIrModel(bool flatVols, const Currency& ccy, Handle<YieldTermStructure> yts, Real kappa,
        Volatility lowerBound, Volatility upperBound,
        vector<QuantLib::ext::shared_ptr<Parametrization> >& singleModels) {

        // If use flat vols, add the parameterisation and return.
        if (flatVols) {
            singleModels.push_back(QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(ccy, yts, lowerBound, kappa));
            return;
        }

        // Use non-flat volatilities
        static vector<Date> vDates{
            Date(15, July, 2016),
            Date(15, July, 2017),
            Date(15, July, 2018),
            Date(15, July, 2019),
            Date(15, July, 2020)
        };

        Array vTimes(vDates.size());
        for (Size i = 0; i < vTimes.size(); ++i) {
            vTimes[i] = yts->timeFromReference(vDates[i]);
        }

        Array vols(vDates.size() + 1);
        for (Size i = 0; i < vols.size(); ++i) {
            vols[i] = lowerBound + (upperBound - lowerBound) * std::exp(-0.3 * i);
        }

        singleModels.push_back(QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(
            ccy, yts, vTimes, vols, Array(), Array(1, kappa)));
    }

    // Add an FX model with flat or time depending volatilities.
    void addSingleFxModel(bool flatVols, const Currency& ccy, Handle<Quote> spot,
        Volatility lowerBound, Volatility upperBound,
        vector<QuantLib::ext::shared_ptr<Parametrization> >& singleModels) {

        // If use flat vols, add the parameterisation and return.
        if (flatVols) {
            singleModels.push_back(QuantLib::ext::make_shared<FxBsConstantParametrization>(ccy, spot, lowerBound));
            return;
        }

        // Use non-flat volatilities
        static vector<Date> vDates{
            Date(15, July, 2016),
            Date(15, October, 2016),
            Date(15, May, 2017),
            Date(13, September, 2017),
            Date(15, July, 2018)
        };

        Array vTimes(vDates.size());
        for (Size i = 0; i < vTimes.size(); ++i) {
            vTimes[i] = dc.yearFraction(referenceDate, vDates[i]);
        }

        Array vols(vDates.size() + 1);
        for (Size i = 0; i < vols.size(); ++i) {
            vols[i] = lowerBound + (upperBound - lowerBound) * std::exp(-0.3 * i);
        }

        singleModels.push_back(QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(
            ccy, spot, vTimes, vols));
    }

    // Create the correlation matrix.
    Matrix createCorrelation(bool infEurIsDK, bool infGbpIsDK) {

        // Four different correlation matrices depending on type of two inflation components.
        vector<vector<Real>> tmp;
        if (infEurIsDK && infGbpIsDK) {
            tmp = {
                // EUR  USD GBP  FX1  FX2  INF_EUR INF_GBP CR COM1 COM2
                { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // EUR
                { 0.6, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // USD
                { 0.3, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // GBP
                { 0.2, 0.2, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // FX1
                { 0.3, 0.1, 0.1, 0.3, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, // FX2
                { 0.8, 0.2, 0.1, 0.4, 0.2, 1.0, 0.0, 0.0, 0.0, 0.0 }, // INF_EUR
                { 0.6, 0.1, 0.2, 0.2, 0.5, 0.5, 1.0, 0.0, 0.0, 0.0 }, // INF_GBP
                { 0.3, 0.2, 0.1, 0.1, 0.3, 0.4, 0.2, 1.0, 0.0, 0.0 }, // CR
                { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 }, // COM1
                { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0 }  // COM2
            };
        } else if (!infEurIsDK && infGbpIsDK) {
            tmp = {
                {1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_EUR
                {0.600, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_USD
                {0.300, 0.100, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_GBP
                {0.200, 0.200, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // FX_EURUSD
                {0.300, 0.100, 0.100, 0.300, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // FX_EURGBP
                {0.400, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // INF_EUR_RR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000}, // INF_EUR_IDX
                {0.000, 0.000, 0.600, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000}, // INF_GBP
                {0.300, 0.200, 0.100, 0.100, 0.300, 0.400, 0.000, 0.200, 1.000, 0.000, 0.000}, // CR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000}, // COM1
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.500, 1.000}  // COM2
            };
        } else if (infEurIsDK && !infGbpIsDK) {
            tmp = {
                {1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_EUR
                {0.600, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_USD
                {0.300, 0.100, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_GBP
                {0.200, 0.200, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // FX_EURUSD
                {0.300, 0.100, 0.100, 0.300, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // FX_EURGBP
                {0.600, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // INF_EUR
                {0.000, 0.000, 0.400, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000}, // INF_GBP_RR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000}, // INF_GBP_IDX
                {0.300, 0.200, 0.100, 0.100, 0.300, 0.400, 0.200, 0.000, 1.000, 0.000, 0.000}, // CR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000}, // COM1
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.500, 1.000}  // COM2
            };
        } else {
            tmp = {
                {1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_EUR
                {0.600, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_USD
                {0.300, 0.100, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // IR_GBP
                {0.200, 0.200, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // FX_EURUSD
                {0.300, 0.100, 0.100, 0.300, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // FX_EURGBP
                {0.600, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // INF_EUR_RR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000, 0.000}, // INF_EUR_IDX
                {0.000, 0.000, 0.600, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000, 0.000}, // INF_GBP_RR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000, 0.000, 0.000}, // INF_GBP_IDX
                {0.300, 0.200, 0.100, 0.100, 0.300, 0.400, 0.000, 0.200, 0.000, 1.000, 0.000, 0.000}, // CR
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.000}, // COM1
                {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.500, 1.000}  // COM2
            };
        }

        Matrix c(tmp.size(), tmp.size());
        for (Size i = 0; i < tmp.size(); ++i) {
            for (Size j = 0; j <= i; ++j) {
                c[i][j] = c[j][i] = tmp[i][j];
            }
        }

        return c;
    }

    SavedSettings backup;
    Date referenceDate;
    DayCounter dc;
    
    // IR
    Handle<YieldTermStructure> eurYts;
    Handle<YieldTermStructure> usdYts;
    Handle<YieldTermStructure> gbpYts;

    // FX
    Handle<Quote> fxEurUsd;
    Handle<Quote> fxEurGbp;
    
    // Inflation
    Handle<ZeroInflationTermStructure> infEurTs;
    Handle<ZeroInflationTermStructure> infGbpTs;
    Real infLag;

    // Credit
    Handle<DefaultProbabilityTermStructure> n1Ts;
    
    // Commodities
    Handle<QuantExt::PriceTermStructure> comTS;
    QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> comParametrizationA, comParametrizationB;
    QuantLib::ext::shared_ptr<CommoditySchwartzModel> comModelA, comModelB;
    QuantLib::ext::shared_ptr<CommoditySchwartzStateProcess> comProcessA, comProcessB;

    // Model
    QuantLib::ext::shared_ptr<CrossAssetModel> modelExact, modelEuler;

}; // IrFxInfCrModelTestData

} // anonymous namespace

// Test case options
vector<bool> infEurFlags{ true, false };
vector<bool> infGbpFlags{ true, false };
vector<bool> flatVolsFlags{ true, false };
vector<bool> driftFreeState{ true, false };

BOOST_DATA_TEST_CASE(testIrFxInfCrComMartingaleProperty,
    bdata::make(infEurFlags) * bdata::make(infGbpFlags) * bdata::make(flatVolsFlags) * bdata::make(driftFreeState),
                     infEurIsDk, infGbpIsDk, flatVols, driftFreeState) {

    BOOST_TEST_MESSAGE("Testing martingale property in ir-fx-inf-cr-com model for Euler and exact discretizations...");
    BOOST_TEST_MESSAGE("EUR inflation model is: " << (infEurIsDk ? "DK" : "JY"));
    BOOST_TEST_MESSAGE("GBP inflation model is: " << (infGbpIsDk ? "DK" : "JY"));
    BOOST_TEST_MESSAGE("Using " << (flatVols ? "" : "non-") << "flat volatilities.");

    IrFxInfCrComModelTestData d(infEurIsDk, infGbpIsDk, flatVols, driftFreeState);

    BOOST_TEST_MESSAGE("get exact state process");
    QuantLib::ext::shared_ptr<StochasticProcess> process1 = d.modelExact->stateProcess();
    BOOST_TEST_MESSAGE("get Euler state process");
    QuantLib::ext::shared_ptr<StochasticProcess> process2 = d.modelEuler->stateProcess();

    Size n = 5000;                         // number of paths
    Size seed = 18;                         // rng seed
    Time T = 2.0;                          // maturity of payoff
    Time T2 = 20.0;                         // zerobond maturity
    Size steps = static_cast<Size>(T * 40); // number of steps taken (euler)

    BOOST_TEST_MESSAGE("build sequence generators");
    LowDiscrepancy::rsg_type sg1 = LowDiscrepancy::make_sequence_generator(process1->factors() * 1, seed);
    LowDiscrepancy::rsg_type sg2 = LowDiscrepancy::make_sequence_generator(process2->factors() * steps, seed);

    BOOST_TEST_MESSAGE("build multi path generator");
    TimeGrid grid1(T, 1);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process1)) {
        tmp->resetCache(grid1.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg1(process1, grid1, sg1, false);
    TimeGrid grid2(T, steps);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process2)) {
        tmp->resetCache(grid2.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg2(process2, grid2, sg2, false);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > eurzb1, usdzb1, gbpzb1, infeur1, infgbp1,
        n1eur1, commodityA_1, commodityB_1;
    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > eurzb2, usdzb2, gbpzb2, infeur2, infgbp2,
        n1eur2, commodityA_2, commodityB_2;

    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path2 = pg2.next();
        Size l2 = path2.value[0].length() - 1;
        Sample<MultiPath> path1 = pg1.next();
        Size l1 = path1.value[0].length() - 1;
        Real zeur1 = path1.value[0][l1];
        Real zusd1 = path1.value[1][l1];
        Real zgbp1 = path1.value[2][l1];
        Real fxusd1 = std::exp(path1.value[3][l1]);
        Real fxgbp1 = std::exp(path1.value[4][l1]);
        Real infeurz1 = path1.value[5][l1];
        Real infeury1 = path1.value[6][l1];
        Real infgbpz1 = path1.value[7][l1];
        Real infgbpy1 = path1.value[8][l1];
        Real crzn11 = path1.value[9][l1];
        Real cryn11 = path1.value[10][l1];
        Real coma1 = path1.value[11][l1];
        Real comb1 = path1.value[12][l1];
        Real zeur2 = path2.value[0][l2];
        Real zusd2 = path2.value[1][l2];
        Real zgbp2 = path2.value[2][l2];
        Real fxusd2 = std::exp(path2.value[3][l2]);
        Real fxgbp2 = std::exp(path2.value[4][l2]);
        Real infeurz2 = path2.value[5][l2];
        Real infeury2 = path2.value[6][l2];
        Real infgbpz2 = path2.value[7][l2];
        Real infgbpy2 = path2.value[8][l2];
        Real crzn12 = path2.value[9][l2];
        Real cryn12 = path2.value[10][l2];
        Real coma2 = path2.value[11][l2];
        Real comb2 = path2.value[12][l2];
        // EUR zerobond
        eurzb1(d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));
        // USD zerobond
        usdzb1(d.modelExact->discountBond(1, T, T2, zusd1) * fxusd1 / d.modelExact->numeraire(0, T, zeur1));
        // GBP zerobond
        gbpzb1(d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 / d.modelExact->numeraire(0, T, zeur1));
        // EUR CPI indexed bond
        bool indexIsInterpolated = true;
        if (infEurIsDk) {
            std::pair<Real, Real> sinfeur1 = d.modelExact->infdkI(0, T, T2, infeurz1, infeury1);
            infeur1(sinfeur1.first * sinfeur1.second * d.modelExact->discountBond(0, T, T2, zeur1) /
                d.modelExact->numeraire(0, T, zeur1));
        } else {
            infeur1(exp(infeury1) * inflationGrowth(d.modelExact, 0, T, T2, zeur1, infeurz1, indexIsInterpolated) *
                    d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));
        }
        // GBP CPI indexed bond
        if (infGbpIsDk) {
            std::pair<Real, Real> sinfgbp1 = d.modelExact->infdkI(1, T, T2, infgbpz1, infgbpy1);
            infgbp1(sinfgbp1.first * sinfgbp1.second * d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 /
                d.modelExact->numeraire(0, T, zeur1));
        } else {
            infgbp1(exp(infgbpy1) * inflationGrowth(d.modelExact, 1, T, T2, zgbp1, infgbpz1, indexIsInterpolated) *
                d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 / d.modelExact->numeraire(0, T, zeur1));
        }
        // EUR defaultable zerobond
        std::pair<Real, Real> sn11 = d.modelExact->crlgm1fS(0, 0, T, T2, crzn11, cryn11);
        n1eur1(sn11.first * sn11.second * d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));

        // EUR zerobond
        eurzb2(d.modelExact->discountBond(0, T, T2, zeur2) / d.modelExact->numeraire(0, T, zeur2));
        // USD zerobond
        usdzb2(d.modelExact->discountBond(1, T, T2, zusd2) * fxusd2 / d.modelExact->numeraire(0, T, zeur2));
        // GBP zerobond
        gbpzb2(d.modelExact->discountBond(2, T, T2, zgbp2) * fxgbp2 / d.modelExact->numeraire(0, T, zeur2));
        // EUR CPI indexed bond
        if (infEurIsDk) {
            std::pair<Real, Real> sinfeur2 = d.modelExact->infdkI(0, T, T2, infeurz2, infeury2);
            infeur2(sinfeur2.first * sinfeur2.second * d.modelExact->discountBond(0, T, T2, zeur2) /
                d.modelExact->numeraire(0, T, zeur2));
        } else {
            infeur2(exp(infeury2) * inflationGrowth(d.modelExact, 0, T, T2, zeur2, infeurz2, indexIsInterpolated) *
                    d.modelExact->discountBond(0, T, T2, zeur2) / d.modelExact->numeraire(0, T, zeur2));
        }
        // GBP CPI indexed bond
        if (infGbpIsDk) {
            std::pair<Real, Real> sinfgbp2 = d.modelExact->infdkI(1, T, T2, infgbpz2, infgbpy2);
            infgbp2(sinfgbp2.first * sinfgbp2.second * d.modelExact->discountBond(2, T, T2, zgbp2) * fxgbp2 /
                d.modelExact->numeraire(0, T, zeur2));
        } else {
            infgbp2(exp(infgbpy2) * inflationGrowth(d.modelExact, 1, T, T2, zgbp2, infgbpz2, indexIsInterpolated) *
                    d.modelExact->discountBond(2, T, T2, zgbp2) * fxgbp2 / d.modelExact->numeraire(0, T, zeur2));
        }
        // EUR defaultable zerobond
        std::pair<Real, Real> sn12 = d.modelExact->crlgm1fS(0, 0, T, T2, crzn12, cryn12);
        n1eur2(sn12.first * sn12.second * d.modelExact->discountBond(0, T, T2, zeur2) / d.modelExact->numeraire(0, T, zeur2));
        // commodity forward prices
        commodityA_1(d.comModelA->forwardPrice(T, T2, Array(1, coma1)));
        commodityB_1(d.comModelB->forwardPrice(T, T2, Array(1, comb1)));
        commodityA_2(d.comModelA->forwardPrice(T, T2, Array(1, coma2)));
        commodityB_2(d.comModelB->forwardPrice(T, T2, Array(1, comb2)));
    }

    BOOST_TEST_MESSAGE("EXACT:");
    BOOST_TEST_MESSAGE("EUR zb = " << mean(eurzb1) << " +- " << error_of<tag::mean>(eurzb1) << " vs analytical "
                                   << d.eurYts->discount(T2));
    BOOST_TEST_MESSAGE("USD zb = " << mean(usdzb1) << " +- " << error_of<tag::mean>(usdzb1) << " vs analytical "
                                   << d.usdYts->discount(T2) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("GBP zb = " << mean(gbpzb1) << " +- " << error_of<tag::mean>(gbpzb1) << " vs analytical "
                                   << d.gbpYts->discount(T2) * d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("IDX zb EUR = " << mean(infeur1) << " +- " << error_of<tag::mean>(infeur1) << " vs analytical "
                                       << d.eurYts->discount(T2) *
                                              std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2));
    BOOST_TEST_MESSAGE("IDX zb GBP = " << mean(infgbp1) << " +- " << error_of<tag::mean>(infgbp1) << " vs analytical "
                                       << d.gbpYts->discount(T2) *
                                              std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) *
                                              d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1eur1) << " +- " << error_of<tag::mean>(n1eur1) << " vs analytical "
                                      << d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2));

    BOOST_TEST_MESSAGE("\nEULER:");
    BOOST_TEST_MESSAGE("EUR zb = " << mean(eurzb2) << " +- " << error_of<tag::mean>(eurzb2) << " vs analytical "
                                   << d.eurYts->discount(T2));
    BOOST_TEST_MESSAGE("USD zb = " << mean(usdzb2) << " +- " << error_of<tag::mean>(usdzb2) << " vs analytical "
                                   << d.usdYts->discount(T2) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("GBP zb = " << mean(gbpzb2) << " +- " << error_of<tag::mean>(gbpzb2) << " vs analytical "
                                   << d.gbpYts->discount(T2) * d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("IDX zb EUR = " << mean(infeur2) << " +- " << error_of<tag::mean>(infeur2) << " vs analytical "
                                       << d.eurYts->discount(T2) *
                                              std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2));
    BOOST_TEST_MESSAGE("IDX zb GBP = " << mean(infgbp2) << " +- " << error_of<tag::mean>(infgbp2) << " vs analytical "
                                       << d.gbpYts->discount(T2) *
                                              std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) *
                                              d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1eur2) << " +- " << error_of<tag::mean>(n1eur2) << " vs analytical "
                                      << d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2));

    // a bit higher than for plain zero bond , since we look at indexed zero
    // bonds, too
    Real tol1 = 5.0E-4;  // EXACT
    Real tol2 = 14.0E-4; // EULER

    Real ev = d.eurYts->discount(T2);
    if (std::abs(mean(eurzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(eurzb1) << ", tolerance " << tol1);
    ev = d.usdYts->discount(T2) * d.fxEurUsd->value();
    if (std::abs(mean(usdzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(usdzb1) << ", tolerance " << tol1);
    ev = d.gbpYts->discount(T2) * d.fxEurGbp->value();
    if (std::abs(mean(gbpzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(gbpzb1) << ", tolerance " << tol1);
    ev = d.eurYts->discount(T2) * std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2);
    if (std::abs(mean(infeur1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for idx eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(infeur1) << ", tolerance " << tol1);
    ev = d.gbpYts->discount(T2) * std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) * d.fxEurGbp->value();
    if (std::abs(mean(infgbp1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for idx gbpzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(infgbp1) << ", tolerance " << tol1);
    ev = d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1eur1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for def eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(n1eur1) << ", tolerance " << tol1);

    ev = d.eurYts->discount(T2);
    if (std::abs(mean(eurzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(eurzb2) << ", tolerance " << tol2);
    ev = d.usdYts->discount(T2) * d.fxEurUsd->value();
    if (std::abs(mean(usdzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for usdzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(usdzb2) << ", tolerance " << tol2 * error_of<tag::mean>(usdzb2));
    ev = d.gbpYts->discount(T2) * d.fxEurGbp->value();
    if (std::abs(mean(gbpzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for gbpzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(gbpzb2) << ", tolerance " << tol2);
    ev = d.eurYts->discount(T2) * std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2);
    if (std::abs(mean(infeur2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for idx eurzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(infeur2) << ", tolerance " << tol1);
    ev = d.gbpYts->discount(T2) * std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) * d.fxEurGbp->value();
    if (std::abs(mean(infgbp2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for idx gbpzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(infgbp2) << ", tolerance " << tol1);
    ev = d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1eur2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for def eurzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(n1eur1) << ", tolerance " << tol1);

    // commodity A forward prices
    Real tol;
    ev = d.comParametrizationA->priceCurve()->price(T2);
    tol = error_of<tag::mean>(commodityA_1);
    if (std::abs(mean(commodityA_1) - ev) > tol)
        BOOST_TEST_ERROR("Martingale test failed for commodity A (exact discr.),"
                         "expected " << ev << ", got " << mean(commodityA_1) << " +- " << tol);
    tol = error_of<tag::mean>(commodityA_2);
    if (std::abs(mean(commodityA_2) - ev) > tol)
        BOOST_TEST_ERROR("Martingale test failed for commodity A (Euler discr.),"
                         "expected " << ev << ", got " << mean(commodityA_2) << " +- " << tol);

    // commodity B forward prices
    ev = d.comParametrizationB->priceCurve()->price(T2); 
    tol = error_of<tag::mean>(commodityB_1);
    if (std::abs(mean(commodityB_1) - ev) > tol)
        BOOST_TEST_ERROR("Martingale test failed for commodity B (exact discr.),"
                         "expected " << ev << ", got " << mean(commodityB_1) << " +- " << tol);
    tol = error_of<tag::mean>(commodityB_2);
    if (std::abs(mean(commodityB_2) - ev) > tol)
        BOOST_TEST_ERROR("Martingale test failed for commodity B (Euler discr.),"
                         "expected " << ev << ", got " << mean(commodityB_2) << " +- " << tol);

    
} // testIrFxInfCrMartingaleProperty

BOOST_DATA_TEST_CASE(testIrFxInfCrComMoments,
    bdata::make(infEurFlags) * bdata::make(infGbpFlags) * bdata::make(flatVolsFlags) * bdata::make(driftFreeState),
                     infEurIsDk, infGbpIsDk, flatVols, driftFreeState) {

    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization in ir-fx-inf-cr-com model...");
    BOOST_TEST_MESSAGE("EUR inflation model is: " << (infEurIsDk ? "DK" : "JY"));
    BOOST_TEST_MESSAGE("GBP inflation model is: " << (infGbpIsDk ? "DK" : "JY"));
    BOOST_TEST_MESSAGE("Using " << (flatVols ? "" : "non-") << "flat volatilities.");

    IrFxInfCrComModelTestData d(infEurIsDk, infGbpIsDk, flatVols, driftFreeState);

    Size n = d.modelExact->dimension();

    QuantLib::ext::shared_ptr<StochasticProcess> p_exact = d.modelExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> p_euler = d.modelExact->stateProcess();

    Real T = 2.0;                            // horizon at which we compare the moments
    Size steps = static_cast<Size>(T * 10); // number of simulation steps (Euler and exact)
    Size paths = 10000;                     // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);

    Size seed = 18;
    TimeGrid grid(T, steps);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_euler)) {
        tmp->resetCache(grid.size() - 1);
    }
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_exact)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorSobolBrownianBridge pgen(p_euler, grid, SobolBrownianGenerator::Diagonal, seed,
                                               SobolRsg::JoeKuoD7);
    MultiPathGeneratorSobolBrownianBridge pgen2(p_exact, grid, SobolBrownianGenerator::Diagonal, seed,
                                                SobolRsg::JoeKuoD7);

    vector<accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > > e_eu(n);
    vector<accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > > e_eu2(n);
    vector<vector<accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > > > v_eu(n);
    vector<vector<accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > > > v_eu2(n);
    for (Size i = 0; i < n; ++i) {
        v_eu[i] = vector<accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > >(n);
        v_eu2[i] = vector<accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > >(n);
    }

    for (Size i = 0; i < paths; ++i) {
        Sample<MultiPath> path = pgen.next();
        Sample<MultiPath> path2 = pgen2.next();
        for (Size ii = 0; ii < n; ++ii) {
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

    for (Size i = 0; i < n; ++i) {
        BOOST_TEST_MESSAGE("E_" << i << " " << std::fixed << std::setprecision(12) <<
            e_an[i] << " " << mean(e_eu[i]) << " " << mean(e_eu2[i]));
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("one step analytical");
    for (Size i = 0; i < n; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << v_an[i][j] << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("euler");
    for (Size i = 0; i < n; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << boost::accumulators::covariance(v_eu[i][j]) << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("exact");
    for (Size i = 0; i < n; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << boost::accumulators::covariance(v_eu2[i][j]) << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    Real errTolLd[] = { 0.5E-4, 0.5E-4, 0.5E-4, 10.0E-4, 10.0E-4, 1E-4, 1E-4, 1E-4, 1E-4, 1E-4, 1E-4, 1E-4, 1E-4 };

    for (Size i = 0; i < n; ++i) {
        // check expectation against analytical calculation (Euler)
        if (std::fabs(mean(e_eu[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (Euler "
                                                                    "discretization, "
                                                                 << mean(e_eu[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu[i]) << " tolerance is "
                                                                 << errTolLd[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (exact "
                                                                    "discretization, "
                                                                 << mean(e_eu2[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu2[i]) << " tolerance is "
                                                                 << errTolLd[i]);
        }
    } 

    // as above, this is a bit rough compared to the more differentiated
    // test of the IR-FX model ...
    Real tol = 10.0E-4;

    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (std::fabs(boost::accumulators::covariance(v_eu[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Euler discretization, "
                            << boost::accumulators::covariance(v_eu[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu[i][j]) << " tolerance is " << tol);
            }
            if (std::fabs(boost::accumulators::covariance(v_eu2[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (exact discretization, "
                            << boost::accumulators::covariance(v_eu2[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu2[i][j]) << " tolerance is " << tol);
            }
        }
    }
    
} // testIrFxInfCrMoments

namespace {

struct IrFxInfCrEqModelTestData {
    IrFxInfCrEqModelTestData()
        : referenceDate(30, July, 2015), eurYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed())),
          usdYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed())),
          gbpYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.04, Actual365Fixed())),
          fxEurUsd(QuantLib::ext::make_shared<SimpleQuote>(0.90)), fxEurGbp(QuantLib::ext::make_shared<SimpleQuote>(1.35)),
          fxEurEur(QuantLib::ext::make_shared<SimpleQuote>(1.00)), infEurAlpha(0.01), infEurKappa(0.01), infGbpAlpha(0.01),
          infGbpKappa(0.01), n1Ts(QuantLib::ext::make_shared<FlatHazardRate>(referenceDate, 0.01, Actual365Fixed())),
          n1Alpha(0.01), n1Kappa(0.01), spSpotToday(QuantLib::ext::make_shared<SimpleQuote>(2100)),
          lhSpotToday(QuantLib::ext::make_shared<SimpleQuote>(12.50)),
          eqDivSp(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed())),
          eqDivLh(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.0075, Actual365Fixed())), c(10, 10, 0.0) {

        std::vector<Date> infDates;
        std::vector<Real> infRates;
        infDates.push_back(Date(30, April, 2015));
        infDates.push_back(Date(30, July, 2015));
        infRates.push_back(0.01);
        infRates.push_back(0.01);
        infEurTs = Handle<ZeroInflationTermStructure>(QuantLib::ext::make_shared<ZeroInflationCurve>(
            referenceDate, TARGET(), Actual365Fixed(), 3 * Months, Monthly, infDates, infRates));
        infGbpTs = Handle<ZeroInflationTermStructure>(QuantLib::ext::make_shared<ZeroInflationCurve>(
            referenceDate, UnitedKingdom(), Actual365Fixed(), 3 * Months, Monthly, infDates, infRates));
        infEurTs->enableExtrapolation();
        infGbpTs->enableExtrapolation();
        // same for eur and gbp (doesn't matter anyway, since we are
        // using flat ts here)
        infLag =
            inflationYearFraction(Monthly, false, Actual365Fixed(), infEurTs->baseDate(), infEurTs->referenceDate());

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

        volsteptimes_a = Array(volstepdates.size());
        volsteptimesFx_a = Array(volstepdatesFx.size());
        eqSpTimes = Array(volstepdatesEqSp.size());
        eqLhTimes = Array(volstepdatesEqLh.size());

        for (Size i = 0; i < volstepdates.size(); ++i) {
            volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
        }
        for (Size i = 0; i < volstepdatesFx.size(); ++i) {
            volsteptimesFx_a[i] = eurYts->timeFromReference(volstepdatesFx[i]);
        }
        for (Size i = 0; i < eqSpTimes.size(); ++i) {
            eqSpTimes[i] = eurYts->timeFromReference(volstepdatesEqSp[i]);
        }
        for (Size i = 0; i < eqLhTimes.size(); ++i) {
            eqLhTimes[i] = eurYts->timeFromReference(volstepdatesEqLh[i]);
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
        for (Size i = 0; i < volstepdatesEqSp.size() + 1; ++i) {
            eqSpVols.push_back(0.20 + (0.35 - 0.20) * std::exp(-0.3 * static_cast<double>(i)));
        }
        for (Size i = 0; i < volstepdatesEqLh.size() + 1; ++i) {
            eqLhVols.push_back(0.25 + (0.45 - 0.25) * std::exp(-0.3 * static_cast<double>(i)));
        }
        eurVols_a = Array(eurVols.begin(), eurVols.end());
        usdVols_a = Array(usdVols.begin(), usdVols.end());
        gbpVols_a = Array(gbpVols.begin(), gbpVols.end());
        fxSigmasUsd_a = Array(fxSigmasUsd.begin(), fxSigmasUsd.end());
        fxSigmasGbp_a = Array(fxSigmasGbp.begin(), fxSigmasGbp.end());
        spSigmas = Array(eqSpVols.begin(), eqSpVols.end());
        lhSigmas = Array(eqLhVols.begin(), eqLhVols.end());

        notimes_a = Array(0);
        eurKappa_a = Array(1, 0.02);
        usdKappa_a = Array(1, 0.03);
        gbpKappa_a = Array(1, 0.04);

        eurLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYts, volsteptimes_a,
                                                                               eurVols_a, notimes_a, eurKappa_a);
        usdLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(USDCurrency(), usdYts, volsteptimes_a,
                                                                               usdVols_a, notimes_a, usdKappa_a);
        gbpLgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(GBPCurrency(), gbpYts, volsteptimes_a,
                                                                               gbpVols_a, notimes_a, gbpKappa_a);

        fxUsd_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), fxEurUsd, volsteptimesFx_a,
                                                                           fxSigmasUsd_a);
        fxGbp_p = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(GBPCurrency(), fxEurGbp, volsteptimesFx_a,
                                                                           fxSigmasGbp_a);

        // inflation
        infEur_p = QuantLib::ext::make_shared<InfDkConstantParametrization>(EURCurrency(), infEurTs, infEurAlpha, infEurKappa);
        infGbp_p = QuantLib::ext::make_shared<InfDkConstantParametrization>(GBPCurrency(), infGbpTs, infGbpAlpha, infGbpKappa);

        // credit
        n1_p = QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(EURCurrency(), n1Ts, n1Alpha, n1Kappa);

        // equity
        QuantLib::ext::shared_ptr<EqBsParametrization> eqSpBsParam = QuantLib::ext::make_shared<EqBsPiecewiseConstantParametrization>(
            USDCurrency(), "SP", spSpotToday, fxEurUsd, eqSpTimes, spSigmas, usdYts, eqDivSp);

        QuantLib::ext::shared_ptr<EqBsParametrization> eqLhBsParam = QuantLib::ext::make_shared<EqBsPiecewiseConstantParametrization>(
            EURCurrency(), "LH", lhSpotToday, fxEurEur, eqLhTimes, lhSigmas, eurYts, eqDivLh);

        singleModels.push_back(eurLgm_p);
        singleModels.push_back(usdLgm_p);
        singleModels.push_back(gbpLgm_p);
        singleModels.push_back(fxUsd_p);
        singleModels.push_back(fxGbp_p);
        singleModels.push_back(infEur_p);
        singleModels.push_back(infGbp_p);
        singleModels.push_back(n1_p);
        singleModels.push_back(eqSpBsParam);
        singleModels.push_back(eqLhBsParam);

        Real tmp[10][10] = {
            // EUR  USD GBP  FX1  FX2  INF_EUR INF_GBP CR EQ1 EQ2
            { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },          // EUR
            { 0.6, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },          // USD
            { 0.3, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },          // GBP
            { 0.2, 0.2, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },          // FX1
            { 0.3, 0.1, 0.1, 0.3, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 },          // FX2
            { 0.8, 0.2, 0.1, 0.4, 0.2, 1.0, 0.0, 0.0, 0.0, 0.0 },          // INF_EUR
            { 0.6, 0.1, 0.2, 0.2, 0.5, 0.5, 1.0, 0.0, 0.0, 0.0 },          // INF_GBP
            { 0.3, 0.2, 0.1, 0.1, 0.3, 0.4, 0.2, 1.0, 0.0, 0.0 },          // CR
            { 0.1, 0.08, 0.06, 0.04, 0.02, 0.00, -0.02, -0.04, 1.0, 0.0 }, // EQ1
            { 0.14, 0.12, 0.10, 0.08, 0.06, 0.04, 0.02, 0.00, -0.02, 1.0 } // EQ2
        };

        for (Size i = 0; i < 10; ++i) {
            for (Size j = 0; j <= i; ++j) {
                c[i][j] = c[j][i] = tmp[i][j];
            }
        }

        BOOST_TEST_MESSAGE("correlation matrix is\n" << c);

        modelExact = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);
        modelEuler = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, c, SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Euler);
    }

    SavedSettings backup;
    Date referenceDate;
    // ir-fx
    Handle<YieldTermStructure> eurYts, usdYts, gbpYts;
    std::vector<Date> volstepdates, volstepdatesFx;
    Array volsteptimes_a, volsteptimesFx_a;
    std::vector<Real> eurVols, usdVols, gbpVols, fxSigmasUsd, fxSigmasGbp;
    Handle<Quote> fxEurUsd, fxEurGbp, fxEurEur;
    Array eurVols_a, usdVols_a, gbpVols_a, fxSigmasUsd_a, fxSigmasGbp_a;
    Array notimes_a, eurKappa_a, usdKappa_a, gbpKappa_a;
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> eurLgm_p, usdLgm_p, gbpLgm_p;
    QuantLib::ext::shared_ptr<FxBsParametrization> fxUsd_p, fxGbp_p;
    // inf
    Handle<ZeroInflationTermStructure> infEurTs, infGbpTs;
    QuantLib::ext::shared_ptr<InfDkParametrization> infEur_p, infGbp_p;
    Real infEurAlpha, infEurKappa, infGbpAlpha, infGbpKappa;
    Real infLag;
    // cr
    Handle<DefaultProbabilityTermStructure> n1Ts, n2Ts, n3Ts;
    QuantLib::ext::shared_ptr<CrLgm1fParametrization> n1_p, n2_p, n3_p;
    Real n1Alpha, n1Kappa, n2Alpha, n2Kappa, n3Alpha, n3Kappa;
    // eq
    std::vector<Date> volstepdatesEqSp, volstepdatesEqLh;
    std::vector<Real> eqSpVols, eqLhVols;
    Array eqSpTimes, spSigmas, eqLhTimes, lhSigmas;
    Handle<Quote> spSpotToday, lhSpotToday;
    Handle<YieldTermStructure> eqDivSp, eqDivLh;
    // model
    std::vector<QuantLib::ext::shared_ptr<Parametrization> > singleModels;
    Matrix c;
    QuantLib::ext::shared_ptr<CrossAssetModel> modelExact, modelEuler;
}; // IrFxInfCrEqModelTestData

} // anonymous namespace

BOOST_AUTO_TEST_CASE(testIrFxInfCrEqMartingaleProperty) {

    BOOST_TEST_MESSAGE("Testing martingale property in ir-fx-inf-cr-eq model for "
                       "Euler and exact discretizations...");

    IrFxInfCrEqModelTestData d;

    QuantLib::ext::shared_ptr<StochasticProcess> process1 = d.modelExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> process2 = d.modelEuler->stateProcess();

    Size n = 50000;                         // number of paths
    Size seed = 18;                         // rng seed
    Time T = 2.0;                          // maturity of payoff
    Time T2 = 20.0;                         // zerobond maturity
    Size steps = static_cast<Size>(T * 24); // number of steps taken (euler)

    // this can be made more accurate by using LowDiscrepancy instead
    // of PseudoRandom, but we use an error estimator for the check
    LowDiscrepancy::rsg_type sg1 = LowDiscrepancy::make_sequence_generator(process1->factors() * 1, seed);
    LowDiscrepancy::rsg_type sg2 = LowDiscrepancy::make_sequence_generator(process2->factors() * steps, seed);

    TimeGrid grid1(T, 1);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process1)) {
        tmp->resetCache(grid1.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg1(process1, grid1, sg1, false);
    TimeGrid grid2(T, steps);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process2)) {
        tmp->resetCache(grid2.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg2(process2, grid2, sg2, false);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > eurzb1, usdzb1, gbpzb1, infeur1, infgbp1,
        n1eur1, eqsp1, eqlh1;
    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > eurzb2, usdzb2, gbpzb2, infeur2, infgbp2,
        n1eur2, eqsp2, eqlh2;

    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path1 = pg1.next();
        Sample<MultiPath> path2 = pg2.next();
        Size l1 = path1.value[0].length() - 1;
        Size l2 = path2.value[0].length() - 1;
        Real zeur1 = path1.value[0][l1];
        Real zusd1 = path1.value[1][l1];
        Real zgbp1 = path1.value[2][l1];
        Real fxusd1 = std::exp(path1.value[3][l1]);
        Real fxgbp1 = std::exp(path1.value[4][l1]);
        Real infeurz1 = path1.value[5][l1];
        Real infeury1 = path1.value[6][l1];
        Real infgbpz1 = path1.value[7][l1];
        Real infgbpy1 = path1.value[8][l1];
        Real crzn11 = path1.value[9][l1];
        Real cryn11 = path1.value[10][l1];
        Real eq11 = path1.value[11][l1];
        Real eq21 = path1.value[12][l1];
        Real zeur2 = path2.value[0][l2];
        Real zusd2 = path2.value[1][l2];
        Real zgbp2 = path2.value[2][l2];
        Real fxusd2 = std::exp(path2.value[3][l2]);
        Real fxgbp2 = std::exp(path2.value[4][l2]);
        Real infeurz2 = path2.value[5][l2];
        Real infeury2 = path2.value[6][l2];
        Real infgbpz2 = path2.value[7][l2];
        Real infgbpy2 = path2.value[8][l2];
        Real crzn12 = path2.value[9][l2];
        Real cryn12 = path2.value[10][l2];
        Real eq12 = path2.value[11][l2];
        Real eq22 = path2.value[12][l2];

        // EUR zerobond
        eurzb1(d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));
        // USD zerobond
        usdzb1(d.modelExact->discountBond(1, T, T2, zusd1) * fxusd1 / d.modelExact->numeraire(0, T, zeur1));
        // GBP zerobond
        gbpzb1(d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 / d.modelExact->numeraire(0, T, zeur1));
        // EUR CPI indexed bond
        std::pair<Real, Real> sinfeur1 = d.modelExact->infdkI(0, T, T2, infeurz1, infeury1);
        infeur1(sinfeur1.first * sinfeur1.second * d.modelExact->discountBond(0, T, T2, zeur1) /
                d.modelExact->numeraire(0, T, zeur1));
        // GBP CPI indexed bond
        std::pair<Real, Real> sinfgbp1 = d.modelExact->infdkI(1, T, T2, infgbpz1, infgbpy1);
        infgbp1(sinfgbp1.first * sinfgbp1.second * d.modelExact->discountBond(2, T, T2, zgbp1) * fxgbp1 /
                d.modelExact->numeraire(0, T, zeur1));
        // EUR defaultable zerobond
        std::pair<Real, Real> sn11 = d.modelExact->crlgm1fS(0, 0, T, T2, crzn11, cryn11);
        n1eur1(sn11.first * sn11.second * d.modelExact->discountBond(0, T, T2, zeur1) / d.modelExact->numeraire(0, T, zeur1));
        // EQ
        eqsp1(std::exp(eq11) * fxusd1 / d.modelExact->numeraire(0, T, zeur1));
        eqlh1(std::exp(eq21) / d.modelExact->numeraire(0, T, zeur1));

        // EUR zerobond
        eurzb2(d.modelExact->discountBond(0, T, T2, zeur2) / d.modelExact->numeraire(0, T, zeur2));
        // USD zerobond
        usdzb2(d.modelExact->discountBond(1, T, T2, zusd2) * fxusd2 / d.modelExact->numeraire(0, T, zeur2));
        // GBP zerobond
        gbpzb2(d.modelExact->discountBond(2, T, T2, zgbp2) * fxgbp2 / d.modelExact->numeraire(0, T, zeur2));
        // EUR CPI indexed bond
        std::pair<Real, Real> sinfeur2 = d.modelExact->infdkI(0, T, T2, infeurz2, infeury2);
        infeur2(sinfeur2.first * sinfeur2.second * d.modelExact->discountBond(0, T, T2, zeur2) /
                d.modelExact->numeraire(0, T, zeur2));
        // GBP CPI indexed bond
        std::pair<Real, Real> sinfgbp2 = d.modelExact->infdkI(1, T, T2, infgbpz2, infgbpy2);
        infgbp2(sinfgbp2.first * sinfgbp2.second * d.modelExact->discountBond(2, T, T2, zgbp2) * fxgbp2 /
                d.modelExact->numeraire(0, T, zeur2));
        // EUR defaultable zerobond
        std::pair<Real, Real> sn12 = d.modelExact->crlgm1fS(0, 0, T, T2, crzn12, cryn12);
        n1eur2(sn12.first * sn12.second * d.modelExact->discountBond(0, T, T2, zeur2) / d.modelExact->numeraire(0, T, zeur2));
        // EQ
        eqsp2(std::exp(eq12) * fxusd2 / d.modelExact->numeraire(0, T, zeur2));
        eqlh2(std::exp(eq22) / d.modelExact->numeraire(0, T, zeur2));
    }

    BOOST_TEST_MESSAGE("EXACT:");
    BOOST_TEST_MESSAGE("EUR zb = " << mean(eurzb1) << " +- " << error_of<tag::mean>(eurzb1) << " vs analytical "
                                   << d.eurYts->discount(T2));
    BOOST_TEST_MESSAGE("USD zb = " << mean(usdzb1) << " +- " << error_of<tag::mean>(usdzb1) << " vs analytical "
                                   << d.usdYts->discount(T2) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("GBP zb = " << mean(gbpzb1) << " +- " << error_of<tag::mean>(gbpzb1) << " vs analytical "
                                   << d.gbpYts->discount(T2) * d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("IDX zb EUR = " << mean(infeur1) << " +- " << error_of<tag::mean>(infeur1) << " vs analytical "
                                       << d.eurYts->discount(T2) *
                                              std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2));
    BOOST_TEST_MESSAGE("IDX zb GBP = " << mean(infgbp1) << " +- " << error_of<tag::mean>(infgbp1) << " vs analytical "
                                       << d.gbpYts->discount(T2) *
                                              std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) *
                                              d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1eur1) << " +- " << error_of<tag::mean>(n1eur1) << " vs analytical "
                                      << d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("EQSP USD = " << mean(eqsp1) << " +- " << error_of<tag::mean>(eqsp1) << " vs analytical "
                                     << d.spSpotToday->value() * d.eqDivSp->discount(T) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("EQLH EUR = " << mean(eqlh1) << " +- " << error_of<tag::mean>(eqlh1) << " vs analytical "
                                     << d.lhSpotToday->value() * d.eqDivLh->discount(T));

    BOOST_TEST_MESSAGE("\nEULER:");
    BOOST_TEST_MESSAGE("EUR zb = " << mean(eurzb2) << " +- " << error_of<tag::mean>(eurzb2) << " vs analytical "
                                   << d.eurYts->discount(T2));
    BOOST_TEST_MESSAGE("USD zb = " << mean(usdzb2) << " +- " << error_of<tag::mean>(usdzb2) << " vs analytical "
                                   << d.usdYts->discount(T2) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("GBP zb = " << mean(gbpzb2) << " +- " << error_of<tag::mean>(gbpzb2) << " vs analytical "
                                   << d.gbpYts->discount(T2) * d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("IDX zb EUR = " << mean(infeur2) << " +- " << error_of<tag::mean>(infeur2) << " vs analytical "
                                       << d.eurYts->discount(T2) *
                                              std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2));
    BOOST_TEST_MESSAGE("IDX zb GBP = " << mean(infgbp2) << " +- " << error_of<tag::mean>(infgbp2) << " vs analytical "
                                       << d.gbpYts->discount(T2) *
                                              std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) *
                                              d.fxEurGbp->value());
    BOOST_TEST_MESSAGE("N1 zb EUR = " << mean(n1eur2) << " +- " << error_of<tag::mean>(n1eur2) << " vs analytical "
                                      << d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("EQSP USD = " << mean(eqsp2) << " +- " << error_of<tag::mean>(eqsp2) << " vs analytical "
                                     << d.spSpotToday->value() * d.eqDivSp->discount(T) * d.fxEurUsd->value());
    BOOST_TEST_MESSAGE("EQLH EUR = " << mean(eqlh2) << " +- " << error_of<tag::mean>(eqlh2) << " vs analytical "
                                     << d.lhSpotToday->value() * d.eqDivLh->discount(T));

    // a bit higher than for plain zero bond , since we look at indexed zero
    // bonds, too
    Real tol1 = 3.0E-4, tol1r = 0.001; // EXACT
    Real tol2 = 14.0E-4, tol2r = 0.01; // EULER

    Real ev = d.eurYts->discount(T2);
    if (std::abs(mean(eurzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(eurzb1) << ", tolerance " << tol1);
    ev = d.usdYts->discount(T2) * d.fxEurUsd->value();
    if (std::abs(mean(usdzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(usdzb1) << ", tolerance " << tol1);
    ev = d.gbpYts->discount(T2) * d.fxEurGbp->value();
    if (std::abs(mean(gbpzb1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(gbpzb1) << ", tolerance " << tol1);
    ev = d.eurYts->discount(T2) * std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2);
    if (std::abs(mean(infeur1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for idx eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(infeur1) << ", tolerance " << tol1);
    ev = d.gbpYts->discount(T2) * std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) * d.fxEurGbp->value();
    if (std::abs(mean(infgbp1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for idx gbpzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(infgbp1) << ", tolerance " << tol1);
    ev = d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1eur1) - ev) > tol1)
        BOOST_TEST_ERROR("Martingale test failed for def eurzb (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(n1eur1) << ", tolerance " << tol1);
    ev = d.spSpotToday->value() * d.eqDivSp->discount(T) * d.fxEurUsd->value();
    if (std::abs(mean(eqsp1) - ev) / ev > tol1r)
        BOOST_TEST_ERROR("Martingale test failed for eq sp (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(eqsp1) << ", rel tolerance " << tol1r);
    ev = d.lhSpotToday->value() * d.eqDivLh->discount(T);
    if (std::abs(mean(eqlh1) - ev) / ev > tol1r)
        BOOST_TEST_ERROR("Martingale test failed for eq lh (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(eqlh1) << ", rel tolerance " << tol1r);

    ev = d.eurYts->discount(T2);
    if (std::abs(mean(eurzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for eurzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(eurzb2) << ", tolerance " << tol2);
    ev = d.usdYts->discount(T2) * d.fxEurUsd->value();
    if (std::abs(mean(usdzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for usdzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(usdzb2) << ", tolerance " << tol2 * error_of<tag::mean>(usdzb2));
    ev = d.gbpYts->discount(T2) * d.fxEurGbp->value();
    if (std::abs(mean(gbpzb2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for gbpzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(gbpzb2) << ", tolerance " << tol2);
    ev = d.eurYts->discount(T2) * std::pow(1.0 + d.infEurTs->zeroRate(T2 - d.infLag), T2);
    if (std::abs(mean(infeur2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for idx eurzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(infeur2) << ", tolerance " << tol2);
    ev = d.gbpYts->discount(T2) * std::pow(1.0 + d.infGbpTs->zeroRate(T2 - d.infLag), T2) * d.fxEurGbp->value();
    if (std::abs(mean(infgbp2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for idx gbpzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(infgbp2) << ", tolerance " << tol2);
    ev = d.eurYts->discount(T2) * d.n1Ts->survivalProbability(T2);
    if (std::abs(mean(n1eur2) - ev) > tol2)
        BOOST_TEST_ERROR("Martingale test failed for def eurzb (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(n1eur1) << ", tolerance " << tol2);
    ev = d.spSpotToday->value() * d.eqDivSp->discount(T) * d.fxEurUsd->value();
    if (std::abs(mean(eqsp2) - ev) / ev > tol2r)
        BOOST_TEST_ERROR("Martingale test failed for eq sp (Euler discr.),"
                   "expected "
                   << ev << ", got " << mean(eqsp2) << ", rel tolerance " << tol2r);
    ev = d.lhSpotToday->value() * d.eqDivLh->discount(T);
    if (std::abs(mean(eqlh2) - ev) / ev > tol2r)
        BOOST_TEST_ERROR("Martingale test failed for eq lh (exact discr.),"
                   "expected "
                   << ev << ", got " << mean(eqlh2) << ", rel tolerance " << tol2r);

} // testIrFxInfCrEqMartingaleProperty

BOOST_AUTO_TEST_CASE(testIrFxInfCrEqMoments) {

    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization "
                       "in ir-fx-inf-cr-eq model...");

    IrFxInfCrEqModelTestData d;

    const Size n = 13; // d.modelExact->dimension();

    QuantLib::ext::shared_ptr<StochasticProcess> p_exact = d.modelExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> p_euler = d.modelExact->stateProcess();

    Real T = 2.0;                           // horizon at which we compare the moments
    Size steps = static_cast<Size>(T * 10); // number of simulation steps (Euler and exact)
    Size paths = 60000;                     // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);

    Size seed = 18;
    TimeGrid grid(T, steps);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_euler)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorSobolBrownianBridge pgen(p_euler, grid, SobolBrownianGenerator::Diagonal, seed,
                                               SobolRsg::JoeKuoD7);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_exact)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorSobolBrownianBridge pgen2(p_exact, grid, SobolBrownianGenerator::Diagonal, seed,
                                                SobolRsg::JoeKuoD7);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > e_eu[n], e_eu2[n];
    accumulator_set<double, stats<tag::covariance<double, tag::covariate1> > > v_eu[n][n], v_eu2[n][n];

    for (Size i = 0; i < paths; ++i) {
        Sample<MultiPath> path = pgen.next();
        Sample<MultiPath> path2 = pgen2.next();
        for (Size ii = 0; ii < n; ++ii) {
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

    for (Size i = 0; i < n; ++i) {
        BOOST_TEST_MESSAGE("E_" << i << " " << e_an[i] << " " << mean(e_eu[i]) << " " << mean(e_eu2[i]));
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("one step analytical");
    for (Size i = 0; i < n; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << v_an[i][j] << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("euler");
    for (Size i = 0; i < n; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << boost::accumulators::covariance(v_eu[i][j]) << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    BOOST_TEST_MESSAGE("exact");
    for (Size i = 0; i < n; ++i) {
        std::ostringstream tmp;
        for (Size j = 0; j <= i; ++j) {
            tmp << boost::accumulators::covariance(v_eu2[i][j]) << " ";
        }
        BOOST_TEST_MESSAGE(tmp.str());
    }
    BOOST_TEST_MESSAGE("==================");

    Real errTolLd[] = { 0.5E-4, 0.5E-4, 0.5E-4, 10.0E-4, 10.0E-4, 0.7E-4, 0.7E-4,
                        0.7E-4, 0.7E-4, 0.7E-4, 0.7E-4,  10.0E-4, 10.0E-4 };
    Real errTolLdEuler[] = { 0.5E-4, 0.5E-4, 0.5E-4, 10.0E-4, 10.0E-4, 0.7E-4, 0.7E-4,
                             0.7E-4, 0.7E-4, 0.7E-4, 0.7E-4,  40.0E-4, 40.0E-4 };

    for (Size i = 0; i < n; ++i) {
        // check expectation against analytical calculation (Euler)
        if (std::fabs(mean(e_eu[i]) - e_an[i]) > errTolLdEuler[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (Euler "
                                                                    "discretization, "
                                                                 << mean(e_eu[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu[i]) << " tolerance is "
                                                                 << errTolLdEuler[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTolLd[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (exact "
                                                                    "discretization, "
                                                                 << mean(e_eu2[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu2[i]) << " tolerance is "
                                                                 << errTolLd[i]);
        }
    }

    // as above, this is a bit rough compared to the more differentiated
    // test of the IR-FX model ...
    Real tol = 10.0E-4, tolEuler = 65.0E-4;

    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (std::fabs(boost::accumulators::covariance(v_eu[i][j]) - v_an[i][j]) > tolEuler) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Euler discretization, "
                            << boost::accumulators::covariance(v_eu[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu[i][j]) << " tolerance is "
                            << tolEuler);
            }
            if (std::fabs(boost::accumulators::covariance(v_eu2[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (exact discretization, "
                            << boost::accumulators::covariance(v_eu2[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu2[i][j]) << " tolerance is " << tol);
            }
        }
    }

} // testIrFxInfCrEqMoments

namespace {

struct IrFxEqModelTestData {
    IrFxEqModelTestData()
        : referenceDate(30, July, 2015), eurYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed())),
          usdYts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.05, Actual365Fixed())),
          eqDivSp(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed())),
          eqDivLh(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.0075, Actual365Fixed())),
          usdEurSpotToday(QuantLib::ext::make_shared<SimpleQuote>(0.90)), eurEurSpotToday(QuantLib::ext::make_shared<SimpleQuote>(1.0)),
          spSpotToday(QuantLib::ext::make_shared<SimpleQuote>(2100)), lhSpotToday(QuantLib::ext::make_shared<SimpleQuote>(12.50)) {

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

        QuantLib::ext::shared_ptr<IrLgm1fParametrization> eurLgmParam =
            QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYts, alphaTimesEur, alphaEur,
                                                                        kappaTimesEur, kappaEur);

        QuantLib::ext::shared_ptr<IrLgm1fParametrization> usdLgmParam =
            QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(USDCurrency(), usdYts, alphaTimesUsd, alphaUsd,
                                                                        kappaTimesUsd, kappaUsd);

        QuantLib::ext::shared_ptr<FxBsParametrization> fxUsdEurBsParam =
            QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(USDCurrency(), usdEurSpotToday, fxTimes, fxSigmas);

        QuantLib::ext::shared_ptr<EqBsParametrization> eqSpBsParam = QuantLib::ext::make_shared<EqBsPiecewiseConstantParametrization>(
            USDCurrency(), "SP", spSpotToday, usdEurSpotToday, eqSpTimes, spSigmas, usdYts, eqDivSp);

        QuantLib::ext::shared_ptr<EqBsParametrization> eqLhBsParam = QuantLib::ext::make_shared<EqBsPiecewiseConstantParametrization>(
            EURCurrency(), "LH", lhSpotToday, eurEurSpotToday, eqLhTimes, lhSigmas, eurYts, eqDivLh);

        singleModels.resize(0);
        singleModels.push_back(eurLgmParam);
        singleModels.push_back(usdLgmParam);
        singleModels.push_back(fxUsdEurBsParam);
        singleModels.push_back(eqSpBsParam);
        singleModels.push_back(eqLhBsParam);

        ccLgmEuler = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, Matrix(), SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);
        ccLgmExact = QuantLib::ext::make_shared<CrossAssetModel>(singleModels, Matrix(), SalvagingAlgorithm::None,
                                                         IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);

        eurIdx = ccLgmEuler->ccyIndex(EURCurrency());
        usdIdx = ccLgmEuler->ccyIndex(USDCurrency());
        eurUsdIdx = usdIdx - 1;
        eqSpIdx = ccLgmEuler->eqIndex("SP");
        eqLhIdx = ccLgmEuler->eqIndex("LH");

        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::IR, eurIdx, CrossAssetModel::AssetType::IR, usdIdx, -0.2);
        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::IR, eurIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, 0.8);
        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::IR, usdIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, -0.5);
        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::EQ, eqSpIdx, CrossAssetModel::AssetType::EQ, eqLhIdx, 0.6);
        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::EQ, eqSpIdx, CrossAssetModel::AssetType::IR, usdIdx, -0.1);
        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::EQ, eqLhIdx, CrossAssetModel::AssetType::IR, eurIdx, -0.05);
        ccLgmEuler->setCorrelation(CrossAssetModel::AssetType::EQ, eqSpIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, 0.1);

        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::IR, eurIdx, CrossAssetModel::AssetType::IR, usdIdx, -0.2);
        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::IR, eurIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, 0.8);
        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::IR, usdIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, -0.5);
        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::EQ, eqSpIdx, CrossAssetModel::AssetType::EQ, eqLhIdx, 0.6);
        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::EQ, eqSpIdx, CrossAssetModel::AssetType::IR, usdIdx, -0.1);
        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::EQ, eqLhIdx, CrossAssetModel::AssetType::IR, eurIdx, -0.05);
        ccLgmExact->setCorrelation(CrossAssetModel::AssetType::EQ, eqSpIdx, CrossAssetModel::AssetType::FX, eurUsdIdx, 0.1);
    }

    SavedSettings backup;
    Date referenceDate;
    Handle<YieldTermStructure> eurYts, usdYts;
    Handle<YieldTermStructure> eqDivSp, eqDivLh;
    Handle<Quote> usdEurSpotToday, eurEurSpotToday, spSpotToday, lhSpotToday;
    std::vector<QuantLib::ext::shared_ptr<Parametrization> > singleModels;
    QuantLib::ext::shared_ptr<CrossAssetModel> ccLgmExact, ccLgmEuler;
    Size eurIdx, usdIdx, eurUsdIdx, eqSpIdx, eqLhIdx;
    std::vector<Date> volstepdatesEqSp, volstepdatesEqLh;
}; // IrFxEqModelTestData

} // anonymous namespace

BOOST_AUTO_TEST_CASE(testEqLgm5fPayouts) {

    BOOST_TEST_MESSAGE("Testing pricing of equity payouts under domestic "
                       "measure in CrossAsset LGM model...");

    IrFxEqModelTestData d;
    Settings::instance().evaluationDate() = d.referenceDate;

    QuantLib::ext::shared_ptr<StochasticProcess> process = d.ccLgmExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> process2 = d.ccLgmEuler->stateProcess();

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

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGeneratorMersenneTwister pg(process, grid, seed, false);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process2)) {
        tmp->resetCache(grid_euler.size() - 1);
    }
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
        Real ccnum = d.ccLgmExact->numeraire(0, T, zeur);

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

    QuantLib::ext::shared_ptr<EquityForward> lhFwdTrade =
        QuantLib::ext::make_shared<EquityForward>("LH", EURCurrency(), Position::Long, 1, tradeMaturity, strikeLh);
    QuantLib::ext::shared_ptr<EquityForward> spFwdTrade =
        QuantLib::ext::make_shared<EquityForward>("SP", USDCurrency(), Position::Long, 1, tradeMaturity, strikeSp);

    QuantLib::ext::shared_ptr<VanillaOption> lhCall =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, strikeLh),
                                          QuantLib::ext::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));
    QuantLib::ext::shared_ptr<VanillaOption> lhPut =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, strikeLh),
                                          QuantLib::ext::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));
    QuantLib::ext::shared_ptr<VanillaOption> spCall =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, strikeSp),
                                          QuantLib::ext::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));
    QuantLib::ext::shared_ptr<VanillaOption> spPut =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, strikeSp),
                                          QuantLib::ext::make_shared<EuropeanExercise>(d.referenceDate + 5 * 365));

    QuantLib::ext::shared_ptr<DiscountingEquityForwardEngine> lhFwdEngine =
        QuantLib::ext::make_shared<DiscountingEquityForwardEngine>(d.eurYts, d.eqDivLh, d.lhSpotToday, d.eurYts);
    QuantLib::ext::shared_ptr<DiscountingEquityForwardEngine> spFwdEngine =
        QuantLib::ext::make_shared<DiscountingEquityForwardEngine>(d.usdYts, d.eqDivSp, d.spSpotToday, d.usdYts);

    lhFwdTrade->setPricingEngine(lhFwdEngine);
    spFwdTrade->setPricingEngine(spFwdEngine);

    QuantLib::ext::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> spEqOptionEngine =
        QuantLib::ext::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgmExact, d.eqSpIdx, d.ccLgmExact->ccyIndex(d.ccLgmExact->eqbs(d.eqSpIdx)->currency()));
    QuantLib::ext::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> lhEqOptionEngine =
        QuantLib::ext::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgmExact, d.eqLhIdx, d.ccLgmExact->ccyIndex(d.ccLgmExact->eqbs(d.eqLhIdx)->currency()));

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

BOOST_AUTO_TEST_CASE(testEqLgm5fCalibration) {

    BOOST_TEST_MESSAGE("Testing EQ calibration of IR-FX-EQ LGM 5F model...");

    IrFxEqModelTestData d;
    Settings::instance().evaluationDate() = d.referenceDate;

    // calibration baskets
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > basketSp, basketLh;

    for (Size i = 0; i < d.volstepdatesEqSp.size(); ++i) {
        Date tmp = i < d.volstepdatesEqSp.size() ? d.volstepdatesEqSp[i] : d.volstepdatesEqSp.back() + 365;
        basketSp.push_back(QuantLib::ext::make_shared<FxEqOptionHelper>(
            tmp, Null<Real>(), d.spSpotToday, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.20)), d.usdYts, d.eqDivSp,
            BlackCalibrationHelper::RelativePriceError));
    }
    for (Size i = 0; i < d.volstepdatesEqLh.size(); ++i) {
        Date tmp = i < d.volstepdatesEqLh.size() ? d.volstepdatesEqLh[i] : d.volstepdatesEqLh.back() + 365;
        basketLh.push_back(QuantLib::ext::make_shared<FxEqOptionHelper>(
            tmp, Null<Real>(), d.lhSpotToday, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.20)), d.eurYts, d.eqDivLh,
            BlackCalibrationHelper::RelativePriceError));
    }

    // pricing engines
    QuantLib::ext::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> spEqOptionEngine =
        QuantLib::ext::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgmExact, d.eqSpIdx, d.ccLgmExact->ccyIndex(d.ccLgmExact->eqbs(d.eqSpIdx)->currency()));
    QuantLib::ext::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> lhEqOptionEngine =
        QuantLib::ext::make_shared<AnalyticXAssetLgmEquityOptionEngine>(
            d.ccLgmExact, d.eqLhIdx, d.ccLgmExact->ccyIndex(d.ccLgmExact->eqbs(d.eqLhIdx)->currency()));

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

    d.ccLgmExact->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::EQ, d.eqSpIdx, basketSp, lm, ec);
    d.ccLgmExact->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::EQ, d.eqLhIdx, basketLh, lm, ec);

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
} // testEqLgm5fCalibration

BOOST_AUTO_TEST_CASE(testEqLgm5fMoments) {

    // TODO : REVIEW TOLERANCES
    BOOST_TEST_MESSAGE("Testing analytic moments vs. Euler and exact discretization "
                       "in IR-FX-EQ LGM 5F model...");

    IrFxEqModelTestData d;
    Settings::instance().evaluationDate() = d.referenceDate;

    QuantLib::ext::shared_ptr<StochasticProcess> p_exact = d.ccLgmExact->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> p_euler = d.ccLgmEuler->stateProcess();

    Real T = 2.0;                                   // horizon at which we compare the moments
    Size steps_euler = static_cast<Size>(T * 50.0); // number of simulation steps
    Size steps_exact = 1;
    Size paths = 25000; // number of paths

    Array e_an = p_exact->expectation(0.0, p_exact->initialValues(), T);
    Matrix v_an = p_exact->covariance(0.0, p_exact->initialValues(), T);
    Matrix v_an_eu = p_euler->covariance(0.0, p_euler->initialValues(), T);

    TimeGrid grid_euler(T, steps_euler);
    TimeGrid grid_exact(T, steps_exact);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_euler)) {
        tmp->resetCache(grid_euler.size() - 1);
    }
    MultiPathGeneratorSobolBrownianBridge pgen(p_euler, grid_euler);

    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_exact)) {
        tmp->resetCache(grid_exact.size() - 1);
    }
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
                        << i << " (" << e_an[i]
                        << ") is inconsistent with numerical value (Euler "
                           "discretization, "
                        << mean(e_eu[i]) << "), error is " << e_an[i] - mean(e_eu[i]) << " tolerance is " << errTol[i]);
        }
        // check expectation against analytical calculation (exact disc)
        if (std::fabs(mean(e_eu2[i]) - e_an[i]) > errTol[i]) {
            BOOST_ERROR("analytical expectation for component #" << i << " (" << e_an[i]
                                                                 << ") is inconsistent with numerical value (Exact "
                                                                    "discretization, "
                                                                 << mean(e_eu2[i]) << "), error is "
                                                                 << e_an[i] - mean(e_eu2[i]) << " tolerance is "
                                                                 << errTol[i]);
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
            if (std::fabs(boost::accumulators::covariance(v_eu[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Euler discretization, "
                            << boost::accumulators::covariance(v_eu[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu[i][j]) << " tolerance is " << tol);
            }
            if (std::fabs(boost::accumulators::covariance(v_eu2[i][j]) - v_an[i][j]) > tol) {
                BOOST_ERROR("analytical covariance at ("
                            << i << "," << j << ") (" << v_an[i][j]
                            << ") is inconsistent with numerical "
                               "value (Exact discretization, "
                            << boost::accumulators::covariance(v_eu2[i][j]) << "), error is "
                            << v_an[i][j] - boost::accumulators::covariance(v_eu2[i][j]) << " tolerance is " << tol);
            }
        }
    }

    BOOST_TEST_MESSAGE("Testing correlation matrix recovery in presence of equity simulation");

    // reset caching so that we can retrieve further info from the processes
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_euler)) {
        tmp->resetCache(0);
    }
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(p_exact)) {
        tmp->resetCache(grid_euler.size() - 1);
    }


    Matrix corr_input = d.ccLgmExact->correlation();
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
            Real cov = boost::accumulators::covariance(v_eu[i][j]);
            BOOST_TEST_MESSAGE(i << ";" << j << ";"
                                 << "EULER;" << v_an[i][j] << ";" << cov);
            Real cov2 = boost::accumulators::covariance(v_eu2[i][j]);
            BOOST_TEST_MESSAGE(i << ";" << j << ";"
                                 << "EXACT;" << v_an[i][j] << ";" << cov2);
        }
    }
} // testEqLgm5fMoments

BOOST_AUTO_TEST_CASE(testCorrelationRecovery) {

    BOOST_TEST_MESSAGE("Test if random correlation input is recovered for "
                       "small dt in Ccy LGM model...");

    class PseudoCurrency : public Currency {
    public:
        PseudoCurrency(const Size id) {
            std::ostringstream ln, sn;
            ln << "Dummy " << id;
            sn << "DUM " << id;
            data_ = QuantLib::ext::make_shared<Data>(ln.str(), sn.str(), id, sn.str(), "", 100, Rounding(), "%3% %1$.2f");
        }
    };

    Real dt = 1.0E-6;
    Real tol = 1.0E-7;

    // for ir-fx this fully specifies the correlation matrix
    // for new asset classes add other possible combinations as well
    Size currencies[] = { 1, 2, 3, 4, 5, 10, 20 };

    MersenneTwisterUniformRng mt(42);

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));

    Handle<Quote> fxspot(QuantLib::ext::make_shared<SimpleQuote>(1.00));

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

        std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;

        // IR
        for (Size i = 0; i < currencies[ii]; ++i) {
            parametrizations.push_back(
                QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(pseudoCcy[i], yts, 0.01, 0.01));
        }
        // FX
        for (Size i = 0; i < currencies[ii] - 1; ++i) {
            parametrizations.push_back(
                QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(pseudoCcy[i + 1], fxspot, notimes, fxsigma));
        }

        QuantLib::ext::shared_ptr<CrossAssetModel> modelExact =
            QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, c, SalvagingAlgorithm::None, IrModel::Measure::LGM,
                                                CrossAssetModel::Discretization::Exact);
        QuantLib::ext::shared_ptr<CrossAssetModel> modelEuler =
            QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, c, SalvagingAlgorithm::None, IrModel::Measure::LGM,
                                                CrossAssetModel::Discretization::Euler);

        QuantLib::ext::shared_ptr<StochasticProcess> peuler = modelExact->stateProcess();
        QuantLib::ext::shared_ptr<StochasticProcess> pexact = modelEuler->stateProcess();

        Matrix c1 = peuler->covariance(dt, peuler->initialValues(), dt);
        Matrix c2 = pexact->covariance(0.0, peuler->initialValues(), dt);

        Matrix r1(dim, dim), r2(dim, dim);

        for (Size i = 0; i < dim; ++i) {
            for (Size j = 0; j <= i; ++j) {
                r1[i][j] = r1[j][i] = c1[i][j] / std::sqrt(c1[i][i] * c1[j][j]);
                r2[i][j] = r2[j][i] = c2[i][j] / std::sqrt(c2[i][i] * c2[j][j]);
                if (std::fabs(r1[i][j] - c[i][j]) > tol) {
                    BOOST_ERROR("failed to recover correlation matrix from "
                                "Euler state process (i,j)=("
                                << i << "," << j << "), input correlation is " << c[i][j] << ", output is " << r1[i][j]
                                << ", difference " << (c[i][j] - r1[i][j]) << ", tolerance " << tol);
                }
                if (std::fabs(r2[i][j] - c[i][j]) > tol) {
                    BOOST_ERROR("failed to recover correlation matrix from "
                                "exact state process (i,j)=("
                                << i << "," << j << "), input correlation is " << c[i][j] << ", output is " << r2[i][j]
                                << ", difference " << (c[i][j] - r2[i][j]) << ", tolerance " << tol);
                }
            }
        }

    } // for currencies

} // test correlation recovery

BOOST_AUTO_TEST_CASE(testIrFxInfCrCorrelationRecovery) {

    BOOST_TEST_MESSAGE("Test if random correlation input is recovered for "
                       "small dt in ir-fx-inf-cr model...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(30, July, 2015);

    class PseudoCurrency : public Currency {
    public:
        PseudoCurrency(const Size id) {
            std::ostringstream ln, sn;
            ln << "Dummy " << id;
            sn << "DUM " << id;
            data_ = QuantLib::ext::make_shared<Data>(ln.str(), sn.str(), id, sn.str(), "", 100, Rounding(), "%3% %1$.2f");
        }
    };

    Real dt = 1.0E-6;
    Real tol = 1.0E-7;

    // for ir-fx this fully specifies the correlation matrix
    // for new asset classes add other possible combinations as well
    Size currencies[] = { 1, 2, 3, 4, 5, 10, 20 };
    Size cpiindexes[] = { 0, 1, 10 };
    Size creditnames[] = { 0, 1, 5 };

    MersenneTwisterUniformRng mt(42);

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));

    std::vector<Date> infDates;
    std::vector<Real> infRates;
    infDates.push_back(Date(30, April, 2015));
    infDates.push_back(Date(30, July, 2015));
    infRates.push_back(0.01);
    infRates.push_back(0.01);
    Handle<ZeroInflationTermStructure> its(
        QuantLib::ext::make_shared<ZeroInflationCurve>(Settings::instance().evaluationDate(), NullCalendar(), Actual365Fixed(),
                                               3 * Months, Monthly, infDates, infRates));

    Handle<DefaultProbabilityTermStructure> hts(
        QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.01, Actual365Fixed()));

    Handle<Quote> fxspot(QuantLib::ext::make_shared<SimpleQuote>(1.00));

    Array notimes(0);
    Array fxsigma(1, 0.10);

    for (Size ii = 0; ii < LENGTH(currencies); ++ii) {
        for (Size kk = 0; kk < LENGTH(cpiindexes); ++kk) {
            for (Size jj = 0; jj < LENGTH(creditnames); ++jj) {

                std::vector<Currency> pseudoCcy;
                for (Size i = 0; i < currencies[ii]; ++i) {
                    PseudoCurrency tmp(i);
                    pseudoCcy.push_back(tmp);
                }

                Size dim = 2 * currencies[ii] - 1 + cpiindexes[kk] + creditnames[jj];

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

                std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;

                // IR
                for (Size i = 0; i < currencies[ii]; ++i) {
                    parametrizations.push_back(
                        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(pseudoCcy[i], yts, 0.01, 0.01));
                }
                // FX
                for (Size i = 0; i < currencies[ii] - 1; ++i) {
                    parametrizations.push_back(QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(
                        pseudoCcy[i + 1], fxspot, notimes, fxsigma));
                }
                // INF
                for (Size i = 0; i < cpiindexes[kk]; ++i) {
                    parametrizations.push_back(
                        QuantLib::ext::make_shared<InfDkConstantParametrization>(pseudoCcy[0], its, 0.01, 0.01));
                }
                // CR
                for (Size i = 0; i < creditnames[jj]; ++i) {
                    parametrizations.push_back(
                        QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(pseudoCcy[0], hts, 0.01, 0.01));
                }

                // get QuantLib::Error: negative eigenvalue(s) (-3.649315e-16) with SalvagingAlgorithm::None
                QuantLib::ext::shared_ptr<CrossAssetModel> modelEuler =
                    QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, c, SalvagingAlgorithm::Spectral,
                                                        IrModel::Measure::LGM, CrossAssetModel::Discretization::Euler);
                QuantLib::ext::shared_ptr<CrossAssetModel> modelExact =
                    QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, c, SalvagingAlgorithm::Spectral,
                                                        IrModel::Measure::LGM, CrossAssetModel::Discretization::Exact);

                QuantLib::ext::shared_ptr<StochasticProcess> peuler = modelEuler->stateProcess();
                QuantLib::ext::shared_ptr<StochasticProcess> pexact = modelExact->stateProcess();

                Matrix c1 = peuler->covariance(dt, peuler->initialValues(), dt);
                Matrix c2 = pexact->covariance(0.0, peuler->initialValues(), dt);

                Matrix r1(dim, dim), r2(dim, dim);

                for (Size i = 0; i < dim; ++i) {
                    for (Size j = 0; j <= i; ++j) {
                        // there are two state variables per credit name,
                        // and per inflation index
                        Size subi = i < 2 * currencies[ii] - 1 ? 1 : 2;
                        Size subj = j < 2 * currencies[ii] - 1 ? 1 : 2;
                        for (Size k1 = 0; k1 < subi; ++k1) {
                            for (Size k2 = 0; k2 < subj; ++k2) {
                                Size i0 = i < 2 * currencies[ii] - 1
                                              ? i
                                              : 2 * currencies[ii] - 1 + 2 * (i - (2 * currencies[ii] - 1)) + k1;
                                Size j0 = j < 2 * currencies[ii] - 1
                                              ? j
                                              : 2 * currencies[ii] - 1 + 2 * (j - (2 * currencies[ii] - 1)) + k2;
                                r1[i][j] = r1[j][i] = c1[i0][j0] / std::sqrt(c1[i0][i0] * c1[j0][j0]);
                                r2[i][j] = r2[j][i] = c2[i0][j0] / std::sqrt(c2[i0][i0] * c2[j0][j0]);
                                if (std::fabs(r1[i][j] - c[i][j]) > tol) {
                                    BOOST_ERROR("failed to recover correlation matrix "
                                                "from "
                                                "Euler state process (i,j)=("
                                                << i << "," << j << "), (i0,j0)=(" << i0 << "," << j0
                                                << "), input correlation is " << c[i][j] << ", output is " << r1[i][j]
                                                << ", difference " << (c[i][j] - r1[i][j]) << ", tolerance " << tol
                                                << " test configuration is " << currencies[ii] << " currencies and "
                                                << cpiindexes[kk] << " cpi indexes and " << creditnames[jj]
                                                << " credit names");
                                }
                                if (k1 == k2) {
                                    if (std::fabs(r2[i][j] - c[i][j]) > tol) {
                                        BOOST_ERROR("failed to recover correlation "
                                                    "matrix "
                                                    "from "
                                                    "exact state process (i,j)=("
                                                    << i << "," << j << "), (i0,j0)=(" << i0 << "," << j0
                                                    << "), input correlation is " << c[i][j] << ", output is "
                                                    << r2[i][j] << ", difference " << (c[i][j] - r2[i][j])
                                                    << ", tolerance " << tol << " test configuration is "
                                                    << currencies[ii] << " currencies and " << cpiindexes[kk]
                                                    << " cpi indexes and " << creditnames[jj] << " credit names");
                                    }
                                }
                            }
                        }
                    }
                }
            } // for creditnames
        }     // for cpiindexes
    }         // for currencies
} // testIrFxInfCrCorrelationRecovery

BOOST_AUTO_TEST_CASE(testIrFxInfCrEqCorrelationRecovery) {

    BOOST_TEST_MESSAGE("Test if random correlation input is recovered for "
                       "small dt in ir-fx-inf-cr-eq model...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(30, July, 2015);

    class PseudoCurrency : public Currency {
    public:
        PseudoCurrency(const Size id) {
            std::ostringstream ln, sn;
            ln << "Dummy " << id;
            sn << "DUM " << id;
            data_ = QuantLib::ext::make_shared<Data>(ln.str(), sn.str(), id, sn.str(), "", 100, Rounding(), "%3% %1$.2f");
        }
    };

    Real dt = 1.0E-6;
    Real tol = 1.0E-7;

    // for ir-fx this fully specifies the correlation matrix
    // for new asset classes add other possible combinations as well
    Size currencies[] = { 1, 2, 3, 4, 5 };
    Size cpiindexes[] = { 0, 1, 10 };
    Size creditnames[] = { 0, 1, 5 };
    Size eqs[] = { 0, 1, 5 };

    MersenneTwisterUniformRng mt(42);

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));

    std::vector<Date> infDates;
    std::vector<Real> infRates;
    infDates.push_back(Date(30, April, 2015));
    infDates.push_back(Date(30, July, 2015));
    infRates.push_back(0.01);
    infRates.push_back(0.01);
    Handle<ZeroInflationTermStructure> its(
        QuantLib::ext::make_shared<ZeroInflationCurve>(Settings::instance().evaluationDate(), NullCalendar(), Actual365Fixed(),
                                               3 * Months, Monthly, infDates, infRates));

    Handle<DefaultProbabilityTermStructure> hts(
        QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.01, Actual365Fixed()));

    Handle<Quote> fxspot(QuantLib::ext::make_shared<SimpleQuote>(1.00)), eqspot(QuantLib::ext::make_shared<SimpleQuote>(1.00));

    Array notimes(0);
    Array fxsigma(1, 0.10), eqsigma(1, 0.10);

    for (Size ii = 0; ii < LENGTH(currencies); ++ii) {
        for (Size kk = 0; kk < LENGTH(cpiindexes); ++kk) {
            for (Size jj = 0; jj < LENGTH(creditnames); ++jj) {
                for (Size ee = 0; ee < LENGTH(eqs); ++ee) {

                    std::vector<Currency> pseudoCcy;
                    for (Size i = 0; i < currencies[ii]; ++i) {
                        PseudoCurrency tmp(i);
                        pseudoCcy.push_back(tmp);
                    }

                    Size dim = 2 * currencies[ii] - 1 + cpiindexes[kk] + creditnames[jj] + eqs[ee];

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

                    std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;

                    // IR
                    for (Size i = 0; i < currencies[ii]; ++i) {
                        parametrizations.push_back(
                            QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(pseudoCcy[i], yts, 0.01, 0.01));
                    }
                    // FX
                    for (Size i = 0; i < currencies[ii] - 1; ++i) {
                        parametrizations.push_back(QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(
                            pseudoCcy[i + 1], fxspot, notimes, fxsigma));
                    }
                    // INF
                    for (Size i = 0; i < cpiindexes[kk]; ++i) {
                        parametrizations.push_back(
                            QuantLib::ext::make_shared<InfDkConstantParametrization>(pseudoCcy[0], its, 0.01, 0.01));
                    }
                    // CR
                    for (Size i = 0; i < creditnames[jj]; ++i) {
                        parametrizations.push_back(
                            QuantLib::ext::make_shared<CrLgm1fConstantParametrization>(pseudoCcy[0], hts, 0.01, 0.01));
                    }
                    // EQ
                    for (Size i = 0; i < eqs[ee]; ++i) {
                        parametrizations.push_back(QuantLib::ext::make_shared<EqBsPiecewiseConstantParametrization>(
                            pseudoCcy[0], "dummy", eqspot, fxspot, notimes, eqsigma, yts, yts));
                    }

                    // get QuantLib::Error: negative eigenvalue(s) (-3.649315e-16) with SalvagingAlgorithm::None
                    QuantLib::ext::shared_ptr<CrossAssetModel> modelEuler = QuantLib::ext::make_shared<CrossAssetModel>(
                        parametrizations, c, SalvagingAlgorithm::Spectral, IrModel::Measure::LGM,
                        CrossAssetModel::Discretization::Euler);
                    QuantLib::ext::shared_ptr<CrossAssetModel> modelExact = QuantLib::ext::make_shared<CrossAssetModel>(
                        parametrizations, c, SalvagingAlgorithm::Spectral, IrModel::Measure::LGM,
                        CrossAssetModel::Discretization::Exact);

                    QuantLib::ext::shared_ptr<StochasticProcess> peuler = modelEuler->stateProcess();
                    QuantLib::ext::shared_ptr<StochasticProcess> pexact = modelExact->stateProcess();

                    Matrix c1 = peuler->covariance(dt, peuler->initialValues(), dt);
                    Matrix c2 = pexact->covariance(0.0, peuler->initialValues(), dt);

                    Matrix r1(dim, dim), r2(dim, dim);

                    Size sizeIrFx = 2 * currencies[ii] - 1;

                    for (Size i = 0; i < dim; ++i) {
                        for (Size j = 0; j <= i; ++j) {
                            // there are two state variables per credit name,
                            // and per inflation index
                            Size subi = i < sizeIrFx || i >= sizeIrFx + cpiindexes[kk] + creditnames[jj] ? 1 : 2;
                            Size subj = j < sizeIrFx || i >= sizeIrFx + cpiindexes[kk] + creditnames[jj] ? 1 : 2;
                            for (Size k1 = 0; k1 < subi; ++k1) {
                                for (Size k2 = 0; k2 < subj; ++k2) {
                                    Size i0 = i < sizeIrFx
                                                  ? i
                                                  : (i < sizeIrFx + cpiindexes[kk] + creditnames[jj]
                                                         ? sizeIrFx + 2 * (i - sizeIrFx) + k1
                                                         : sizeIrFx + 2 * cpiindexes[kk] + 2 * creditnames[jj] +
                                                               (i - sizeIrFx - cpiindexes[kk] - creditnames[jj]));
                                    Size j0 = j < sizeIrFx
                                                  ? j
                                                  : (j < sizeIrFx + cpiindexes[kk] + creditnames[jj]
                                                         ? sizeIrFx + 2 * (j - sizeIrFx) + k2
                                                         : sizeIrFx + 2 * cpiindexes[kk] + 2 * creditnames[jj] +
                                                               (j - sizeIrFx - cpiindexes[kk] - creditnames[jj]));
                                    r1[i][j] = r1[j][i] = c1[i0][j0] / std::sqrt(c1[i0][i0] * c1[j0][j0]);
                                    r2[i][j] = r2[j][i] = c2[i0][j0] / std::sqrt(c2[i0][i0] * c2[j0][j0]);
                                    if (std::fabs(r1[i][j] - c[i][j]) > tol) {
                                        BOOST_ERROR("failed to recover correlation matrix "
                                                    "from "
                                                    "Euler state process (i,j)=("
                                                    << i << "," << j << "), (i0,j0)=(" << i0 << "," << j0
                                                    << "), input correlation is " << c[i][j] << ", output is "
                                                    << r1[i][j] << ", difference " << (c[i][j] - r1[i][j])
                                                    << ", tolerance " << tol << " test configuration is "
                                                    << currencies[ii] << " currencies and " << cpiindexes[kk]
                                                    << " cpi indexes and " << creditnames[jj] << " credit names"
                                                    << " and " << eqs[ee] << " equities");
                                    }
                                    if (k1 == k2) {
                                        if (std::fabs(r2[i][j] - c[i][j]) > tol) {
                                            BOOST_ERROR("failed to recover correlation "
                                                        "matrix "
                                                        "from "
                                                        "exact state process (i,j)=("
                                                        << i << "," << j << "), (i0,j0)=(" << i0 << "," << j0
                                                        << "), input correlation is " << c[i][j] << ", output is "
                                                        << r2[i][j] << ", difference " << (c[i][j] - r2[i][j])
                                                        << ", tolerance " << tol << " test configuration is "
                                                        << currencies[ii] << " currencies and " << cpiindexes[kk]
                                                        << " cpi indexes and " << creditnames[jj] << " credit names"
                                                        << " and " << eqs[ee] << " equities");
                                        }
                                    }
                                }
                            }
                        }
                    }
                } // for equity
            }     // for creditnames
        }         // for cpiindexes
    }             // for currencies
} // testIrFxInfCrEqCorrelationRecovery

BOOST_AUTO_TEST_CASE(testCpiCalibrationByAlpha) {

    BOOST_TEST_MESSAGE("Testing calibration to ZC CPI Floors (using alpha) and repricing via MC...");

    // set up IR-INF model, calibrate to given premiums and check
    // the result with a MC simulation

    SavedSettings backup;
    Date refDate(30, July, 2015);
    Settings::instance().evaluationDate() = Date(30, July, 2015);

    // IR
    Handle<YieldTermStructure> eurYts(QuantLib::ext::make_shared<FlatForward>(refDate, 0.01, Actual365Fixed()));
    QuantLib::ext::shared_ptr<Parametrization> ireur_p =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), eurYts, 0.01, 0.01);

    // INF
    Real baseCPI = 100.0;
    std::vector<Date> infDates;
    std::vector<Real> infRates;
    infDates.push_back(Date(30, April, 2015));
    infDates.push_back(Date(30, July, 2015));
    infRates.push_back(0.0075);
    infRates.push_back(0.0075);
    Handle<ZeroInflationTermStructure> infEurTs(QuantLib::ext::make_shared<ZeroInflationCurve>(
        refDate, TARGET(), Actual365Fixed(), 3 * Months, Monthly, infDates, infRates));
    infEurTs->enableExtrapolation();
    Handle<ZeroInflationIndex> infIndex(QuantLib::ext::make_shared<EUHICPXT>(false, infEurTs));
    
    infIndex->addFixing(Date(1, April, 2015), 100);

    Real premium[] = { 0.0044, 0.0085, 0.0127, 0.0160, 0.0186 };

    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > cpiHelpers;
    Array volStepTimes(4), noTimes(0);
    Array infVols(5, 0.01), infRev(1, 1.5); // !!

    Time T;
    for (Size i = 1; i <= 5; ++i) {
        Date maturity = refDate + i * Years;
        QuantLib::ext::shared_ptr<CpiCapFloorHelper> h(new CpiCapFloorHelper(Option::Put, baseCPI, maturity, TARGET(),
                                                                     ModifiedFollowing, TARGET(), ModifiedFollowing,
                                                                     0.01, infIndex, 3 * Months, premium[i - 1]));
        Real t = inflationYearFraction(Monthly, false, Actual365Fixed(), infEurTs->baseDate(),
                                       h->instrument()->fixingDate());
        cpiHelpers.push_back(h);
        if (i <= 4)
            volStepTimes[i - 1] = t;
        T = t;
    }

    QuantLib::ext::shared_ptr<InfDkPiecewiseConstantParametrization> infeur_p =
        QuantLib::ext::make_shared<InfDkPiecewiseConstantParametrization>(EURCurrency(), infEurTs, volStepTimes, infVols,
                                                                  noTimes, infRev);

    std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;
    parametrizations.push_back(ireur_p);
    parametrizations.push_back(infeur_p);

    QuantLib::ext::shared_ptr<CrossAssetModel> model =
        QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, Matrix(), SalvagingAlgorithm::None);

    model->setCorrelation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::INF, 0, 0.33);

    // pricing engine
    QuantLib::ext::shared_ptr<AnalyticDkCpiCapFloorEngine> engine =
        QuantLib::ext::make_shared<AnalyticDkCpiCapFloorEngine>(model, 0, baseCPI);

    for (Size i = 0; i < cpiHelpers.size(); ++i) {
        cpiHelpers[i]->setPricingEngine(engine);
    }

    // calibration
    LevenbergMarquardt lm;
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);
    model->calibrateInfDkVolatilitiesIterative(0, cpiHelpers, lm, ec);

    for (Size i = 0; i < cpiHelpers.size(); ++i) {
        BOOST_TEST_MESSAGE("i=" << i << " modelvol=" << model->infdk(0)->parameterValues(0)[i] << " market="
                                << cpiHelpers[i]->marketValue() << " model=" << cpiHelpers[i]->modelValue()
                                << " diff=" << (cpiHelpers[i]->marketValue() - cpiHelpers[i]->modelValue()));
    }

    // reprice last ZC floor with Monte Carlo
    Size n = 50000; // number of paths
    Size seed = 18; // rng seed
    Size steps = 1; // number of discretization steps

    QuantLib::ext::shared_ptr<StochasticProcess> process = model->stateProcess();
    LowDiscrepancy::rsg_type sg = LowDiscrepancy::make_sequence_generator(process->factors() * steps, seed);
    TimeGrid grid(T, steps);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg(process, grid, sg, false);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > floor;

    Real K = std::pow(1.0 + 0.01, T);

    for (Size i = 0; i < n; ++i) {
        Sample<MultiPath> path = pg.next();
        Size l = path.value[0].length() - 1;
        Real irz = path.value[0][l];
        Real infz = path.value[1][l];
        Real infy = path.value[2][l];
        Real I = model->infdkI(0, T, T, infz, infy).first;
        floor(std::max(-(I - K), 0.0) / model->numeraire(0, T, irz));
    }

    BOOST_TEST_MESSAGE("mc floor 5y = " << mean(floor) << " +- ");

    // check model calibration
    Real tol = 1.0E-12;
    for (Size i = 0; i < cpiHelpers.size(); ++i) {
        if (std::abs(cpiHelpers[i]->modelValue() - cpiHelpers[i]->marketValue()) > tol) {
            BOOST_ERROR("Model calibration for ZC CPI Floor #"
                        << i << " failed, market premium is " << cpiHelpers[i]->marketValue() << ", model value is "
                        << cpiHelpers[i]->modelValue() << ", difference is "
                        << (cpiHelpers[i]->marketValue() - cpiHelpers[i]->modelValue()) << ", tolerance is " << tol);
        }
    }
    // check repricing with MC
    tol = 1.0E-5;
    Real mcPrice = mean(floor);
    if (std::abs(mcPrice - cpiHelpers[4]->modelValue()) > tol) {
        BOOST_ERROR("Failed to reprice 5y ZC CPI Floor with MC ("
                    << mcPrice << "), analytical model price is " << cpiHelpers[4]->modelValue() << ", difference is "
                    << mcPrice - cpiHelpers[4]->modelValue() << ", tolerance is " << tol);
    }
} // testCpiCalibrationByAlpha

BOOST_AUTO_TEST_CASE(testCpiCalibrationByH) {

    BOOST_TEST_MESSAGE("Testing calibration to ZC CPI Floors (using H) and repricing via MC...");

    // set up IR-INF model, calibrate to given premiums and check
    // the result with a MC simulation

    SavedSettings backup;
    Date refDate(30, July, 2015);
    Settings::instance().evaluationDate() = Date(30, July, 2015);

    // IR
    Handle<YieldTermStructure> eurYts(QuantLib::ext::make_shared<FlatForward>(refDate, 0.01, Actual365Fixed()));
    QuantLib::ext::shared_ptr<Parametrization> ireur_p =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), eurYts, 0.01, 0.01);

    // INF
    Real baseCPI = 100.0;
    std::vector<Date> infDates;
    std::vector<Real> infRates;
    infDates.push_back(Date(30, April, 2015));
    infDates.push_back(Date(30, July, 2015));
    infRates.push_back(0.0075);
    infRates.push_back(0.0075);
    Handle<ZeroInflationTermStructure> infEurTs(QuantLib::ext::make_shared<ZeroInflationCurve>(
        refDate, TARGET(), Actual365Fixed(), 3 * Months, Monthly, infDates, infRates));
    infEurTs->enableExtrapolation();
    Handle<ZeroInflationIndex> infIndex(QuantLib::ext::make_shared<EUHICPXT>(false, infEurTs));
    infIndex->addFixing(Date(1, April, 2015), 100);
    Size nMat = 14;
    Real premium[] = { 0.000555, 0.000813, 0.000928, 0.00127, 0.001616, 0.0019, 0.0023,
                       0.0026,   0.0029,   0.0032,   0.0032,  0.0033,   0.0038, 0.0067 };
    Period maturity[] = { 1 * Years, 2 * Years, 3 * Years,  4 * Years,  5 * Years,  6 * Years,  7 * Years,
                          8 * Years, 9 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years, 30 * Years };

    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > cpiHelpers;
    Array volStepTimes(13), noTimes(0);
    Array infVols(14, 0.0030), infRev(14, 1.0); // init vol and rev !!
    Real strike = 0.00;                         // strike !!

    Time T = Null<Time>();
    for (Size i = 1; i <= nMat; ++i) {
        Date mat = refDate + maturity[i - 1];
        QuantLib::ext::shared_ptr<CpiCapFloorHelper> h(new CpiCapFloorHelper(Option::Put, baseCPI, mat, TARGET(),
                                                                     ModifiedFollowing, TARGET(), ModifiedFollowing,
                                                                     strike, infIndex, 3 * Months, premium[i - 1]));
        Real t = inflationYearFraction(Monthly, false, Actual365Fixed(), infEurTs->baseDate(),
                                       h->instrument()->fixingDate());
        cpiHelpers.push_back(h);
        if (i <= nMat - 1)
            volStepTimes[i - 1] = t;
        T = t;
    }

    QuantLib::ext::shared_ptr<InfDkPiecewiseLinearParametrization> infeur_p =
        QuantLib::ext::make_shared<InfDkPiecewiseLinearParametrization>(EURCurrency(), infEurTs, volStepTimes, infVols,
                                                                volStepTimes, infRev);

    std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;
    parametrizations.push_back(ireur_p);
    parametrizations.push_back(infeur_p);

    QuantLib::ext::shared_ptr<CrossAssetModel> model =
        QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, Matrix(), SalvagingAlgorithm::None);

    model->setCorrelation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::INF, 0, 0.33);

    // pricing engine
    QuantLib::ext::shared_ptr<AnalyticDkCpiCapFloorEngine> engine =
        QuantLib::ext::make_shared<AnalyticDkCpiCapFloorEngine>(model, 0, baseCPI);

    for (Size i = 0; i < cpiHelpers.size(); ++i) {
        cpiHelpers[i]->setPricingEngine(engine);
    }

    // calibration
    LevenbergMarquardt lm;
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);
    // model->calibrateInfDkVolatilitiesIterative(0, cpiHelpers, lm, ec);
    model->calibrateInfDkReversionsIterative(0, cpiHelpers, lm, ec);

    for (Size i = 0; i < cpiHelpers.size(); ++i) {
        BOOST_TEST_MESSAGE("i=" << i << " modelvol=" << model->infdk(0)->parameterValues(0)[i] << " modelrev="
                                << model->infdk(0)->parameterValues(1)[i] << " market=" << cpiHelpers[i]->marketValue()
                                << " model=" << cpiHelpers[i]->modelValue()
                                << " diff=" << (cpiHelpers[i]->marketValue() - cpiHelpers[i]->modelValue()));
    }

    // reprice last ZC floor with Monte Carlo
    Size n = 100000; // number of paths
    Size seed = 18;  // rng seed
    Size steps = 1;  // number of discretization steps

    QuantLib::ext::shared_ptr<StochasticProcess> process = model->stateProcess();
    LowDiscrepancy::rsg_type sg = LowDiscrepancy::make_sequence_generator(process->factors() * steps, seed);
    TimeGrid grid(T, steps);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg(process, grid, sg, false);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > floor;

    Real K = std::pow(1.0 + strike, T);

    for (Size i = 0; i < n; ++i) {
        Sample<MultiPath> path = pg.next();
        Size l = path.value[0].length() - 1;
        Real irz = path.value[0][l];
        Real infz = path.value[1][l];
        Real infy = path.value[2][l];
        Real I = model->infdkI(0, T, T, infz, infy).first;
        floor(std::max(-(I - K), 0.0) / model->numeraire(0, T, irz));
    }

    BOOST_TEST_MESSAGE("mc (low disc) floor last = " << mean(floor) << " +- " << error_of<tag::mean>(floor));

    // check model calibration
    Real tol = 1.0E-12;
    for (Size i = 0; i < cpiHelpers.size(); ++i) {
        if (std::abs(cpiHelpers[i]->modelValue() - cpiHelpers[i]->marketValue()) > tol) {
            BOOST_ERROR("Model calibration for ZC CPI Floor #"
                        << i << " failed, market premium is " << cpiHelpers[i]->marketValue() << ", model value is "
                        << cpiHelpers[i]->modelValue() << ", difference is "
                        << (cpiHelpers[i]->marketValue() - cpiHelpers[i]->modelValue()) << ", tolerance is " << tol);
        }
    }
    // check repricing with MC
    tol = 2.0E-4;
    Real mcPrice = mean(floor);
    if (std::abs(mcPrice - cpiHelpers[nMat - 1]->modelValue()) > tol) {
        BOOST_ERROR("Failed to reprice last ZC CPI Floor with MC ("
                    << mcPrice << "), analytical model price is " << cpiHelpers[nMat - 1]->modelValue()
                    << ", difference is " << mcPrice - cpiHelpers[nMat - 1]->modelValue() << ", tolerance is " << tol);
    }
} // testCpiCalibrationByH

BOOST_AUTO_TEST_CASE(testCrCalibration) {

    BOOST_TEST_MESSAGE("Testing calibration to CDS Options and repricing via MC...");

    // set up IR-CR model, calibrate to given CDS Options and check
    // the result with a MC simulation

    SavedSettings backup;
    Date refDate(30, July, 2015);
    Settings::instance().evaluationDate() = Date(30, July, 2015);

    // IR (zero vol)
    Handle<YieldTermStructure> eurYts(QuantLib::ext::make_shared<FlatForward>(refDate, 0.01, Actual365Fixed()));
    QuantLib::ext::shared_ptr<Parametrization> ireur_p =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), eurYts, 0.00, 0.01);

    // CR
    Handle<DefaultProbabilityTermStructure> prob(QuantLib::ext::make_shared<FlatHazardRate>(refDate, 0.01, Actual365Fixed()));

    Size nMat = 10;
    Real impliedVols[] = { 0.10, 0.12, 0.14, 0.16, 0.18, 0.2, 0.21, 0.215, 0.22, 0.225 };
    Period maturity[] = { 1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years,
                          6 * Years, 7 * Years, 8 * Years, 9 * Years, 10 * Years };

    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > cdsoHelpers;
    Array volStepTimes(nMat - 1), noTimes(0);
    Array crVols(nMat, 0.0030), crRev(nMat, 0.01); // init vol and rev

    Time T = Null<Time>();
    Date lastMat;
    for (Size i = 1; i <= nMat; ++i) {
        Date mat = refDate + maturity[i - 1];
        Schedule schedule(mat, mat + 10 * Years, 3 * Months, TARGET(), Following, Following, DateGeneration::CDS,
                          false);
        // ensure that cds starts after option expiry
        if (schedule.date(0) < mat)
            schedule = Schedule(schedule.date(1), mat + 10 * Years, 3 * Months, TARGET(), Following, Following,
                                DateGeneration::CDS, false);
        QL_REQUIRE(schedule.date(0) >= mat,
                   "CDS start (" << schedule.date(0) << ") should be on or after option maturity (" << mat << ")");
        QuantLib::ext::shared_ptr<CdsOptionHelper> h(
            new CdsOptionHelper(mat, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(impliedVols[i - 1])),
                                Protection::Buyer, schedule, Following, Actual360(), prob, 0.4, eurYts));
        Real t = eurYts->timeFromReference(mat);
        cdsoHelpers.push_back(h);
        if (i <= nMat - 1)
            volStepTimes[i - 1] = t;
        T = t;
        lastMat = mat;
    }

    QuantLib::ext::shared_ptr<CrLgm1fPiecewiseConstantParametrization> creur_p =
        QuantLib::ext::make_shared<CrLgm1fPiecewiseConstantParametrization>(EURCurrency(), prob, volStepTimes, crVols,
                                                                    volStepTimes, crRev);

    std::vector<QuantLib::ext::shared_ptr<Parametrization> > parametrizations;
    parametrizations.push_back(ireur_p);
    parametrizations.push_back(creur_p);

    QuantLib::ext::shared_ptr<CrossAssetModel> model =
        QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, Matrix(), SalvagingAlgorithm::None);

    model->setCorrelation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::CR, 0, 0.33);

    // pricing engine
    QuantLib::ext::shared_ptr<AnalyticLgmCdsOptionEngine> engine =
        QuantLib::ext::make_shared<AnalyticLgmCdsOptionEngine>(model, 0, 0, 0.4);

    for (Size i = 0; i < cdsoHelpers.size(); ++i) {
        cdsoHelpers[i]->setPricingEngine(engine);
    }

    // calibration
    LevenbergMarquardt lm;
    EndCriteria ec(1000, 500, 1E-8, 1E-8, 1E-8);
    model->calibrateCrLgm1fVolatilitiesIterative(0, cdsoHelpers, lm, ec);

    for (Size i = 0; i < cdsoHelpers.size(); ++i) {
        BOOST_TEST_MESSAGE("i=" << i << " modelvol=" << model->crlgm1f(0)->parameterValues(0)[i]
                                << " modelrev=" << model->crlgm1f(0)->parameterValues(1)[i] << " market="
                                << cdsoHelpers[i]->marketValue() << " model=" << cdsoHelpers[i]->modelValue()
                                << " diff=" << (cdsoHelpers[i]->marketValue() - cdsoHelpers[i]->modelValue()));
    }

    // check model calibration
    Real tol = 1.0E-12;
    for (Size i = 0; i < cdsoHelpers.size(); ++i) {
        if (std::abs(cdsoHelpers[i]->modelValue() - cdsoHelpers[i]->marketValue()) > tol) {
            BOOST_ERROR("Model calibration for CDSO #"
                        << i << " failed, market premium is " << cdsoHelpers[i]->marketValue() << ", model value is "
                        << cdsoHelpers[i]->modelValue() << ", difference is "
                        << (cdsoHelpers[i]->marketValue() - cdsoHelpers[i]->modelValue()) << ", tolerance is " << tol);
        }
    }

    Real lastModelValue = cdsoHelpers[nMat - 1]->modelValue();

    // reprice last CDSO with Monte Carlo
    // note that the IR vol is zero (same as assumption in CDSO analytic engine)
    Size n = 10000; // number of paths
    Size seed = 18;  // rng seed
    Size steps = 1;  // number of discretization steps

    QuantLib::ext::shared_ptr<StochasticProcess> process = model->stateProcess();
    LowDiscrepancy::rsg_type sg = LowDiscrepancy::make_sequence_generator(process->factors() * steps, seed);
    TimeGrid grid(T, steps);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(grid.size() - 1);
    }
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg(process, grid, sg, false);

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean> > > cdso;

    QuantLib::ext::shared_ptr<CreditDefaultSwap> underlying =
        QuantLib::ext::static_pointer_cast<CdsOptionHelper>(cdsoHelpers.back())->underlying();
    Real K = underlying->fairSpreadClean();
    BOOST_TEST_MESSAGE("Last CDSO fair spread is " << K);

    Settings::instance().evaluationDate() = lastMat;
    QuantLib::ext::shared_ptr<LgmImpliedDefaultTermStructure> probMc =
        QuantLib::ext::make_shared<LgmImpliedDefaultTermStructure>(model, 0, 0);
    QuantLib::ext::shared_ptr<LgmImpliedYieldTermStructure> ytsMc =
        QuantLib::ext::make_shared<LgmImpliedYieldTermStructure>(model->lgm(0));
    QuantLib::ext::shared_ptr<QuantExt::MidPointCdsEngine> dynamicEngine = QuantLib::ext::make_shared<QuantExt::MidPointCdsEngine>(
        Handle<DefaultProbabilityTermStructure>(probMc), 0.4, Handle<YieldTermStructure>(ytsMc));
    underlying->setPricingEngine(dynamicEngine);

    for (Size i = 0; i < n; ++i) {
        Sample<MultiPath> path = pg.next();
        Size l = path.value[0].length() - 1;
        Real irz = path.value[0][l];
        Real crz = path.value[1][l];
        Real cry = path.value[2][l];
        probMc->move(lastMat, crz, cry);
        ytsMc->move(lastMat, irz);
        Real surv = model->crlgm1fS(0, 0, T, T, crz, cry).first;
        Real npv = surv * std::max(underlying->NPV(), 0.0) / model->numeraire(0, T, irz);
        cdso(npv);
    }

    BOOST_TEST_MESSAGE("mc (low disc) cdso last = " << mean(cdso) << " +- " << error_of<tag::mean>(cdso));

    // check repricing with MC
    tol = 3.0E-4;
    Real mcPrice = mean(cdso);
    if (std::abs(mcPrice - lastModelValue) > tol) {
        BOOST_ERROR("Failed to reprice last CDSO with MC (" << mcPrice << "), analytical model price is "
                                                            << lastModelValue << ", difference is "
                                                            << mcPrice - lastModelValue << ", tolerance is " << tol);
    }
} // testCrCalibration

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
