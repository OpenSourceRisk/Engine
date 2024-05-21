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

#include "utilities.hpp"

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <qle/models/cdsoptionhelper.hpp>
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

#include <ql/currencies/europe.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace {
struct F : public qle::test::TopLevelFixture {
    F() { Settings::instance().evaluationDate() = Date(20, March, 2019); }
    ~F() {}
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(AnalyticLgmSwaptionEngineTest, F)

BOOST_AUTO_TEST_CASE(testMonoCurve) {

    BOOST_TEST_MESSAGE("Testing analytic LGM swaption engine coupon "
                       "adjustments in mono curve setup...");

    Handle<YieldTermStructure> flatCurve(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));

    const QuantLib::ext::shared_ptr<IrLgm1fConstantParametrization> irlgm1f =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), flatCurve, 0.01, 0.01);

    // no curve attached
    QuantLib::ext::shared_ptr<SwapIndex> index_nocurves = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years);

    // forward curve attached
    QuantLib::ext::shared_ptr<SwapIndex> index_monocurve = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, flatCurve);

    Swaption swaption_nocurves = MakeSwaption(index_nocurves, 10 * Years, 0.02);
    Swaption swaption_monocurve = MakeSwaption(index_monocurve, 10 * Years, 0.02);

    QuantLib::ext::shared_ptr<PricingEngine> engine_nodisc = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1f);
    QuantLib::ext::shared_ptr<PricingEngine> engine_monocurve =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1f, flatCurve);

    swaption_nocurves.setPricingEngine(engine_nodisc);
    swaption_nocurves.NPV();
    std::vector<Real> fixedAmountCorrections1 = swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement1 = swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");
    swaption_nocurves.setPricingEngine(engine_monocurve);
    swaption_nocurves.NPV();
    std::vector<Real> fixedAmountCorrections2 = swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement2 = swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");
    swaption_monocurve.setPricingEngine(engine_nodisc);
    swaption_monocurve.NPV();
    std::vector<Real> fixedAmountCorrections3 = swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement3 = swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");
    swaption_monocurve.setPricingEngine(engine_monocurve);
    swaption_monocurve.NPV();
    std::vector<Real> fixedAmountCorrections4 = swaption_nocurves.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement4 = swaption_nocurves.result<Real>("fixedAmountCorrectionSettlement");

    if (fixedAmountCorrections1.size() != 10) {
        BOOST_ERROR("fixed coupon adjustment vector 1 should have size 10, "
                    "but actually has size "
                    << fixedAmountCorrections1.size());
    }
    if (fixedAmountCorrections2.size() != 10) {
        BOOST_ERROR("fixed coupon adjustment vector 2 should have size 10, "
                    "but actually has size "
                    << fixedAmountCorrections2.size());
    }
    if (fixedAmountCorrections3.size() != 10) {
        BOOST_ERROR("fixed coupon adjustment vector 3 should have size 10, "
                    "but actually has size "
                    << fixedAmountCorrections3.size());
    }
    if (fixedAmountCorrections4.size() != 10) {
        BOOST_ERROR("fixed coupon adjustment vector 4 should have size 10, "
                    "but actually has size "
                    << fixedAmountCorrections4.size());
    }

    for (Size i = 0; i < 10; ++i) {
        if (!close_enough(fixedAmountCorrections1[i], 0.0)) {
            BOOST_ERROR("fixed coupon adjustment (1) should be zero in mono "
                        "curve setup, but component "
                        << i << " is " << fixedAmountCorrections1[i]);
        }
        if (!close_enough(fixedAmountCorrections2[i], 0.0)) {
            BOOST_ERROR("fixed coupon adjustment (2) should be zero in mono "
                        "curve setup, but component "
                        << i << " is " << fixedAmountCorrections2[i]);
        }
        if (!close_enough(fixedAmountCorrections3[i], 0.0)) {
            BOOST_ERROR("fixed coupon adjustment (3) should be zero in mono "
                        "curve setup, but component "
                        << i << " is " << fixedAmountCorrections3[i]);
        }
        if (!close_enough(fixedAmountCorrections4[i], 0.0)) {
            BOOST_ERROR("fixed coupon adjustment (4) should be zero in mono "
                        "curve setup, but component "
                        << i << " is " << fixedAmountCorrections4[i]);
        }
    }

    if (!close_enough(fixedAmountCorrectionSettlement1, 0.0)) {
        BOOST_ERROR("fixed amount correction on settlement (1) should be "
                    "zero in mono curve setup, but is "
                    << fixedAmountCorrectionSettlement1);
    }
    if (!close_enough(fixedAmountCorrectionSettlement2, 0.0)) {
        BOOST_ERROR("fixed amount correction on settlement (2) should be "
                    "zero in mono curve setup, but is "
                    << fixedAmountCorrectionSettlement2);
    }
    if (!close_enough(fixedAmountCorrectionSettlement3, 0.0)) {
        BOOST_ERROR("fixed amount correction on settlement (3) should be "
                    "zero in mono curve setup, but is "
                    << fixedAmountCorrectionSettlement3);
    }
    if (!close_enough(fixedAmountCorrectionSettlement4, 0.0)) {
        BOOST_ERROR("fixed amount correction on settlement (4) should be "
                    "zero in mono curve setup, but is "
                    << fixedAmountCorrectionSettlement4);
    }
}

BOOST_AUTO_TEST_CASE(testDualCurve) {

    BOOST_TEST_MESSAGE("Testing analytic LGM swaption engine coupon "
                       "adjustments in dual curve setup...");

    // discounting curve
    Handle<YieldTermStructure> discCurve(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    // forward (+10bp)
    Handle<YieldTermStructure> forwardCurve1(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0210, Actual365Fixed()));
    // forward (-10bp)
    Handle<YieldTermStructure> forwardCurve2(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0190, Actual365Fixed()));

    const QuantLib::ext::shared_ptr<IrLgm1fConstantParametrization> irlgm1f =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), discCurve, 0.01, 0.01);

    // forward curve attached
    QuantLib::ext::shared_ptr<SwapIndex> index1 = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve1);
    QuantLib::ext::shared_ptr<SwapIndex> index2 = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve2);

    Swaption swaption1 = MakeSwaption(index1, 10 * Years, 0.02);
    Swaption swaption2 = MakeSwaption(index2, 10 * Years, 0.02);

    QuantLib::ext::shared_ptr<PricingEngine> engine_a =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1f, discCurve, AnalyticLgmSwaptionEngine::nextCoupon);

    QuantLib::ext::shared_ptr<PricingEngine> engine_b =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1f, discCurve, AnalyticLgmSwaptionEngine::proRata);

    swaption1.setPricingEngine(engine_a);
    swaption1.NPV();
    std::vector<Real> fixedAmountCorrections1a = swaption1.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement1a = swaption1.result<Real>("fixedAmountCorrectionSettlement");
    swaption2.setPricingEngine(engine_a);
    swaption2.NPV();
    std::vector<Real> fixedAmountCorrections2a = swaption2.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement2a = swaption2.result<Real>("fixedAmountCorrectionSettlement");
    swaption1.setPricingEngine(engine_b);
    swaption1.NPV();
    std::vector<Real> fixedAmountCorrections1b = swaption1.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement1b = swaption1.result<Real>("fixedAmountCorrectionSettlement");
    swaption2.setPricingEngine(engine_b);
    swaption2.NPV();
    std::vector<Real> fixedAmountCorrections2b = swaption2.result<std::vector<Real> >("fixedAmountCorrections");
    Real fixedAmountCorrectionSettlement2b = swaption2.result<Real>("fixedAmountCorrectionSettlement");

    // check corrections on settlement for plausibility

    Real tolerance = 0.000025; // 0.25 bp

    if (!close_enough(fixedAmountCorrectionSettlement1a, 0.0)) {
        BOOST_ERROR("fixed amount correction on settlement (1) should be "
                    "0 for nextCoupon, but is "
                    << fixedAmountCorrectionSettlement1a);
    }
    if (!close_enough(fixedAmountCorrectionSettlement2a, 0.0)) {
        BOOST_ERROR("fixed amount correction on settlement (2) should be "
                    "0 for nextCoupon, but is "
                    << fixedAmountCorrectionSettlement2a);
    }
    if (std::abs(fixedAmountCorrectionSettlement1b - 0.00025) > tolerance) {
        BOOST_ERROR("fixed amount correction on settlement (1) should be "
                    "close to 2.5bp for proRata, but is "
                    << fixedAmountCorrectionSettlement1b);
    }
    if (std::abs(fixedAmountCorrectionSettlement2b + 0.00025) > tolerance) {
        BOOST_ERROR("fixed amount correction on settlement (2) should be "
                    "close to -2.5bp for proRata, but is "
                    << fixedAmountCorrectionSettlement2b);
    }

    // we can assume that the result vectors have the correct size, this
    // was tested above

    for (Size i = 0; i < 10; ++i) {
        // amount correction should be close to +10bp (-10bp)
        // up to conventions, check this for plausibility
        if (std::abs(fixedAmountCorrections1a[i] - 0.0010) > tolerance) {
            BOOST_ERROR("fixed coupon adjustment (1, nextCoupon) should "
                        "be close to 10bp for "
                        "a 10bp curve spread, but is "
                        << fixedAmountCorrections1a[i] << " for component " << i);
        }
        if (std::abs(fixedAmountCorrections2a[i] + 0.0010) > tolerance) {
            BOOST_ERROR("fixed coupon adjustment (2, nextCoupon) should "
                        "be close to -10bp for "
                        "a -10bp curve spread, but is "
                        << fixedAmountCorrections2a[i] << " for component " << i);
        }
        if (std::abs(fixedAmountCorrections1b[i] - (i == 9 ? 0.00075 : 0.0010)) > tolerance) {
            BOOST_ERROR("fixed coupon adjustment (1, proRata) should "
                        "be close to 10bp (7.5bp for component 9) for "
                        "a 10bp curve spread, but is "
                        << fixedAmountCorrections1b[i] << " for component " << i);
        }
        if (std::abs(fixedAmountCorrections2b[i] + (i == 9 ? 0.00075 : 0.0010)) > tolerance) {
            BOOST_ERROR("fixed coupon adjustment (2, proRata) should "
                        "be close to -10bp (-7.5bp for component 9) for "
                        "a -10bp curve spread, but is "
                        << fixedAmountCorrections2b[i] << " for component " << i);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAgainstOtherEngines) {

    BOOST_TEST_MESSAGE("Testing analytic LGM swaption engine against "
                       "G1d adaptor / Gsr integral and Hull White fd engines...");

    Real discountingRateLevel[] = { -0.0050, 0.01, 0.03, 0.10 };
    Real forwardingRateLevel[] = { -0.0100, 0.01, 0.04, 0.12 };

    // Hull White only allows for positive reversion levels
    Real kappa[] = { 0.01, 0.00001, 0.01, 0.05 };

    // the model volatilities are meant to be Hull White volatilities
    // they are fed into the LGM model via the HW adaptor below
    // the rationale is to have another independent model
    // (QuantLib::HullWhite) and pricing engine
    // (QUantLib::FdHullWhiteSwaptionEngine) available for validation

    Real sigma[] = { 0.0001, 0.01, 0.02 };

    Real strikeOffset[] = { -0.05, -0.02, -0.01, 0.0, 0.01, 0.02, 0.05 };

    Size no = 0;

    // tolerance for comparison FD engine vs integral engines
    Real tol1 = 3.0E-4;

    // tolerance for comparison of integral engines based
    // on GSR and LGM model
    Real tol2 = 1.0E-4;

    // tolerance for LGM integral engine and analytical engine
    // in the case of no basis between discounting and forwarding
    Real tol3 = 0.6E-4;

    // tolerance for LGM integral engine and analytical engine
    // in the case of a non zero basis between discounting and
    // forwarding curve (mapping type a and b)
    // this scales with sigma, the tolerances here are
    // for sigma = 0.01
    Real tol4a = 6.0E-4, tol4b = 4.0E-4;

    for (Size i = 0; i < LENGTH(discountingRateLevel); ++i) {
        for (Size k = 0; k < LENGTH(kappa); ++k) {
            for (Size l = 0; l < LENGTH(sigma); ++l) {

                Handle<YieldTermStructure> discountingCurve(
                    QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), discountingRateLevel[i], Actual365Fixed()));
                Handle<YieldTermStructure> forwardingCurve(
                    QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardingRateLevel[i], Actual365Fixed()));

                Array times(0);
                Array sigma_a(1, sigma[l]);
                Array kappa_a(1, kappa[k]);
                std::vector<Date> dates(0);
                std::vector<Real> sigma_v(1, sigma[l]);
                std::vector<Real> kappa_v(1, kappa[k]);

                const QuantLib::ext::shared_ptr<IrLgm1fPiecewiseConstantHullWhiteAdaptor> irlgm1f =
                    QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), discountingCurve, times,
                                                                                 sigma_a, times, kappa_a);

                std::vector<QuantLib::ext::shared_ptr<Parametrization> > params;
                params.push_back(irlgm1f);
                Matrix rho(1, 1);
                rho[0][0] = 1.0;
                const QuantLib::ext::shared_ptr<CrossAssetModel> crossasset = QuantLib::ext::make_shared<CrossAssetModel>(params, rho);

                const QuantLib::ext::shared_ptr<Gaussian1dModel> g1d =
                    QuantLib::ext::make_shared<Gaussian1dCrossAssetAdaptor>(0, crossasset);

                const QuantLib::ext::shared_ptr<Gsr> gsr = QuantLib::ext::make_shared<Gsr>(discountingCurve, dates, sigma_v, kappa_v);

                const QuantLib::ext::shared_ptr<HullWhite> hw =
                    QuantLib::ext::make_shared<HullWhite>(discountingCurve, kappa[k], sigma[l]);

                QuantLib::ext::shared_ptr<PricingEngine> engine_map_a = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(
                    irlgm1f, discountingCurve, AnalyticLgmSwaptionEngine::nextCoupon);
                QuantLib::ext::shared_ptr<PricingEngine> engine_map_b = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(
                    irlgm1f, discountingCurve, AnalyticLgmSwaptionEngine::proRata);

                QuantLib::ext::shared_ptr<PricingEngine> engine_g1d =
                    QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(g1d, 128, 7.0, true, false, discountingCurve);

                QuantLib::ext::shared_ptr<PricingEngine> engine_gsr =
                    QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(gsr, 128, 7.0, true, false, discountingCurve);

                QuantLib::ext::shared_ptr<PricingEngine> engine_fd =
                    QuantLib::ext::make_shared<FdHullWhiteSwaptionEngine>(hw, 400, 400, 0, 1.0E-8);

                QuantLib::ext::shared_ptr<SwapIndex> index =
                    QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardingCurve, discountingCurve);
                Real atmStrike = index->fixing(TARGET().advance(Settings::instance().evaluationDate(), 5 * Years));

                for (Size s = 0; s < LENGTH(strikeOffset); ++s) {

                    // we have to ensure positive effective fixed flows for
                    // the analytic engine (this is checked there, but we
                    // want to avoid exceptions thrown during testing)
                    if (atmStrike + strikeOffset[s] - (forwardingRateLevel[i] - discountingRateLevel[i]) < 0.0001) {
                        continue;
                    }

                    Swaption swaption =
                        MakeSwaption(index, 5 * Years, atmStrike + strikeOffset[s])
                            .withUnderlyingType(strikeOffset[s] > 0.0 ? VanillaSwap::Payer : VanillaSwap::Receiver);

                    swaption.setPricingEngine(engine_map_a);
                    Real npv_map_a = swaption.NPV();
                    swaption.setPricingEngine(engine_map_b);
                    Real npv_map_b = swaption.NPV();
                    swaption.setPricingEngine(engine_g1d);
                    Real npv_g1d = swaption.NPV();
                    swaption.setPricingEngine(engine_gsr);
                    Real npv_gsr = swaption.NPV();
                    swaption.setPricingEngine(engine_fd);
                    Real npv_fd = swaption.NPV();

                    if (std::abs(npv_fd - npv_gsr) > tol1) {
                        BOOST_ERROR("inconsistent swaption npvs (fd="
                                    << npv_fd << ", gsr=" << npv_gsr << ") for case #" << no
                                    << " with discounting rate=" << discountingRateLevel[i]
                                    << ", forwarding rate=" << forwardingRateLevel[i] << ", kappa=" << kappa[k]
                                    << ", sigma=" << sigma[l] << ", strike offset=" << strikeOffset[s]);
                    }

                    if (std::abs(npv_gsr - npv_g1d) > tol2) {
                        BOOST_ERROR("inconsistent swaption npvs (gsr="
                                    << npv_gsr << ", npv_g1d=" << npv_g1d << ") for case #" << no
                                    << " with discounting rate=" << discountingRateLevel[i]
                                    << ", forwarding rate=" << forwardingRateLevel[i] << ", kappa=" << kappa[k]
                                    << ", sigma=" << sigma[l] << ", strike offset=" << strikeOffset[s]);
                    }

                    Real tolTmpA = 0.0, tolTmpB = 0.0;
                    if (std::abs(discountingRateLevel[i] - forwardingRateLevel[i]) < 1.0E-6) {
                        tolTmpA = tolTmpB = tol3;
                    } else {
                        tolTmpA = tol4a * std::max(sigma[l], 0.01) / 0.01; // see above
                        tolTmpB = tol4b * std::max(sigma[l], 0.01) / 0.01; // see above
                    }

                    if (std::abs(npv_g1d - npv_map_a) > tolTmpA) {
                        BOOST_ERROR("inconsistent swaption npvs (g1d="
                                    << npv_g1d << ", map_a=" << npv_map_a << "), tolerance is " << tolTmpA
                                    << ", for case #" << no << " with discounting rate=" << discountingRateLevel[i]
                                    << ", forwarding rate=" << forwardingRateLevel[i] << ", kappa=" << kappa[k]
                                    << ", sigma=" << sigma[l] << ", strike offset=" << strikeOffset[s]);
                    }

                    if (std::abs(npv_g1d - npv_map_b) > tolTmpB) {
                        BOOST_ERROR("inconsistent swaption npvs (g1d="
                                    << npv_g1d << ", map_b=" << npv_map_b << "), tolerance is " << tolTmpB
                                    << ", for case #" << no << " with discounting rate=" << discountingRateLevel[i]
                                    << ", forwarding rate=" << forwardingRateLevel[i] << ", kappa=" << kappa[k]
                                    << ", sigma=" << sigma[l] << ", strike offset=" << strikeOffset[s]);
                    }

                    no++;
                }
            }
        }
    }
} // testAgainstOtherEngines

BOOST_AUTO_TEST_CASE(testLgmInvariances) {

    BOOST_TEST_MESSAGE("Testing LGM model invariances in the analytic LGM "
                       "swaption engine...");

    Real shift[] = { -2.0, -1.0, 0.0, 1.0, 2.0 };
    Real scaling[] = { 5.0, 2.0, 1.0, 0.1, 0.01, -0.01, -0.1, -1.0, -2.0, -5.0 };

    Handle<YieldTermStructure> discountingCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.03, Actual365Fixed()));
    Handle<YieldTermStructure> forwardingCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, Actual365Fixed()));

    Array times(0);
    Array sigma_a(1, 0.01);
    Array alpha_a(1, 0.01);
    Array kappa_a(1, 0.01);

    QuantLib::ext::shared_ptr<SwapIndex> index =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardingCurve, discountingCurve);
    Swaption swaption = MakeSwaption(index, 5 * Years, 0.07); // otm

    for (Size i = 0; i < LENGTH(shift); ++i) {
        for (Size j = 0; j < LENGTH(scaling); ++j) {

            const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1f0 =
                QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), discountingCurve, 0.01, 0.01);

            const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1fa =
                QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), discountingCurve, 0.01, 0.01);
            irlgm1fa->shift() = shift[i];
            irlgm1fa->scaling() = scaling[j];

            const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1fb =
                QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), discountingCurve, times,
                                                                            alpha_a, times, kappa_a);
            irlgm1fb->shift() = shift[i];
            irlgm1fb->scaling() = scaling[j];

            const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1f0c =
                QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), discountingCurve, times,
                                                                             sigma_a, times, kappa_a);

            const QuantLib::ext::shared_ptr<IrLgm1fParametrization> irlgm1fc =
                QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), discountingCurve, times,
                                                                             sigma_a, times, kappa_a);
            irlgm1fc->shift() = shift[i];
            irlgm1fc->scaling() = scaling[j];

            const QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm0 = QuantLib::ext::make_shared<LinearGaussMarkovModel>(irlgm1f0);
            const QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgma = QuantLib::ext::make_shared<LinearGaussMarkovModel>(irlgm1fa);
            const QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgmb = QuantLib::ext::make_shared<LinearGaussMarkovModel>(irlgm1fb);
            const QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm0c =
                QuantLib::ext::make_shared<LinearGaussMarkovModel>(irlgm1f0c);
            const QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgmc = QuantLib::ext::make_shared<LinearGaussMarkovModel>(irlgm1fc);

            QuantLib::ext::shared_ptr<PricingEngine> engine0 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1f0);
            QuantLib::ext::shared_ptr<PricingEngine> enginea = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1fa);
            QuantLib::ext::shared_ptr<PricingEngine> engineb = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1fb);
            QuantLib::ext::shared_ptr<PricingEngine> engine0c = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1f0c);
            QuantLib::ext::shared_ptr<PricingEngine> enginec = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(irlgm1fc);

            swaption.setPricingEngine(engine0);
            Real npv0 = swaption.NPV();
            swaption.setPricingEngine(enginea);
            Real npva = swaption.NPV();
            swaption.setPricingEngine(engineb);
            Real npvb = swaption.NPV();
            swaption.setPricingEngine(engine0c);
            Real npv0c = swaption.NPV();
            swaption.setPricingEngine(enginec);
            Real npvc = swaption.NPV();

            Real tol = 1.0E-10;
            if (std::fabs(npva - npv0) > tol) {
                BOOST_ERROR("price is not invariant under (shift,scaling)=(" << shift[i] << "," << scaling[i]
                                                                             << "), difference is " << (npva - npv0)
                                                                             << " (constant parametrization)");
            }
            if (std::fabs(npvb - npv0) > tol) {
                BOOST_ERROR("price is not invariant under (shift,scaling)=("
                            << shift[i] << "," << scaling[i] << "), difference is " << (npvb - npv0)
                            << " (piecewise constant parametrization)");
            }
            if (std::fabs(npvc - npv0c) > tol) {
                BOOST_ERROR("price is not invariant under (shift,scaling)=("
                            << shift[i] << "," << scaling[i] << "), difference is " << (npvc - npv0c)
                            << " (hull white adaptor parametrization)");
            }
        }
    }
} // testInvariances

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
