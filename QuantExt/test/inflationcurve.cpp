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
#include <qle/termstructures/piecewisezeroinflationcurve.hpp>
//#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>

#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

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
        : today(15, Aug, 2022), tolerance(1e-8), dayCounter(Actual365Fixed()), 
          zeroCouponPillars({1 * Years, 2 * Years, 3 * Years, 5 * Years}), zeroCouponQuotes({0.06, 0.04, 0.03, 0.02}),
          cpiFixings({{Date(1, June, 2022), 100.}, {Date(1, July, 2022), 104.}}), obsLag(2, Months){};
};


void addFixings(const std::map<Date, Rate> fixings, ZeroInflationIndex& index) {
    index.clearFixings();
    for (const auto& fixing : fixings) {
        index.addFixing(fixing.first, fixing.second, true);
    }
};

boost::shared_ptr<ZeroInflationCurve> buildZeroInflationCurve(CommonData & cd, bool useLastKnownFixing,
                                                              const boost::shared_ptr<ZeroInflationIndex>& index) {
    Date today = Settings::instance().evaluationDate();
    boost::shared_ptr<SimpleQuote> flatZero = boost::make_shared<SimpleQuote>(0.01);
    DayCounter dc = cd.dayCounter;
    Calendar fixingCalendar = NullCalendar();
    BusinessDayConvention bdc = ModifiedFollowing;

    boost::shared_ptr<YieldTermStructure> discountTS =
        boost::make_shared<FlatForward>(0, NullCalendar(), Handle<Quote>(flatZero), dc);

    std::vector<boost::shared_ptr<QuantExt::ZeroInflationTraits::helper>> helpers;
    for (size_t i = 0; i < cd.zeroCouponQuotes.size(); ++i) {
        Date maturity = today + cd.zeroCouponPillars[i];
        Rate quote = cd.zeroCouponQuotes[i];
        boost::shared_ptr<QuantExt::ZeroInflationTraits::helper> instrument =
            boost::make_shared<ZeroCouponInflationSwapHelper>(
                Handle<Quote>(boost::make_shared<SimpleQuote>(quote)), cd.obsLag, maturity, fixingCalendar, bdc,
                dc, index, CPI::AsIndex, Handle<YieldTermStructure>(discountTS), today);
        helpers.push_back(instrument);
    }
    if (useLastKnownFixing) {
        return boost::make_shared<QuantExt::PiecewiseZeroInflationCurve<Linear>>(
            today, fixingCalendar, dc, cd.obsLag, index->frequency(), cd.zeroCouponQuotes.front(), helpers, index,
            1e-10);
    } else {
        return boost::make_shared<QuantExt::PiecewiseZeroInflationCurve<Linear>>(
            today, fixingCalendar, dc, cd.obsLag, index->frequency(), cd.zeroCouponQuotes.front(), helpers, nullptr,
            1e-10);
    }      
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InflationCurveTest)

BOOST_AUTO_TEST_CASE(testZeroInflationCurveNonInterpolatedLastMonthFixingUnknown) {

    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    Date today = Settings::instance().evaluationDate();
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = false;
    //Build Curve and Index
    boost::shared_ptr<ZeroInflationIndex> curveBuildIndex = boost::make_shared<EUHICPXT>(isInterpolated);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex);
    
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
    Date today = Settings::instance().evaluationDate();
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = true;
    // Build Curve and Index
    boost::shared_ptr<ZeroInflationIndex> curveBuildIndex = boost::make_shared<EUHICPXT>(isInterpolated);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, July, 2022), Date(1, June, 2023), Date(1, June, 2024),
                                          Date(1, June, 2025), Date(1, June, 2027)};

    std::vector<Real> expectedZeroRates{0.06, 0.02097086546, 0.02068868041, 0.01710609424437, 0.01223686945};

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
    Date today = Settings::instance().evaluationDate();
    bool isInterpolated = true;
    bool useLastKnownFixingDateAsBaseDate = true;
    // Build Curve and Index
    boost::shared_ptr<ZeroInflationIndex> curveBuildIndex = boost::make_shared<EUHICPXT>(isInterpolated);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, July, 2022), Date(1, July, 2023), Date(1, July, 2024),
                                          Date(1, July, 2025), Date(1, July, 2027)};

    std::vector<Real> expectedZeroRates{0.06, 0.03852371519493, 0.02925743918788, 0.02277521712975, 0.01564696128};


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
        auto forwardCPI = index->fixing(fixingDates.at(i));
        BOOST_CHECK_CLOSE(forwardCPI, expectedCPIs.at(i), cd.tolerance);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()