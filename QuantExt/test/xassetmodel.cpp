/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include "xassetmodel.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
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
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;

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
    //  EUR            USD         FX
    c[0][0] = 1.0; c[0][1] = -0.2; c[0][2] = 0.8;  // EUR
    c[1][0] = -0.2; c[1][1] = 1.0; c[1][2] = -0.5; // USD
    c[2][0] = 0.8; c[2][1] = -0.5; c[2][2] = 1.0;  // FX

    boost::shared_ptr<XAssetModel> ccLgm =
        boost::make_shared<XAssetModel>(singleModels, c);

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

    MultiPathGenerator<PseudoRandom::rsg_type> pg(process, grid, sg, false);
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

test_suite *XAssetModelTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("XAsset model tests");
    suite->add(
        QUANTEXT_TEST_CASE(&XAssetModelTest::testCcyLgm3fForeignPayouts));
    return suite;
}
