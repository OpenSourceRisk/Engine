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

#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <qle/termstructures/inflation/piecewisecpiinflationcurve.hpp>
#include <qle/utilities/inflation.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <qle/termstructures/inflation/inflationtraits.hpp>

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

QuantLib::ext::shared_ptr<YieldTermStructure> buildYts(double flatZeroRate, const DayCounter& dc) {
    return  QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), Handle<Quote>(ext::make_shared<SimpleQuote>(flatZeroRate)), dc);
}


QuantLib::ext::shared_ptr<ZeroInflationCurve>
buildZeroInflationCurve(CommonData& cd, bool useLastKnownFixing, const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
                        const bool isInterpolated, const QuantLib::ext::shared_ptr<Seasonality>& seasonality = nullptr) {
    Date today = Settings::instance().evaluationDate();
    double flatZero = 0.01;
    DayCounter dc = cd.dayCounter;
    Calendar fixingCalendar = NullCalendar();
    BusinessDayConvention bdc = ModifiedFollowing;

    auto discountTS = buildYts(flatZero, dc);

    std::vector<QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTraits::helper>> helpers;
    for (size_t i = 0; i < cd.zeroCouponQuotes.size(); ++i) {
        Date maturity = today + cd.zeroCouponPillars[i];
        Rate quote = cd.zeroCouponQuotes[i];
        QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTraits::helper> instrument =
            QuantLib::ext::make_shared<ZeroCouponInflationSwapHelper>(
                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(quote)), cd.obsLag, maturity, fixingCalendar, bdc,
                dc, index, isInterpolated ? CPI::Linear : CPI::Flat, Handle<YieldTermStructure>(discountTS), today);
        helpers.push_back(instrument);
    }
    Date baseDate =
        QuantExt::ZeroInflation::curveBaseDate(useLastKnownFixing, today, cd.obsLag, index->frequency(), index);
    QuantLib::ext::shared_ptr<ZeroInflationCurve> curve =
        QuantLib::ext::make_shared<QuantLib::PiecewiseZeroInflationCurve<Linear>>(today, baseDate, cd.obsLag, index->frequency(),
                                                                                  dc, helpers, seasonality, 1e-10);
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
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>();
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
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>();
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
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>();
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
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>();
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
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<EUHICPXT>();
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto seasonalityCurve = buildSeasonalityCurve();
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated,
                                         seasonalityCurve);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    std::vector<Date> expectedPillarDates{Date(1, July, 2022), Date(1, June, 2023), Date(1, June, 2024),
                                          Date(1, June, 2025), Date(1, June, 2027)};

    std::vector<Real> expectedZeroRates{-0.0543942497, 0.02097086546, 0.02068868041, 0.01710609424437, 0.01223686945};
    std::vector<Real> expectedZeroRatesWithoutSeasonality{-0.0543942497, -0.0543942497, -0.01603861959, -0.00711164972,
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

BOOST_AUTO_TEST_CASE(testPiecewiseInterpolatedCPICurve) {
    // try the Zero UK
    Calendar calendar = UnitedKingdom();
    BusinessDayConvention bdc = ModifiedFollowing;
    Date evaluationDate(13, August, 2007);
    evaluationDate = calendar.adjust(evaluationDate);
    Settings::instance().evaluationDate() = evaluationDate;

    // fixing data
    Date from(1, January, 2005);
    Date to(1, July, 2007);
    Schedule rpiSchedule = MakeSchedule().from(from).to(to).withFrequency(Monthly);

    std::vector<double> fixData{189.9, 189.9, 189.6, 190.5, 191.6, 192.0, 192.2, 192.2, 192.6, 193.1, 193.3,
                                193.6, 194.1, 193.4, 194.2, 195.0, 196.5, 197.7, 198.5, 198.5, 199.2, 200.1,
                                200.4, 201.1, 202.7, 201.6, 203.1, 204.4, 205.4, 206.2, 207.3};

    RelinkableHandle<ZeroInflationTermStructure> hz;
    auto ii = ext::make_shared<UKRPI>(hz);
    for (Size i = 0; i < fixData.size(); i++) {
        ii->addFixing(rpiSchedule[i], fixData[i]);
    }

    Handle<YieldTermStructure> nominalTS{buildYts(0.01, Actual365Fixed())};

    // now build the zero inflation curve
    std::vector<std::pair<Date, double>> zcData = {
        {Date(13, August, 2008), 2.93},  {Date(13, August, 2009), 2.95},  {Date(13, August, 2010), 2.965},
        {Date(15, August, 2011), 2.98},  {Date(13, August, 2012), 3.0},   {Date(13, August, 2014), 3.06},
        {Date(13, August, 2017), 3.175}, {Date(13, August, 2019), 3.243}, {Date(15, August, 2022), 3.293},
        {Date(14, August, 2027), 3.338}, {Date(13, August, 2032), 3.348}, {Date(15, August, 2037), 3.348},
        {Date(13, August, 2047), 3.308}, {Date(13, August, 2057), 3.228}};

    Period observationLag = Period(3, Months);
    DayCounter dc = Thirty360(Thirty360::BondBasis);
    Frequency frequency = Monthly;

    auto makeHelper = [&](const ext::shared_ptr<Quote>& quote, const Date& maturity) {
        return ext::make_shared<ZeroCouponInflationSwapHelper>(Handle<Quote>(quote), observationLag, maturity, calendar,
                                                               bdc, dc, ii, CPI::AsIndex, nominalTS);
    };

    std::vector<ext::shared_ptr<QuantExt::CPITraits::helper>> helpers;
    for (const auto& [maturity, rate] : zcData) {
        helpers.push_back(makeHelper(ext::make_shared<SimpleQuote>(rate / 100.0), maturity));
    }

    Date baseDate = ii->lastFixingDate();
    Rate baseCPI = ii->fixing(baseDate);

    ext::shared_ptr<QuantExt::PiecewiseCPIInflationCurve<Linear>> pZITS =
        ext::make_shared<QuantExt::PiecewiseCPIInflationCurve<Linear>>(evaluationDate, baseDate, baseCPI,
                                                                       observationLag, frequency, dc, helpers);
    hz.linkTo(pZITS);

    //===========================================================================================
    // first check that the quoted swaps are repriced correctly

    const Real eps = 1.0e-7;
    auto engine = ext::make_shared<DiscountingSwapEngine>(nominalTS);

    for (const auto& [date, rate] : zcData) {
        ZeroCouponInflationSwap nzcis(Swap::Payer, 1000000.0, evaluationDate, date, calendar, bdc, dc, rate / 100.0, ii,
                                      observationLag, CPI::AsIndex);
        nzcis.setPricingEngine(engine);

        BOOST_CHECK_MESSAGE(std::fabs(nzcis.NPV()) < eps, "zero-coupon inflation swap does not reprice to zero"
                                                              << "\n    NPV:      " << nzcis.NPV()
                                                              << "\n    maturity: " << nzcis.maturityDate()
                                                              << "\n    rate:     " << rate / 100.0);
    }

    //===========================================================================================
    // now test the forecasting capability of the index.

    from = hz->referenceDate();
    to = hz->maxDate() - 1 * Months; // a bit of margin for adjustments
    Schedule testIndex = MakeSchedule()
                             .from(from)
                             .to(to)
                             .withTenor(1 * Months)
                             .withCalendar(UnitedKingdom())
                             .withConvention(ModifiedFollowing);

    // we are testing UKRPI which is not interpolated
    Date bd = hz->baseDate();
    Real bf = ii->fixing(bd);
    for (const auto& d : testIndex) {
        Real z = hz->zeroRate(d, Period(0, Days));
        Real t = hz->dayCounter().yearFraction(bd, inflationPeriod(d, ii->frequency()).first);
        Real calc = bf * std::pow(1 + z, t);
        if (t <= 0)
            calc = ii->fixing(d, false); // still historical
        if (std::fabs(calc - ii->fixing(d, true)) > eps)
            BOOST_ERROR("inflation index does not forecast correctly"
                        << "\n    date:        " << d << "\n    base date:   " << bd << "\n    base fixing: " << bf
                        << "\n    expected:    " << calc << "\n    forecast:    " << ii->fixing(d, true));
    }

    //===========================================================================================
    // Add a seasonality correction.  The curve should recalculate and still reprice the swaps.

    Date nextBaseDate = inflationPeriod(hz->baseDate(), ii->frequency()).second;
    Date seasonalityBaseDate(31, January, nextBaseDate.year());
    vector<Rate> seasonalityFactors = {1.003245, 1.000000, 0.999715, 1.000495, 1.000929, 0.998687,
                                       0.995949, 0.994682, 0.995949, 1.000519, 1.003705, 1.004186};

    ext::shared_ptr<MultiplicativePriceSeasonality> nonUnitSeasonality =
        ext::make_shared<MultiplicativePriceSeasonality>(seasonalityBaseDate, Monthly, seasonalityFactors);

    pZITS->setSeasonality(nonUnitSeasonality);

    for (const auto& d : testIndex) {
        Real z = hz->zeroRate(d, Period(0, Days));
        Real t = hz->dayCounter().yearFraction(bd, inflationPeriod(d, ii->frequency()).first);
        Real calc = bf * std::pow(1 + z, t);
        if (t <= 0)
            calc = ii->fixing(d, false); // still historical
        if (std::fabs(calc - ii->fixing(d, true)) > eps)
            BOOST_ERROR("inflation index does not forecast correctly"
                        << "\n    date:        " << d << "\n    base date:   " << bd << "\n    base fixing: " << bf
                        << "\n    expected:    " << calc << "\n    forecast:    " << ii->fixing(d, true));
    }

    for (const auto& [date, rate] : zcData) {
        ZeroCouponInflationSwap nzcis(Swap::Payer, 1000000.0, evaluationDate, date, calendar, bdc, dc, rate / 100.0, ii,
                                      observationLag, CPI::AsIndex);
        nzcis.setPricingEngine(engine);

        BOOST_CHECK_MESSAGE(std::fabs(nzcis.NPV()) < eps, "zero-coupon inflation swap does not reprice to zero"
                                                              << "\n    NPV:      " << nzcis.NPV() << "\n    maturity: "
                                                              << nzcis.maturityDate() << "\n    rate:     " << rate);
    }

    // remove circular refernce
    hz.linkTo(ext::shared_ptr<ZeroInflationTermStructure>());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()