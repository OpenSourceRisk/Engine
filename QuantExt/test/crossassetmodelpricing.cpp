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
#include <qle/indexes/equityindex.hpp>
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

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CrossAssetModelTest)


namespace {

struct Lgm5fTestDataV {
    Lgm5fTestDataV()
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


} // anonymous namespace

BOOST_AUTO_TEST_CASE(testLgmCalibrationPricing) {

    BOOST_TEST_MESSAGE("Testing full calibration of Ccy LGM 5F model pricing ...");

    Lgm5fTestDataV d1;
    Lgm5fTestDataV d2;
    Lgm5fTestDataV d3;
    Lgm5fTestDataV d4;

    // calibration baskets
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper> > basketEur1, basketEur2, basketEur3, basketEur4, basketEurGbp;

    QuantLib::ext::shared_ptr<IborIndex> euribor6m = QuantLib::ext::make_shared<Euribor>(6 * Months, d1.eurYts);

    for (Size i = 0; i <= d1.volstepdates.size(); ++i) {
        Date tmp = i < d1.volstepdates.size() ? d1.volstepdates[i] : d1.volstepdates.back() + 365;
        // EUR: atm+200bp, 150bp normal vol

        BOOST_TEST_MESSAGE("Input Swaption " << i << ": From " << tmp << " to " << "10Y");

        basketEur1.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.015)), euribor6m, 1 * Years, Thirty360(Thirty360::BondBasis),
            Actual360(), d1.eurYts, BlackCalibrationHelper::RelativePriceError, 0.02, 1.0, Normal)));

        basketEur2.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.025)), euribor6m, 1 * Years, Thirty360(Thirty360::BondBasis),
            Actual360(), d2.eurYts, BlackCalibrationHelper::RelativePriceError, 0.02, 1.0, Normal)));

        basketEur3.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.035)), euribor6m, 1 * Years, Thirty360(Thirty360::BondBasis),
            Actual360(), d3.eurYts, BlackCalibrationHelper::RelativePriceError, 0.0000008, 1.0, Normal)));

        basketEur4.push_back(QuantLib::ext::shared_ptr<SwaptionHelper>(new SwaptionHelper(
            tmp, 10 * Years, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.035)), euribor6m, 1 * Years, Thirty360(Thirty360::BondBasis),
            Actual360(), d4.eurYts, BlackCalibrationHelper::RelativePriceError, 0.0000008, 1.0, Normal)));
    }

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(d1.ccLgmExact, 0);
    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng2 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(d2.ccLgmExact, 0);

    // assign engines to calibration instruments
    for (Size i = 0; i < basketEur1.size(); ++i) {
        basketEur1[i]->setPricingEngine(eurSwEng1);
    }

    for (Size i = 0; i < basketEur2.size(); ++i) {
        basketEur2[i]->setPricingEngine(eurSwEng2);
    }

    LevenbergMarquardt lm1(1E-14, 1E-14, 1E-14);
    EndCriteria ec1(1000, 500, 1E-14, 1E-14, 1E-14);
    d1.ccLgmExact->calibrateIrLgm1fVolatilitiesIterative(0, basketEur1, lm1, ec1);

    LevenbergMarquardt lm2(1E-14, 1E-14, 1E-14);
    EndCriteria ec2(1000, 500, 1E-14, 1E-14, 1E-14);
    d2.ccLgmExact->calibrateIrLgm1fVolatilitiesIterative(0, basketEur2, lm2, ec2);

    for (Size i = 0; i < basketEur3.size(); ++i) {
        basketEur3[i]->setPricingEngine(eurSwEng1);
    }

    for (Size i = 0; i < basketEur4.size(); ++i) {
        basketEur4[i]->setPricingEngine(eurSwEng2);
    }

    for (Size i = 0; i < basketEur1.size(); ++i) {
        Real model2 = basketEur3[i]->modelValue();
        Real model3 = basketEur4[i]->modelValue();
        
        BOOST_TEST_MESSAGE("Swaption "<<i<<": " << model2 << ", " << model3 << " diff: " << (model3 - model2) * 10000.00 <<" bp.");
    }

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
