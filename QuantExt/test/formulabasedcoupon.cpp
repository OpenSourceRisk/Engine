/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <boost/assign/std/vector.hpp>
#include <qle/cashflows/formulabasedcoupon.hpp>
#include <qle/cashflows/mcgaussianformulabasedcouponpricer.hpp>
#include <qle/indexes/formulabasedindex.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/experimental/coupons/lognormalcmsspreadpricer.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/cashflows/quantocouponpricer.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

namespace {
struct TestData {

    TestData() {
        refDate = Date(23, February, 2018);
        samples = 100000;
        Settings::instance().evaluationDate() = refDate;

        yts2 = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(refDate, 0.02, Actual365Fixed()));

        ovtLn = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            refDate, TARGET(), Following, 0.20, Actual365Fixed(), ShiftedLognormal, 0.0));
        ovtSln = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            refDate, TARGET(), Following, 0.10, Actual365Fixed(), ShiftedLognormal, 0.01));
        ovtN = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            refDate, TARGET(), Following, 0.0075, Actual365Fixed(), Normal, 0.0));

        swLn = Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<ConstantSwaptionVolatility>(
            refDate, TARGET(), Following, 0.20, Actual365Fixed(), ShiftedLognormal, 0.0));
        swSln = Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<ConstantSwaptionVolatility>(
            refDate, TARGET(), Following, 0.10, Actual365Fixed(), ShiftedLognormal, 0.01));
        swN = Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<ConstantSwaptionVolatility>(
            refDate, TARGET(), Following, 0.0075, Actual365Fixed(), Normal, 0.01));

        fxVol = Handle<BlackVolTermStructure>(
            QuantLib::ext::make_shared<BlackConstantVol>(refDate, TARGET(), 0.20, Actual365Fixed()));

        blackPricerLn = QuantLib::ext::make_shared<BlackIborCouponPricer>(ovtLn);
        blackPricerSln = QuantLib::ext::make_shared<BlackIborCouponPricer>(ovtSln);
        blackPricerN = QuantLib::ext::make_shared<BlackIborCouponPricer>(ovtN);

        reversion = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.01));
        cmsPricerLn = QuantLib::ext::make_shared<LinearTsrPricer>(swLn, reversion, yts2);
        cmsPricerSln = QuantLib::ext::make_shared<LinearTsrPricer>(swSln, reversion, yts2);
        cmsPricerN = QuantLib::ext::make_shared<LinearTsrPricer>(swN, reversion, yts2);

        correlation = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6));
        correlationTS = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<FlatCorrelation>(
            refDate, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6)), Actual365Fixed()));
        cmsspPricerLn = QuantLib::ext::make_shared<LognormalCmsSpreadPricer>(cmsPricerLn, correlation, yts2, 32);
        cmsspPricerSln = QuantLib::ext::make_shared<LognormalCmsSpreadPricer>(cmsPricerSln, correlation, yts2, 32);
        cmsspPricerN = QuantLib::ext::make_shared<LognormalCmsSpreadPricer>(cmsPricerN, correlation, yts2, 32);

        quantoCorrelation = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.8));
        quantoCorrelationTS = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<FlatCorrelation>(
            refDate, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.8)), Actual365Fixed()));
        blackQuantoPricerLn =
            QuantLib::ext::make_shared<QuantExt::BlackIborQuantoCouponPricer>(fxVol, quantoCorrelation, ovtLn);
        blackQuantoPricerSln =
            QuantLib::ext::make_shared<QuantExt::BlackIborQuantoCouponPricer>(fxVol, quantoCorrelation, ovtSln);
        blackQuantoPricerN = QuantLib::ext::make_shared<QuantExt::BlackIborQuantoCouponPricer>(fxVol, quantoCorrelation, ovtN);
        // correlation term structures
        std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> indCorrelationTS;
        indCorrelationTS[std::make_pair("EuriborSwapIsdaFixA2Y 30/360 (Bond Basis)",
                                        "EuriborSwapIsdaFixA10Y 30/360 (Bond Basis)")] = correlationTS;
        indCorrelationTS[std::make_pair("Euribor6M Actual/360", "FX")] = quantoCorrelationTS;
        std::map<std::string, QuantLib::ext::shared_ptr<IborCouponPricer>> iborPricers;
        std::map<std::string, QuantLib::ext::shared_ptr<CmsCouponPricer>> cmsPricers;
        std::map<std::string, Handle<BlackVolTermStructure>> fxVols, emptyFxVols;
        fxVols["EUR"] = fxVol; // vs USD
	std::string key = QuantLib::ext::make_shared<Euribor>(6 * Months)->name();
        iborPricers[key] = blackPricerLn;
        cmsPricers[key] = cmsPricerLn;
        formulaPricerLn = QuantLib::ext::make_shared<MCGaussianFormulaBasedCouponPricer>(
            "EUR", iborPricers, cmsPricers, emptyFxVols, indCorrelationTS, yts2, samples);
        formulaPricerUSDLn = QuantLib::ext::make_shared<MCGaussianFormulaBasedCouponPricer>(
            "USD", iborPricers, cmsPricers, fxVols, indCorrelationTS, yts2, samples);
        iborPricers[key] = blackPricerSln;
        cmsPricers[key] = cmsPricerSln;
        formulaPricerSln = QuantLib::ext::make_shared<MCGaussianFormulaBasedCouponPricer>(
            "EUR", iborPricers, cmsPricers, emptyFxVols, indCorrelationTS, yts2, samples);
        formulaPricerUSDSln = QuantLib::ext::make_shared<MCGaussianFormulaBasedCouponPricer>(
            "USD", iborPricers, cmsPricers, fxVols, indCorrelationTS, yts2, samples);
        iborPricers[key] = blackPricerN;
        cmsPricers[key] = cmsPricerN;
        formulaPricerN = QuantLib::ext::make_shared<MCGaussianFormulaBasedCouponPricer>(
            "EUR", iborPricers, cmsPricers, emptyFxVols, indCorrelationTS, yts2, samples);
        formulaPricerUSDN = QuantLib::ext::make_shared<MCGaussianFormulaBasedCouponPricer>(
            "USD", iborPricers, cmsPricers, fxVols, indCorrelationTS, yts2, samples);
    }

    SavedSettings backup;
    Date refDate;
    Size samples;
    Handle<YieldTermStructure> yts2;
    Handle<OptionletVolatilityStructure> ovtLn, ovtSln, ovtN;
    Handle<SwaptionVolatilityStructure> swLn, swSln, swN;
    Handle<BlackVolTermStructure> fxVol;
    Handle<Quote> reversion, correlation, quantoCorrelation;
    Handle<QuantExt::CorrelationTermStructure> correlationTS, quantoCorrelationTS;
    QuantLib::ext::shared_ptr<IborCouponPricer> blackPricerLn, blackPricerSln, blackPricerN;
    QuantLib::ext::shared_ptr<IborCouponPricer> blackQuantoPricerLn, blackQuantoPricerSln, blackQuantoPricerN;
    QuantLib::ext::shared_ptr<CmsCouponPricer> cmsPricerLn, cmsPricerSln, cmsPricerN;
    QuantLib::ext::shared_ptr<CmsSpreadCouponPricer> cmsspPricerLn, cmsspPricerSln, cmsspPricerN;
    QuantLib::ext::shared_ptr<FormulaBasedCouponPricer> formulaPricerLn, formulaPricerSln, formulaPricerN;
    QuantLib::ext::shared_ptr<FormulaBasedCouponPricer> formulaPricerUSDLn, formulaPricerUSDSln, formulaPricerUSDN;
};

void runTest(QuantLib::ext::shared_ptr<FloatingRateCoupon> cpn, QuantLib::ext::shared_ptr<FloatingRateCoupon> cpnRef,
             const QuantLib::ext::shared_ptr<FloatingRateCouponPricer>& pricer,
             const QuantLib::ext::shared_ptr<FloatingRateCouponPricer>& pricerRef, const std::string& testLabel,
             const Real tol) {

    cpnRef->setPricer(pricerRef);
    cpn->setPricer(pricer);

    // for capped floored coupons we need to explicitly set the underlying, this is fixed in QL 1.12
    auto cfRef = QuantLib::ext::dynamic_pointer_cast<CappedFlooredCoupon>(cpnRef);
    if (cfRef)
        cfRef->underlying()->setPricer(pricerRef);
    auto cf = QuantLib::ext::dynamic_pointer_cast<CappedFlooredCoupon>(cpn);
    if (cf)
        cf->underlying()->setPricer(pricer);

    boost::timer::cpu_timer timer;
    Real aRef = cpnRef->amount();
    timer.stop();
    Real tRef = timer.elapsed().wall * 1e-6;
    timer.start();
    Real a = cpn->amount();
    timer.stop();
    Real t = timer.elapsed().wall * 1e-6;
    BOOST_TEST_MESSAGE(testLabel << ": amount = " << a << " (" << t << " ms), reference amount = " << aRef << " ("
                                 << tRef << " ms)");
    BOOST_CHECK_SMALL(std::abs(a - aRef), tol);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FormulaBasedCouponTest)

BOOST_AUTO_TEST_CASE(testCappedLiborCoupon) {

    BOOST_TEST_MESSAGE("Testing formula based coupons against capped Libor coupon...");

    TestData d;

    auto euribor6m = QuantLib::ext::make_shared<Euribor>(6 * Months, d.yts2);

    CompiledFormula formulaPlain = CompiledFormula(Size(0));                              // plain payoff
    CompiledFormula formulaCapped = min(CompiledFormula(Size(0)), CompiledFormula(0.03)); // capped ibor coupon, at 0.03

    auto indexPlain = QuantLib::ext::make_shared<FormulaBasedIndex>(
        "libor-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{euribor6m}, formulaPlain,
        euribor6m->fixingCalendar());
    auto indexCapped = QuantLib::ext::make_shared<FormulaBasedIndex>(
        "libor-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{euribor6m}, formulaCapped,
        euribor6m->fixingCalendar());

    auto undRef = QuantLib::ext::make_shared<IborCoupon>(Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                                 Date(23, February, 2029), 2, euribor6m, 1.0, 0.0, Date(), Date(),
                                                 Actual360(), false);
    auto cappedRef = QuantLib::ext::make_shared<CappedFlooredCoupon>(undRef, 0.03);

    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> und(
        new QuantExt::FormulaBasedCoupon(EURCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexPlain, Date(), Date(), Actual360(), false));
    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> capped(
        new QuantExt::FormulaBasedCoupon(EURCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexCapped, Date(), Date(), Actual360(), false));

    runTest(und, undRef, d.formulaPricerLn, d.blackPricerLn, "Plain Ibor Coupon, Lognormal", 0.05);
    runTest(und, undRef, d.formulaPricerSln, d.blackPricerSln, "Plain Ibor Coupon, ShiftedLN", 0.05);
    runTest(und, undRef, d.formulaPricerN, d.blackPricerN, "Plain Ibor Coupon, Normal", 0.05);

    runTest(capped, cappedRef, d.formulaPricerLn, d.blackPricerLn, "Capped Ibor Coupon, Lognormal", 0.05);
    runTest(capped, cappedRef, d.formulaPricerSln, d.blackPricerSln, "Capped Ibor Coupon, ShiftedLN", 0.05);
    runTest(capped, cappedRef, d.formulaPricerN, d.blackPricerN, "Capped Ibor Coupon, Normal", 0.05);
}

BOOST_AUTO_TEST_CASE(testCappedCmsCoupon) {

    BOOST_TEST_MESSAGE("Testing formula based coupons against capped CMS coupon...");

    TestData d;

    auto cms10y = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, d.yts2, d.yts2);

    CompiledFormula formulaPlain = CompiledFormula(Size(0));                              // plain payoff
    CompiledFormula formulaCapped = min(CompiledFormula(Size(0)), CompiledFormula(0.03)); // capped cms coupon, at 0.03

    auto indexPlain =
        QuantLib::ext::make_shared<FormulaBasedIndex>("cms-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{cms10y},
                                              formulaPlain, cms10y->fixingCalendar());
    auto indexCapped =
        QuantLib::ext::make_shared<FormulaBasedIndex>("cms-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{cms10y},
                                              formulaCapped, cms10y->fixingCalendar());

    auto undRef = QuantLib::ext::make_shared<CmsCoupon>(Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                                Date(23, February, 2029), 2, cms10y, 1.0, 0.0, Date(), Date(),
                                                Actual360(), false);
    auto cappedRef = QuantLib::ext::make_shared<CappedFlooredCoupon>(undRef, 0.03);

    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> und(
        new QuantExt::FormulaBasedCoupon(EURCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexPlain, Date(), Date(), Actual360(), false));
    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> capped(
        new QuantExt::FormulaBasedCoupon(EURCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexCapped, Date(), Date(), Actual360(), false));

    runTest(und, undRef, d.formulaPricerLn, d.cmsPricerLn, "Plain CMS Coupon, Lognormal", 0.05);
    runTest(und, undRef, d.formulaPricerSln, d.cmsPricerSln, "Plain CMS Coupon, ShiftedLN", 0.05);
    runTest(und, undRef, d.formulaPricerN, d.cmsPricerN, "Plain CMS Coupon, Normal", 0.05);

    // the replication model and the model in the formula coupon pricer are not identical,
    // so we can not check for identical results; there should be reaosonably close though,
    // check for 2bp difference
    runTest(capped, cappedRef, d.formulaPricerLn, d.cmsPricerLn, "Capped CMS Coupon, Lognormal", 2.0);
    runTest(capped, cappedRef, d.formulaPricerSln, d.cmsPricerSln, "Capped CMS Coupon, ShiftedLN", 2.0);
    runTest(capped, cappedRef, d.formulaPricerN, d.cmsPricerN, "Capped CMS Coupon, Normal", 2.0);
}

BOOST_AUTO_TEST_CASE(testCappedCmsSpreadCoupon) {

    BOOST_TEST_MESSAGE("Testing formula based coupons against capped CMS spread coupon...");

    TestData d;

    auto cms2y = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(2 * Years, d.yts2, d.yts2);
    auto cms10y = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, d.yts2, d.yts2);
    auto cms10y2y = QuantLib::ext::make_shared<SwapSpreadIndex>("cms10y2y", cms10y, cms2y);

    CompiledFormula formulaPlain = CompiledFormula(Size(1)) - CompiledFormula(Size(0)); // plain payoff
    CompiledFormula formulaCapped = min(CompiledFormula(Size(1)) - CompiledFormula(Size(0)),
                                        CompiledFormula(0.03)); // capped cms sp coupon, at 0.03

    auto indexPlain = QuantLib::ext::make_shared<FormulaBasedIndex>(
        "cmssp-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{cms2y, cms10y}, formulaPlain,
        cms10y2y->fixingCalendar());
    auto indexCapped = QuantLib::ext::make_shared<FormulaBasedIndex>(
        "cmssp-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{cms2y, cms10y}, formulaCapped,
        cms10y2y->fixingCalendar());

    auto undRef = QuantLib::ext::make_shared<CmsSpreadCoupon>(Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                                      Date(23, February, 2029), 2, cms10y2y, 1.0, 0.0, Date(), Date(),
                                                      Actual360(), false);
    auto cappedRef = QuantLib::ext::make_shared<CappedFlooredCoupon>(undRef, 0.03);

    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> und(
        new QuantExt::FormulaBasedCoupon(EURCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexPlain, Date(), Date(), Actual360(), false));
    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> capped(
        new QuantExt::FormulaBasedCoupon(EURCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexCapped, Date(), Date(), Actual360(), false));

    runTest(und, undRef, d.formulaPricerLn, d.cmsspPricerLn, "Plain CmsSp Coupon, Lognormal", 0.05);
    runTest(und, undRef, d.formulaPricerSln, d.cmsspPricerSln, "Plain CmsSp Coupon, ShiftedLN", 0.05);
    runTest(und, undRef, d.formulaPricerN, d.cmsspPricerN, "Plain CmsSp Coupon, Normal", 0.05);

    runTest(capped, cappedRef, d.formulaPricerLn, d.cmsspPricerLn, "Capped CmsSp Coupon, Lognormal", 0.05);
    runTest(capped, cappedRef, d.formulaPricerSln, d.cmsspPricerSln, "Capped CmsSp Coupon, ShiftedLN", 0.05);
    runTest(capped, cappedRef, d.formulaPricerN, d.cmsspPricerN, "Capped CmsSp Coupon, Normal", 0.05);
}

BOOST_AUTO_TEST_CASE(testQuantoLiborCoupon) {

    BOOST_TEST_MESSAGE("Testing formula based coupons against (capped) Quanto Libor coupon...");

    TestData d;

    auto euribor6m = QuantLib::ext::make_shared<Euribor>(6 * Months, d.yts2);

    CompiledFormula formulaPlain = CompiledFormula(Size(0));                              // plain payoff
    CompiledFormula formulaCapped = min(CompiledFormula(Size(0)), CompiledFormula(0.03)); // capped ibor coupon, at 0.03

    auto indexPlain = QuantLib::ext::make_shared<FormulaBasedIndex>(
        "libor-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{euribor6m}, formulaPlain,
        euribor6m->fixingCalendar());
    auto indexCapped = QuantLib::ext::make_shared<FormulaBasedIndex>(
        "libor-family", std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>{euribor6m}, formulaCapped,
        euribor6m->fixingCalendar());

    auto undRef = QuantLib::ext::make_shared<IborCoupon>(Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                                 Date(23, February, 2029), 2, euribor6m, 1.0, 0.0, Date(), Date(),
                                                 Actual360(), false);
    auto cappedRef = QuantLib::ext::make_shared<CappedFlooredCoupon>(undRef, 0.03);

    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> und(
        new QuantExt::FormulaBasedCoupon(USDCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexPlain, Date(), Date(), Actual360(), false));
    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedCoupon> capped(
        new QuantExt::FormulaBasedCoupon(USDCurrency(), Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                         Date(23, February, 2029), 2, indexCapped, Date(), Date(), Actual360(), false));

    runTest(und, undRef, d.formulaPricerUSDLn, d.blackQuantoPricerLn, "Plain Quanto Ibor Coupon, Lognormal", 0.05);
    runTest(und, undRef, d.formulaPricerUSDSln, d.blackQuantoPricerSln, "Plain Quanto Ibor Coupon, ShiftedLN", 0.05);
    runTest(und, undRef, d.formulaPricerUSDN, d.blackQuantoPricerN, "Plain Quanto Ibor Coupon, Normal", 0.05);

    runTest(capped, cappedRef, d.formulaPricerUSDLn, d.blackQuantoPricerLn, "Capped Quanto Ibor Coupon, Lognormal",
            0.05);
    runTest(capped, cappedRef, d.formulaPricerUSDSln, d.blackQuantoPricerSln, "Capped Quanto Ibor Coupon, ShiftedLN",
            0.05);
    runTest(capped, cappedRef, d.formulaPricerUSDN, d.blackQuantoPricerN, "Capped Quanto Ibor Coupon, Normal", 0.05);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
