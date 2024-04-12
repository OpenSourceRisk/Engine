/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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
#include <boost/test/unit_test.hpp>

#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/termstructures/inflation/piecewisezeroinflationcurve.hpp>

#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/utilities/inflation.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace std;

namespace {

struct CommonData {

    Date today;
    Real tolerance;
    DayCounter dayCounter;
    std::vector<Period> zeroCouponPillars;
    std::vector<Rate> zeroCouponQuotes;
    std::map<Date, Rate> cpiFixings;
    Period obsLag;

    CommonData()
        : today(15, Aug, 2022), tolerance(1e-6), dayCounter(Actual365Fixed()),
          zeroCouponPillars({1 * Years, 2 * Years, 3 * Years, 5 * Years}), zeroCouponQuotes({0.06, 0.04, 0.03, 0.02}),
          cpiFixings({{Date(1, May, 2022), 98.}, {Date(1, June, 2022), 100.}, {Date(1, July, 2022), 104.}}),
          obsLag(2, Months){};
};

void addFixings(const std::map<Date, Rate> fixings, ZeroInflationIndex& index) {
    index.clearFixings();
    for (const auto& fixing : fixings) {
        index.addFixing(fixing.first, fixing.second, true);
    }
};

QuantLib::ext::shared_ptr<Seasonality> buildSeasonalityCurve() {
    std::vector<double> factors{0.99, 1.01, 0.98, 1.02, 0.97, 1.03, 0.96, 1.04, 0.95, 1.05, 0.94, 1.06};
    Date seasonalityBaseDate(1, Jan, 2022);
    return QuantLib::ext::make_shared<MultiplicativePriceSeasonality>(seasonalityBaseDate, Monthly, factors);
}

QuantLib::ext::shared_ptr<ZeroInflationCurve>
buildZeroInflationCurve(CommonData& cd, bool useLastKnownFixing, const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
                        const bool isInterpolated, const QuantLib::ext::shared_ptr<Seasonality>& seasonality = nullptr) {
    Date today = Settings::instance().evaluationDate();
    QuantLib::ext::shared_ptr<SimpleQuote> flatZero = QuantLib::ext::make_shared<SimpleQuote>(0.01);
    DayCounter dc = cd.dayCounter;
    Calendar fixingCalendar = NullCalendar();
    BusinessDayConvention bdc = ModifiedFollowing;

    QuantLib::ext::shared_ptr<YieldTermStructure> discountTS =
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), Handle<Quote>(flatZero), dc);

    std::vector<QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper>> helpers;
    for (size_t i = 0; i < cd.zeroCouponQuotes.size(); ++i) {
        Date maturity = today + cd.zeroCouponPillars[i];
        Rate quote = cd.zeroCouponQuotes[i];
        QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper> instrument =
            QuantLib::ext::make_shared<ZeroCouponInflationSwapHelper>(
                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(quote)), cd.obsLag, maturity, fixingCalendar, bdc, dc,
                index, isInterpolated ? CPI::Linear : CPI::Flat, Handle<YieldTermStructure>(discountTS), today);
        helpers.push_back(instrument);
    }
    Rate baseRate = QuantExt::ZeroInflation::guessCurveBaseRate(useLastKnownFixing, today, today, cd.zeroCouponPillars[0],
                                                                cd.dayCounter, cd.obsLag, cd.zeroCouponQuotes[0],
                                                                cd.obsLag, cd.dayCounter, index, isInterpolated);
    QuantLib::ext::shared_ptr<ZeroInflationCurve> curve = QuantLib::ext::make_shared<QuantExt::PiecewiseZeroInflationCurve<Linear>>(
        today, fixingCalendar, dc, cd.obsLag, index->frequency(), baseRate, helpers, 1e-10, index, useLastKnownFixing);
    if (seasonality) {
        curve->setSeasonality(seasonality);
    }
    return curve;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InflationCurveTest)

BOOST_AUTO_TEST_CASE(testZeroInflationCurveNonInterpolatedLastMonthFixingUnknown) {

    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = false;
    // Build Curve and Index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>(false);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, June, 2022), Date(1, June, 2023), Date(1, June, 2024),
                                          Date(1, June, 2025), Date(1, June, 2027)};

    std::vector<Real> expectedZeroRates{0.06, 0.06, 0.04, 0.03, 0.02};

    std::vector<Real> expectedCPIs{100., 106., 108.171622850024, 109.281549591561, 110.414070537467};

    BOOST_CHECK_EQUAL(curve->baseDate(), expectedPillarDates.front());

    BOOST_CHECK_EQUAL(curve->dates().size(), expectedPillarDates.size());

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        BOOST_CHECK_EQUAL(curve->dates().at(i), expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(curve->zeroRate(curve->dates().at(i), 0 * Days), expectedZeroRates.at(i), cd.tolerance);
    }

    // Check index fixing forecasts

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        auto forwardCPI = index->fixing(expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(forwardCPI, expectedCPIs.at(i), cd.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testZeroInflationCurveNonInterpolatedLastMonthFixing) {

    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = true;
    // Build Curve and Index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>(false);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, July, 2022), Date(1, June, 2023), Date(1, June, 2024),
                                          Date(1, June, 2025), Date(1, June, 2027)};

    std::vector<Real> expectedZeroRates{0.02097086546, 0.02097086546, 0.02068868041, 0.01710609424437, 0.01223686945};

    std::vector<Real> expectedCPIs{104, 106., 108.171622850024, 109.281549591561, 110.414070537467};

    BOOST_CHECK_EQUAL(curve->baseDate(), expectedPillarDates.front());

    BOOST_CHECK_EQUAL(curve->dates().size(), expectedPillarDates.size());

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        BOOST_CHECK_EQUAL(curve->dates().at(i), expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(curve->zeroRate(curve->dates().at(i), 0 * Days), expectedZeroRates.at(i), cd.tolerance);
    }

    // Check index fixing forecasts

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        auto forwardCPI = index->fixing(expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(forwardCPI, expectedCPIs.at(i), cd.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testZeroInflationCurveInterpolatedLastMonthFixing) {

    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    bool isInterpolated = true;
    bool useLastKnownFixingDateAsBaseDate = true;
    // Build Curve and Index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>(false);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, July, 2022), Date(1, July, 2023), Date(1, July, 2024),
                                          Date(1, July, 2025), Date(1, July, 2027)};

    std::vector<Real> expectedZeroRates{0.03945267289772, 0.03945267289772, 0.02921461897637, 0.02277721089513,
                                        0.01564691567};

    std::vector<Date> fixingDates{Date(15, June, 2022), Date(15, June, 2023), Date(15, June, 2024),
                                  Date(15, June, 2025), Date(15, June, 2027)};

    // Base CPI is 100 + (104-100)*14/31, and then for the forward cpi it is baseCPI * (1+r)^T
    std::vector<Real> expectedCPIs{101.806451613, 107.914838710, 110.125690876, 111.255667907, 112.408647296};

    BOOST_CHECK_EQUAL(curve->baseDate(), expectedPillarDates.front());

    BOOST_CHECK_EQUAL(curve->dates().size(), expectedPillarDates.size());

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        BOOST_CHECK_EQUAL(curve->dates().at(i), expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(curve->zeroRate(curve->dates().at(i), 0 * Days), expectedZeroRates.at(i), 1e-6);
    }

    // Check index fixing forecasts

    for (size_t i = 0; i < fixingDates.size(); ++i) {
        Date fixDate1 = inflationPeriod(fixingDates.at(i), index->frequency()).first;
        Date fixDate2 = inflationPeriod(fixingDates.at(i), index->frequency()).second + 1 * Days;
        Rate cpi1 = index->fixing(fixDate1);
        Rate cpi2 = index->fixing(fixDate2);
        auto forwardCPI = cpi1 + (cpi2 - cpi1) * 14 / 31;
        BOOST_CHECK_CLOSE(forwardCPI, expectedCPIs.at(i), cd.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testZeroInflationCurveNonInterpolatedLastMonthFixingUnknownWithSeasonality) {

    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = false;
    // Build Curve and Index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>(false);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto seasonalityCurve = buildSeasonalityCurve();
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated,
                                         seasonalityCurve);
    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, June, 2022), Date(1, June, 2023), Date(1, June, 2024),
                                          Date(1, June, 2025), Date(1, June, 2027)};

    std::vector<Real> expectedZeroRates{0.06, 0.06, 0.04, 0.03, 0.02};

    std::vector<Real> expectedCPIs{100., 106., 108.171622850024, 109.281549591561, 110.414070537467};

    BOOST_CHECK_EQUAL(curve->baseDate(), expectedPillarDates.front());

    BOOST_CHECK_EQUAL(curve->dates().size(), expectedPillarDates.size());

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        BOOST_CHECK_EQUAL(curve->dates().at(i), expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(curve->zeroRate(curve->dates().at(i), 0 * Days), expectedZeroRates.at(i), cd.tolerance);
        BOOST_CHECK_CLOSE(curve->data().at(i), expectedZeroRates.at(i), cd.tolerance);
    }

    // Check index fixing forecasts

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        auto forwardCPI = index->fixing(expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(forwardCPI, expectedCPIs.at(i), cd.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testZeroInflationCurveNonInterpolatedLastMonthFixingWithSeasonality) {

    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = true;
    // Build Curve and Index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>(false);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto seasonalityCurve = buildSeasonalityCurve();
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated,
                                         seasonalityCurve);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, July, 2022), Date(1, June, 2023), Date(1, June, 2024),
                                          Date(1, June, 2025), Date(1, June, 2027)};

    std::vector<Real> expectedZeroRates{0.02097086546, 0.02097086546, 0.02068868041, 0.01710609424437, 0.01223686945};
    std::vector<Real> expectedZeroRatesWithoutSeasonality{0.02097086546, -0.05439424967, -0.01603861959, -0.00711164972,
                                                          -0.00213855283};
    std::vector<Real> expectedCPIs{104, 106., 108.171622850024, 109.281549591561, 110.414070537467};

    BOOST_CHECK_EQUAL(curve->baseDate(), expectedPillarDates.front());

    BOOST_CHECK_EQUAL(curve->dates().size(), expectedPillarDates.size());

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        BOOST_CHECK_EQUAL(curve->dates().at(i), expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(curve->data().at(i), expectedZeroRatesWithoutSeasonality.at(i), cd.tolerance);
        BOOST_CHECK_CLOSE(curve->zeroRate(curve->dates().at(i), 0 * Days), expectedZeroRates.at(i), cd.tolerance);
    }
    // Check index fixing forecasts

    for (size_t i = 0; i < expectedPillarDates.size(); ++i) {
        auto forwardCPI = index->fixing(expectedPillarDates.at(i));
        BOOST_CHECK_CLOSE(forwardCPI, expectedCPIs.at(i), cd.tolerance);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()