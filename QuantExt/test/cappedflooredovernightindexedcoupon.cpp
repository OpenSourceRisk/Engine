/*
 Copyright (C) 2026 Pengyi Peng
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License. You should have received a
 copy of the license along with this program.
*/

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>
#include <algorithm>

using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)
BOOST_AUTO_TEST_SUITE(CappedFlooredOvernightIndexCouponTests)

BOOST_AUTO_TEST_CASE(testShiftedCouponAmounts) {
    Settings::instance().evaluationDate() = Date(1, October, 2026);
    Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(
        Date(1, October, 2026), 0.04, Actual365Fixed()));
    auto index = ext::make_shared<Sofr>(curve);
    Handle<OptionletVolatilityStructure> vol(ext::make_shared<ConstantOptionletVolatility>(
        0, index->fixingCalendar(), Following, 0.0, Actual365Fixed(), Normal));
    auto pricer = ext::make_shared<QuantExt::BlackOvernightIndexedCouponPricer>(vol);
    const Date start(29, October, 2027), end(30, October, 2028), payment(1, November, 2028);
    const Real nominal = 50000000.0;

    for (bool observationShift : {false, true}) {
        for (bool includeSpread : {false, true}) {
            for (Spread spread : {0.0, 0.0095}) {
                for (Real gearing : {1.0, 2.0}) {
                    if (includeSpread && gearing != 1.0)
                        continue;
                    auto underlying = ext::make_shared<QuantExt::OvernightIndexedCoupon>(
                        payment, nominal, start, end, index, gearing, spread, Date(), Date(), Actual360(),
                        false, includeSpread, 2 * Days, 0, 0, Date(), Date(), observationShift);
                    const Real scale = nominal * underlying->accrualPeriod();
                    const Rate plainRate = underlying->amount() / scale;

                    QuantExt::CappedFlooredOvernightIndexedCoupon uncapped(underlying);
                    uncapped.setPricer(pricer);
                    BOOST_CHECK_SMALL(uncapped.amount() - underlying->amount(), 1e-7);

                    for (Rate strike : {0.0, 0.02, 0.06, 0.20}) {
                        QuantExt::CappedFlooredOvernightIndexedCoupon capped(underlying, strike);
                        capped.setPricer(pricer);
                        BOOST_CHECK_SMALL(capped.amount() - scale * std::min(plainRate, strike), 1e-7);

                        QuantExt::CappedFlooredOvernightIndexedCoupon floored(underlying, Null<Real>(), strike);
                        floored.setPricer(pricer);
                        BOOST_CHECK_SMALL(floored.amount() - scale * std::max(plainRate, strike), 1e-7);

                        QuantExt::CappedFlooredOvernightIndexedCoupon nakedFloor(underlying, Null<Real>(), strike, true);
                        nakedFloor.setPricer(pricer);
                        BOOST_CHECK_SMALL(floored.amount() - underlying->amount() - nakedFloor.amount(), 1e-7);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testWrapperAmountWithSeparatePeriodsAndHolidayDates) {
    Settings::instance().evaluationDate() = Date(1, October, 2025);
    Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(
        Date(1, October, 2025), 0.04, Actual365Fixed()));
    auto index = ext::make_shared<Sofr>(curve);
    Handle<OptionletVolatilityStructure> vol(ext::make_shared<ConstantOptionletVolatility>(
        0, index->fixingCalendar(), Following, 0.008, Actual365Fixed(), Normal));
    auto pricer = ext::make_shared<QuantExt::BlackOvernightIndexedCouponPricer>(vol);
    const Date start(1, November, 2025), end(30, November, 2025), payment(2, December, 2025);

    for (bool separatePeriod : {false, true}) {
        for (bool observationShift : {false, true}) {
            for (bool telescopic : {false, true}) {
                for (Real nominal : {0.0, 50000000.0}) {
                    auto underlying = ext::make_shared<QuantExt::OvernightIndexedCoupon>(
                        payment, nominal, start, end, index, 1.0, 0.0095, Date(), Date(), Actual360(),
                        telescopic, false, 2 * Days, 0, 0,
                        separatePeriod ? start - 7 : Date(), separatePeriod ? end - 7 : Date(), observationShift);
                    QuantExt::CappedFlooredOvernightIndexedCoupon wrapped(underlying);
                    wrapped.setPricer(pricer);
                    BOOST_CHECK_SMALL(wrapped.amount() - underlying->amount(), 1e-7);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testShiftedCouponOptionDecomposition) {
    Settings::instance().evaluationDate() = Date(1, October, 2026);
    Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(
        Date(1, October, 2026), 0.04, Actual365Fixed()));
    auto index = ext::make_shared<Sofr>(curve);
    Handle<OptionletVolatilityStructure> vol(ext::make_shared<ConstantOptionletVolatility>(
        0, index->fixingCalendar(), Following, 0.008, Actual365Fixed(), Normal));
    auto pricer = ext::make_shared<QuantExt::BlackOvernightIndexedCouponPricer>(vol);
    const Date start(29, October, 2027), end(30, October, 2028), payment(1, November, 2028);
    auto underlying = ext::make_shared<QuantExt::OvernightIndexedCoupon>(
        payment, 50000000.0, start, end, index, 1.0, 0.0095, Date(), Date(), Actual360(),
        false, false, 2 * Days, 0, 0, Date(), Date(), true);

    for (Rate strike : {0.0, 0.04, 0.10}) {
        QuantExt::CappedFlooredOvernightIndexedCoupon floored(underlying, Null<Real>(), strike);
        QuantExt::CappedFlooredOvernightIndexedCoupon nakedFloor(underlying, Null<Real>(), strike, true);
        QuantExt::CappedFlooredOvernightIndexedCoupon capped(underlying, strike);
        QuantExt::CappedFlooredOvernightIndexedCoupon nakedCap(underlying, strike, Null<Real>(), true);
        floored.setPricer(pricer);
        nakedFloor.setPricer(pricer);
        capped.setPricer(pricer);
        nakedCap.setPricer(pricer);
        BOOST_CHECK_SMALL(floored.amount() - underlying->amount() - nakedFloor.amount(), 1e-7);
        BOOST_CHECK_SMALL(capped.amount() + nakedCap.amount() - underlying->amount(), 1e-7);
        BOOST_CHECK_SMALL(floored.accruedAmount(end) - floored.amount(), 1e-7);
    }
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
