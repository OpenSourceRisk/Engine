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
#include <boost/assign/list_of.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/math/comparison.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>

using QuantExt::DiscountRatioModifiedCurve;
using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace boost::assign;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DiscountingRatioModifiedCurveTest)

BOOST_AUTO_TEST_CASE(testStandardCurves) {

    BOOST_TEST_MESSAGE("Testing discount ratio modified curve with some standard curves");

    SavedSettings backup;

    Date today(15, Aug, 2018);
    Settings::instance().evaluationDate() = today;

    Actual365Fixed dc;

    // Base curve with fixed reference date of 15th Aug 2018
    vector<Date> baseDates = list_of(today)(today + 1 * Years);
    vector<Real> baseDfs = list_of(1.0)(0.98);
    Handle<YieldTermStructure> baseCurve(QuantLib::ext::make_shared<DiscountCurve>(baseDates, baseDfs, dc));
    baseCurve->enableExtrapolation();

    // Numerator curve with fixed reference date of 15th Aug 2018
    vector<Date> numDates = list_of(today)(today + 1 * Years)(today + 2 * Years);
    vector<Real> numZeroes = list_of(0.025)(0.025)(0.026);
    Handle<YieldTermStructure> numCurve(QuantLib::ext::make_shared<ZeroCurve>(numDates, numZeroes, dc));
    numCurve->enableExtrapolation();

    // Denominator curve with floating reference date
    Handle<YieldTermStructure> denCurve(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, dc));

    DiscountRatioModifiedCurve curve(baseCurve, numCurve, denCurve);

    Date discountDate = today + 18 * Months;
    BOOST_CHECK(
        close(curve.discount(discountDate),
              baseCurve->discount(discountDate) * numCurve->discount(discountDate) / denCurve->discount(discountDate)));

    discountDate = today + 3 * Years;
    BOOST_CHECK(
        close(curve.discount(discountDate),
              baseCurve->discount(discountDate) * numCurve->discount(discountDate) / denCurve->discount(discountDate)));

    // When we change evaluation date, we may not get what we expect here because reference date is taken from the
    // base curve which has been set up here with a fixed reference date. However, the denominator curve has been
    // set up here with a floating reference date. See the warning in the ctor of DiscountRatioModifiedCurve
    Settings::instance().evaluationDate() = today + 3 * Months;
    BOOST_TEST_MESSAGE("Changed evaluation date to " << Settings::instance().evaluationDate());

    BOOST_CHECK(
        !close(curve.discount(discountDate), baseCurve->discount(discountDate) * numCurve->discount(discountDate) /
                                                 denCurve->discount(discountDate)));
    Time t = dc.yearFraction(curve.referenceDate(), discountDate);
    BOOST_CHECK(close(curve.discount(discountDate),
                      baseCurve->discount(discountDate) * numCurve->discount(discountDate) / denCurve->discount(t)));
}

BOOST_AUTO_TEST_CASE(testExtrapolationSettings) {

    BOOST_TEST_MESSAGE("Testing extrapolation settings for discount ratio modified curve");

    SavedSettings backup;

    Date today(15, Aug, 2018);
    Settings::instance().evaluationDate() = today;

    Actual365Fixed dc;

    // Base curve with fixed reference date of 15th Aug 2018
    vector<Date> baseDates = list_of(today)(Date(15, Aug, 2019));
    vector<Real> baseDfs = list_of(1.0)(0.98);
    Handle<YieldTermStructure> baseCurve(QuantLib::ext::make_shared<DiscountCurve>(baseDates, baseDfs, dc));

    // Numerator curve with fixed reference date of 15th Aug 2018
    vector<Date> numDates = list_of(today)(Date(15, Aug, 2019))(Date(15, Aug, 2020));
    vector<Real> numZeroes = list_of(0.025)(0.025)(0.026);
    Handle<YieldTermStructure> numCurve(QuantLib::ext::make_shared<ZeroCurve>(numDates, numZeroes, dc));

    // Denominator curve with floating reference date
    Handle<YieldTermStructure> denCurve(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, dc));

    // Create the discount ratio curve
    DiscountRatioModifiedCurve curve(baseCurve, numCurve, denCurve);

    // Extrapolation is always true
    BOOST_CHECK(curve.allowsExtrapolation());

    // Max date is maximum possible date
    BOOST_CHECK_EQUAL(curve.maxDate(), Date::maxDate());

    // Extrapolation is determined by underlying curves
    BOOST_CHECK_NO_THROW(curve.discount(Date(15, Aug, 2019)));
    BOOST_CHECK_THROW(curve.discount(Date(15, Aug, 2019) + 1 * Days), QuantLib::Error);
    baseCurve->enableExtrapolation();
    BOOST_CHECK_NO_THROW(curve.discount(Date(15, Aug, 2019) + 1 * Days));
    BOOST_CHECK_THROW(curve.discount(Date(15, Aug, 2020) + 1 * Days), QuantLib::Error);
    numCurve->enableExtrapolation();
    BOOST_CHECK_NO_THROW(curve.discount(Date(15, Aug, 2020) + 1 * Days));
}

BOOST_AUTO_TEST_CASE(testConstructionNullUnderlyingCurvesThrow) {

    BOOST_TEST_MESSAGE("Testing construction with null underlying curves throw");

    QuantLib::ext::shared_ptr<DiscountRatioModifiedCurve> curve;

    // All empty handles throw
    Handle<YieldTermStructure> baseCurve;
    Handle<YieldTermStructure> numCurve;
    Handle<YieldTermStructure> denCurve;
    BOOST_CHECK_THROW(curve = QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(baseCurve, numCurve, denCurve),
                      QuantLib::Error);

    // Numerator and denominator empty handles throw
    Handle<YieldTermStructure> baseCurve_1(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, Actual365Fixed()));
    BOOST_CHECK_THROW(curve = QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(baseCurve_1, numCurve, denCurve),
                      QuantLib::Error);

    // Denominator empty handles throw
    Handle<YieldTermStructure> numCurve_1(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, Actual365Fixed()));
    BOOST_CHECK_THROW(curve = QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(baseCurve_1, numCurve_1, denCurve),
                      QuantLib::Error);

    // No empty handles succeeds
    Handle<YieldTermStructure> denCurve_1(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, Actual365Fixed()));
    BOOST_CHECK_NO_THROW(curve = QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(baseCurve_1, numCurve_1, denCurve_1));
}

BOOST_AUTO_TEST_CASE(testLinkingNullUnderlyingCurvesThrow) {

    BOOST_TEST_MESSAGE("Testing that linking with null underlying curves throw");

    QuantLib::ext::shared_ptr<DiscountRatioModifiedCurve> curve;

    // All empty handles throw
    RelinkableHandle<YieldTermStructure> baseCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, Actual365Fixed()));
    RelinkableHandle<YieldTermStructure> numCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, Actual365Fixed()));
    RelinkableHandle<YieldTermStructure> denCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0255, Actual365Fixed()));

    // Curve building succeeds since no empty handles
    BOOST_CHECK_NO_THROW(curve = QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(baseCurve, numCurve, denCurve));

    // Switching base curve to empty handle should give a failure
    BOOST_CHECK_THROW(baseCurve.linkTo(QuantLib::ext::shared_ptr<YieldTermStructure>()), QuantLib::Error);
    BOOST_CHECK_NO_THROW(baseCurve.linkTo(*numCurve));

    // Switching numerator curve to empty handle should give a failure
    BOOST_CHECK_THROW(numCurve.linkTo(QuantLib::ext::shared_ptr<YieldTermStructure>()), QuantLib::Error);
    BOOST_CHECK_NO_THROW(numCurve.linkTo(*denCurve));

    // Switching denominator curve to empty handle should give a failure
    BOOST_CHECK_THROW(denCurve.linkTo(QuantLib::ext::shared_ptr<YieldTermStructure>()), QuantLib::Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
