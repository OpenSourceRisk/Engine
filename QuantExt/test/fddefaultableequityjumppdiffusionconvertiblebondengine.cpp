/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>
#include <qle/pricingengines/fddefaultableequityjumpdiffusionconvertiblebondengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/instruments/bonds/fixedratebond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include "toplevelfixture.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <iomanip>

using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FdConvertibleBondEngine)

BOOST_AUTO_TEST_CASE(test_vanilla_bond) {

    BOOST_TEST_MESSAGE("Test vanilla bond pricing in fd defaultable equity jump diffusion convertible engine...");

    Date today(9, February, 2021);
    Settings::instance().evaluationDate() = today;

    Real S0 = 100.0;
    Handle<YieldTermStructure> rate(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> dividend(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Handle<BlackVolTermStructure> vol(QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), 0.3, Actual365Fixed()));

    Handle<YieldTermStructure> bondBenchmark(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.03, Actual365Fixed()));
    Handle<DefaultProbabilityTermStructure> creditCurve(
        QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0050, Actual365Fixed()));
    Handle<Quote> bondRecoveryRate(QuantLib::ext::make_shared<SimpleQuote>(0.25));
    Handle<Quote> securitySpread(QuantLib::ext::make_shared<SimpleQuote>(0.00));

    auto equity = QuantLib::ext::make_shared<EquityIndex2>("myEqIndex", NullCalendar(), EURCurrency(),
                                                  Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(S0)), rate, dividend);

    auto bond = QuantLib::ext::make_shared<FixedRateBond>(0, TARGET(), 100000.0, today, today + 5 * Years, 1 * Years,
                                                  std::vector<Real>(1, 0.05), Thirty360(Thirty360::BondBasis));

    // vanilla pricing

    auto vanillaEngine = QuantLib::ext::make_shared<DiscountingRiskyBondEngine>(bondBenchmark, creditCurve, bondRecoveryRate,
                                                                        securitySpread, 1 * Years);
    bond->setPricingEngine(vanillaEngine);
    Real vanillaEngineNpv = bond->NPV();

    // jd model

    Real p = 0.0;
    Real eta = 1.0;
    std::vector<Real> stepTimes = {1.0, 2.0, 3.0, 4.0, 5.0};

    auto modelBuilder = QuantLib::ext::make_shared<DefaultableEquityJumpDiffusionModelBuilder>(
        stepTimes, equity, vol, creditCurve, p, eta, false, 24, 400, 1E-5, 1.5, Null<Real>(),
        DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Simultaneously, true);
    auto model = modelBuilder->model();

    auto cpns = bond->cashflows();
    cpns.erase(
        std::remove_if(cpns.begin(), cpns.end(),
                       [](QuantLib::ext::shared_ptr<CashFlow> c) { return QuantLib::ext::dynamic_pointer_cast<Coupon>(c) == nullptr; }),
        cpns.end());

    ConvertibleBond2::MakeWholeData d; // avoid linker problem on linux
    auto convertibleBond =
        QuantLib::ext::make_shared<ConvertibleBond2>(bond->settlementDays(), bond->calendar(), bond->issueDate(), cpns);
    auto convertibleEngine = QuantLib::ext::make_shared<FdDefaultableEquityJumpDiffusionConvertibleBondEngine>(
        model, bondBenchmark, securitySpread, Handle<DefaultProbabilityTermStructure>(), bondRecoveryRate,
        Handle<FxIndex>(), false, 24, 100, 1E-4, 1.5);
    convertibleBond->setPricingEngine(convertibleEngine);
    Real convertibleEngineNpv = convertibleBond->NPV();

    BOOST_TEST_MESSAGE("Vanilla Engine Bond NPV = "
                       << std::setprecision(10) << vanillaEngineNpv
                       << ", Convertible Engine Bond NPV = " << convertibleEngineNpv
                       << ", error=" << (convertibleEngineNpv - vanillaEngineNpv) / vanillaEngineNpv * 100.0 << "%");

    BOOST_CHECK_CLOSE(vanillaEngineNpv, convertibleEngineNpv, 1E-3);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
