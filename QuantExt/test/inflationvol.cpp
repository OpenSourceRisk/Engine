/*
 Copyright (C) 2022 Quaternion Risk Management Ltd

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

#include <ql/indexes/inflation/aucpi.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/termstructures/inflation/cpipricevolatilitysurface.hpp>
#include <qle/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>
#include <qle/utilities/inflation.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace std;

namespace {

struct CommonData {

    Date today;
    Real tolerance;
    DayCounter dayCounter;
    Calendar fixingCalendar;
    BusinessDayConvention bdc;
    Period obsLag;

    std::vector<Period> zeroCouponPillars{1 * Years, 2 * Years, 3 * Years, 5 * Years};
    std::vector<Rate> zeroCouponQuotes{0.06, 0.04, 0.03, 0.02};

    QuantLib::ext::shared_ptr<SimpleQuote> flatZero = QuantLib::ext::make_shared<SimpleQuote>(0.01);

    Handle<YieldTermStructure> discountTS;

    std::map<Date, Rate> cpiFixings{{Date(1, May, 2021), 97.8744653499849},
                                    {Date(1, Jun, 2021), 98.0392156862745},
                                    {Date(1, Jul, 2021), 98.1989155376188},
                                    {Date(1, Aug, 2021), 98.3642120151039},
                                    {Date(1, Sep, 2021), 98.5297867331921},
                                    {Date(1, Oct, 2021), 98.6902856945937},
                                    {Date(1, Nov, 2021), 98.8564092866721},
                                    {Date(1, Dec, 2021), 99.0174402961208},
                                    {Date(1, Jan, 2022), 99.1841145816863},
                                    {Date(1, Feb, 2022), 99.3510694270946},
                                    {Date(1, Mar, 2022), 99.5021088919576},
                                    {Date(1, Apr, 2022), 99.6695990114986},
                                    {Date(1, May, 2022), 99.8319546569845},
                                    {Date(1, Jun, 2022), 100},
                                    {Date(1, July, 2022), 104}};

    std::vector<Rate> strikes{0.02, 0.04, 0.06, 0.08};
    std::vector<Period> tenors{1 * Years, 2 * Years, 3 * Years};

    vector<vector<Handle<Quote>>> vols{
        {Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.3)), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.32)),
         Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.34)), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.36))},
        {Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.35)), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.37)),
         Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.39)), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.41))},
        {Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.40)), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.42)),
         Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.44)), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.46))}};

    CommonData()
        : today(15, Aug, 2022), tolerance(1e-6), dayCounter(Actual365Fixed()), fixingCalendar(NullCalendar()),
          bdc(ModifiedFollowing), obsLag(2, Months),
          discountTS(Handle<YieldTermStructure>(
              QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), Handle<Quote>(flatZero), dayCounter))) {
        Settings::instance().evaluationDate() = today;
    };
};

struct CPICapFloorPriceData {
    std::vector<Period> tenors;
    std::vector<double> cStrikes;
    std::vector<double> fStrikes;
    QuantLib::Matrix cPrices;
    QuantLib::Matrix fPrices;
};

QuantLib::ext::shared_ptr<ZeroInflationCurve>
buildZeroInflationCurve(CommonData& cd, bool useLastKnownFixing, const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
                        const bool isInterpolated, const QuantLib::ext::shared_ptr<Seasonality>& seasonality = nullptr,
                        const QuantLib::Date& startDate = Date()) {
    Date today = Settings::instance().evaluationDate();
    Date start = startDate;
    if (startDate == Date()) {
        start = today;
    }
    DayCounter dc = cd.dayCounter;

    BusinessDayConvention bdc = ModifiedFollowing;

    std::vector<QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper>> helpers;
    for (size_t i = 0; i < cd.zeroCouponQuotes.size(); ++i) {
        Date maturity = start + cd.zeroCouponPillars[i];
        Rate quote = cd.zeroCouponQuotes[i];
        QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper> instrument =
            QuantLib::ext::make_shared<ZeroCouponInflationSwapHelper>(
                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(quote)), cd.obsLag, maturity, cd.fixingCalendar, bdc, dc,
                index, isInterpolated ? CPI::Linear : CPI::Flat, Handle<YieldTermStructure>(cd.discountTS), start);
        helpers.push_back(instrument);
    }
    Rate baseRate = QuantExt::ZeroInflation::guessCurveBaseRate(useLastKnownFixing, start, today, cd.zeroCouponPillars[0],
                                                                cd.dayCounter, cd.obsLag, cd.zeroCouponQuotes[0],
                                                                cd.obsLag, cd.dayCounter, index, isInterpolated);
    QuantLib::ext::shared_ptr<ZeroInflationCurve> curve = QuantLib::ext::make_shared<QuantExt::PiecewiseZeroInflationCurve<Linear>>(
        today, cd.fixingCalendar, dc, cd.obsLag, index->frequency(), baseRate, helpers, 1e-10, index,
        useLastKnownFixing);
    if (seasonality) {
        curve->setSeasonality(seasonality);
    }
    return curve;
}

QuantLib::ext::shared_ptr<ZeroInflationIndex> buildIndexWithForwardTermStructure(CommonData& cd, bool useLastKnownFixing = true,
                                                                         bool isInterpolated = false,
                                                                         const Date& startDate = Date()) {
    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<QuantLib::EUHICPXT>(isInterpolated);
    for (const auto& [date, fixing] : cd.cpiFixings) {
        curveBuildIndex->addFixing(date, fixing);
    }
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixing, curveBuildIndex, isInterpolated, nullptr, startDate);

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    return index;
}

QuantLib::ext::shared_ptr<CPIVolatilitySurface> buildVolSurface(CommonData& cd,
                                                        const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
                                                        const QuantLib::Date& startDate = QuantLib::Date()) {
    auto surface = QuantLib::ext::make_shared<QuantExt::InterpolatedCPIVolatilitySurface<Bilinear>>(
        cd.tenors, cd.strikes, cd.vols, index, 0, cd.fixingCalendar, ModifiedFollowing, cd.dayCounter, cd.obsLag,
        startDate);
    surface->enableExtrapolation();
    return surface;
}

CPICapFloorPriceData pricesFromVolQuotes(CommonData& cd, const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
                                         bool interpolated = false, const Date& startDate = Date()) {

    CPICapFloorPriceData priceData;

    priceData.tenors = cd.tenors;
    priceData.fStrikes = cd.strikes;
    priceData.cStrikes = cd.strikes;
    priceData.cPrices = Matrix(cd.strikes.size(), cd.tenors.size(), 0.0);
    priceData.fPrices = Matrix(cd.strikes.size(), cd.tenors.size(), 0.0);

    Date capFloorStartDate = startDate == Date() ? cd.today : startDate;
    Date capFloorBaseDate = capFloorStartDate - cd.obsLag;
    if (!interpolated) {
        capFloorBaseDate = inflationPeriod(capFloorBaseDate, index->frequency()).first;
    }
    // first day of the month of today - 2M
    Date lastKnownFixing(1, July, 2022);

    Handle<ZeroInflationIndex> hindex(index);

    double baseCPI = index->fixing(capFloorBaseDate);

    for (size_t i = 0; i < cd.strikes.size(); ++i) {
        for (size_t j = 0; j < cd.tenors.size(); ++j) {
            double vol = cd.vols[j][i]->value();
            Date optionFixingDate = capFloorBaseDate + cd.tenors[j];
            Date optionPaymentDate = cd.today + cd.tenors[j];
            double ttm = cd.dayCounter.yearFraction(capFloorBaseDate, optionFixingDate);
            double atmf = index->fixing(optionFixingDate) / baseCPI;
            double strike = std::pow(1 + cd.strikes[i], ttm);
            double discountFactor = cd.discountTS->discount(optionPaymentDate);
            double volTimeFrom = cd.dayCounter.yearFraction(lastKnownFixing, optionFixingDate);

            QuantLib::BlackCalculator callPricer(Option::Call, strike, atmf, sqrt(volTimeFrom) * vol, discountFactor);
            QuantLib::BlackCalculator putPricer(Option::Put, strike, atmf, sqrt(volTimeFrom) * vol, discountFactor);

            priceData.cPrices[i][j] = callPricer.value();
            priceData.fPrices[i][j] = putPricer.value();
        }
    }
    return priceData;
}

QuantLib::ext::shared_ptr<CPIVolatilitySurface> buildVolSurfaceFromPrices(CommonData& cd, CPICapFloorPriceData& priceData,
                                                                  const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
                                                                  const bool useLastKnownFixing,
                                                                  const Date& startDate = Date(),
                                                                  bool ignoreMissingQuotes = false) {

    QuantLib::ext::shared_ptr<QuantExt::CPIBlackCapFloorEngine> engine = QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(
        cd.discountTS, QuantLib::Handle<QuantLib::CPIVolatilitySurface>(), useLastKnownFixing);

    QuantLib::ext::shared_ptr<QuantExt::CPIPriceVolatilitySurface<QuantLib::Linear, QuantLib::Linear>> cpiCapFloorVolSurface;
    cpiCapFloorVolSurface = QuantLib::ext::make_shared<QuantExt::CPIPriceVolatilitySurface<QuantLib::Linear, QuantLib::Linear>>(
        QuantExt::PriceQuotePreference::CapFloor, cd.obsLag, cd.fixingCalendar, cd.bdc, cd.dayCounter, index,
        cd.discountTS, priceData.cStrikes, priceData.fStrikes, priceData.tenors, priceData.cPrices, priceData.fPrices,
        engine, startDate, ignoreMissingQuotes);

    cpiCapFloorVolSurface->enableExtrapolation();
    return cpiCapFloorVolSurface;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InflationCPIVolatilityTest)

BOOST_AUTO_TEST_CASE(testCPIVolatilitySurface) {
    // Test case when the ZCIIS and Cap/Floors start today with using todays fixing
    CommonData cd;

    Date lastKnownFixing(1, Jul, 2022);
    Date capFloorBaseDate(1, Jun, 2022); // first day of the month of today - 2M

    auto index = buildIndexWithForwardTermStructure(cd);

    Handle<ZeroInflationIndex> hindex(index);

    BOOST_CHECK_EQUAL(index->zeroInflationTermStructure()->baseDate(), lastKnownFixing);

    auto volSurface = buildVolSurface(cd, index);

    // Expect the base fixing date of the cap/floor today - 2M
    BOOST_CHECK_EQUAL(volSurface->baseDate(), capFloorBaseDate);

    double baseCPI = index->fixing(volSurface->baseDate());

    BOOST_CHECK_CLOSE(baseCPI, cd.cpiFixings[capFloorBaseDate], cd.tolerance);

    for (size_t i = 0; i < cd.tenors.size(); ++i) {
        auto fixingDate = volSurface->baseDate() + cd.tenors[i];
        for (size_t j = 0; j < cd.strikes.size(); ++j) {   
            auto expectedVol = cd.vols[i][j]->value();
            auto volByTenor = volSurface->volatility(cd.tenors[i], cd.strikes[j]);
            auto volByFixingDate = volSurface->volatility(fixingDate, cd.strikes[j], 0 * Days);
            BOOST_CHECK_CLOSE(volByTenor, expectedVol, cd.tolerance);
            BOOST_CHECK_CLOSE(volByFixingDate, expectedVol, cd.tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCPIBlackCapFloorEngine) {
    // Test case when the ZCIIS and Cap/Floors start today with using todays fixing
    CommonData cd;
    auto index = buildIndexWithForwardTermStructure(cd);

    auto volSurface = buildVolSurface(cd, index);

    double baseCPI = index->fixing(volSurface->baseDate());

    auto priceData = pricesFromVolQuotes(cd, index, false, Date());

    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(
        cd.discountTS, Handle<CPIVolatilitySurface>(volSurface), true);

    for (size_t i = 0; i < priceData.cStrikes.size(); ++i) {
        double strike = priceData.cStrikes[i];
        for (size_t j = 0; j < priceData.tenors.size(); ++j) {
            Period tenor = priceData.tenors[j];
            CPICapFloor cap(Option::Call, 1.0, cd.today, baseCPI, cd.today + tenor, cd.fixingCalendar, cd.bdc,
                            cd.fixingCalendar, cd.bdc, strike, index, cd.obsLag, CPI::Flat);
            cap.setPricingEngine(engine);
            BOOST_CHECK_CLOSE(cap.NPV(), priceData.cPrices[i][j], cd.tolerance);
        }
    }

    for (size_t i = 0; i < priceData.fStrikes.size(); ++i) {
        double strike = priceData.fStrikes[i];
        for (size_t j = 0; j < priceData.tenors.size(); ++j) {
            Period tenor = priceData.tenors[j];
            CPICapFloor put(Option::Put, 1.0, cd.today, baseCPI, cd.today + tenor, cd.fixingCalendar, cd.bdc,
                            cd.fixingCalendar, cd.bdc, strike, index, cd.obsLag, CPI::Flat);
            put.setPricingEngine(engine);
            BOOST_CHECK_CLOSE(put.NPV(), priceData.fPrices[i][j], cd.tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCPIBlackCapFloorEngineSeasonedCapFloors) {
    // Pricing seasoned cap/floors
    CommonData cd;

    Date lastKnownFixing(1, Jul, 2022);
    Date capFloorBaseDate(1, Jun, 2022); // first day of the month of today - 2M

    auto index = buildIndexWithForwardTermStructure(cd);

    auto volSurface = buildVolSurface(cd, index);

    double baseCPI = index->fixing(volSurface->baseDate());

    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(
        cd.discountTS, Handle<CPIVolatilitySurface>(volSurface), true);

    Date seasonedStartDate(15, Aug, 2021);
    Date seasonedMaturity(15, Aug, 2024);
    Date seasonedBaseFixingDate(1, Jun, 2021);
    Date seasonedFixingDate(1, Jun, 2024);
    double seasonedStrike = 0.03;
    double seasonedBaseCPI = index->fixing(seasonedBaseFixingDate);

    double K = pow(1 + seasonedStrike, cd.dayCounter.yearFraction(seasonedBaseFixingDate, seasonedFixingDate));
    double atm = index->fixing(seasonedFixingDate) / seasonedBaseCPI;

    double adjustedStrike = std::pow(K * seasonedBaseCPI / baseCPI,
                                     1.0 / cd.dayCounter.yearFraction(volSurface->baseDate(), seasonedFixingDate)) -
                            1.0;

    double volTimeFrom = cd.dayCounter.yearFraction(lastKnownFixing, seasonedFixingDate);
    double vol = volSurface->volatility(seasonedFixingDate, adjustedStrike, 0 * Days, false);
    double discountFactor = cd.discountTS->discount(seasonedMaturity);
    QuantLib::BlackCalculator callPricer(Option::Call, K, atm, sqrt(volTimeFrom) * vol, discountFactor);

    QuantLib::CPICapFloor cap(Option::Call, 1.0, seasonedStartDate, Null<double>(), seasonedMaturity, cd.fixingCalendar,
                              cd.bdc, cd.fixingCalendar, cd.bdc, seasonedStrike, index,
                              cd.obsLag, CPI::Flat);

    cap.setPricingEngine(engine);

    BOOST_CHECK_CLOSE(cap.NPV(), callPricer.value(), cd.tolerance);
}

BOOST_AUTO_TEST_CASE(testCPIPriceVolSurface) {
    // Test case when the ZCIIS and Cap/Floors start today with using todays fixing
    CommonData cd;
    auto index = buildIndexWithForwardTermStructure(cd);

    auto priceData = pricesFromVolQuotes(cd, index, false, Date());

    auto volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), false);

    for (size_t i = 0; i < cd.tenors.size(); ++i) {
        auto fixingDate = volSurface->baseDate() + cd.tenors[i];
        for (size_t j = 0; j < cd.strikes.size(); ++j) {
            auto expectedVol = cd.vols[i][j]->value();
            auto volByTenor = volSurface->volatility(cd.tenors[i], cd.strikes[j]);
            auto volByFixingDate = volSurface->volatility(fixingDate, cd.strikes[j], 0 * Days);
            BOOST_CHECK_CLOSE(volByTenor, expectedVol, cd.tolerance);
            BOOST_CHECK_CLOSE(volByFixingDate, expectedVol, cd.tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCPIPriceVolSurfaceMissingQuoteInterpolation) {
    // Test case when the ZCIIS and Cap/Floors start today with using todays fixing
    CommonData cd;
    auto index = buildIndexWithForwardTermStructure(cd);

    // Remove 1 Quote and check if its get interpolated
    for (Size tenorIdx = 1; tenorIdx < cd.tenors.size() - 1; tenorIdx++) {
        for (Size strikeIdx = 1; strikeIdx < cd.strikes.size() - 1; strikeIdx++) {
            auto priceData = pricesFromVolQuotes(cd, index, false, Date());
            priceData.cPrices[strikeIdx][tenorIdx] = Null<Real>();
            priceData.fPrices[strikeIdx][tenorIdx] = Null<Real>();

            auto volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), true);

            double vol = volSurface->volatility(cd.tenors[tenorIdx], cd.strikes[strikeIdx]);

            double expectedVol =
                cd.vols[tenorIdx][strikeIdx - 1]->value() +
                (cd.vols[tenorIdx][strikeIdx + 1]->value() - cd.vols[tenorIdx][strikeIdx - 1]->value()) *
                    (cd.strikes[strikeIdx] - cd.strikes[strikeIdx - 1]) /
                    (cd.strikes[strikeIdx + 1] - cd.strikes[strikeIdx - 1]);

            BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
        }
    }
}


BOOST_AUTO_TEST_CASE(testCPIPriceVolSurfaceMissingQuoteExtrapolation) {
    // Test case when the ZCIIS and Cap/Floors start today with using todays fixing
    CommonData cd;
    auto index = buildIndexWithForwardTermStructure(cd);

    // Remove 1 Quote and check if its get extrapolated
    for (Size tenorIdx = 1; tenorIdx < cd.tenors.size() - 1; tenorIdx++) {

        auto priceData = pricesFromVolQuotes(cd, index, false, Date());

        priceData.cPrices[0][tenorIdx] = Null<Real>();
        priceData.fPrices[0][tenorIdx] = Null<Real>();

        auto volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), true);

        double vol = volSurface->volatility(cd.tenors[tenorIdx], cd.strikes.front());

        double expectedVol = cd.vols[tenorIdx][1]->value();

        BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
    }   

    // Remove 1 Quote and check if its get extrapolated
    for (Size tenorIdx = 1; tenorIdx < cd.tenors.size() - 1; tenorIdx++) {

        auto priceData = pricesFromVolQuotes(cd, index, false, Date());

        priceData.cPrices[cd.strikes.size()-1][tenorIdx] = Null<Real>();
        priceData.fPrices[cd.strikes.size() - 1][tenorIdx] = Null<Real>();

        auto volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), true);

        double vol = volSurface->volatility(cd.tenors[tenorIdx], cd.strikes.back());

        double expectedVol = cd.vols[tenorIdx][cd.strikes.size() - 2]->value();

        BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
    }   

    // Remove 2 Quote and check if its get extrapolated
    for (Size tenorIdx = 1; tenorIdx < cd.tenors.size() - 1; tenorIdx++) {

        auto priceData = pricesFromVolQuotes(cd, index, false, Date());

        priceData.cPrices[0][tenorIdx] = Null<Real>();
        priceData.fPrices[0][tenorIdx] = Null<Real>();

        priceData.cPrices[1][tenorIdx] = Null<Real>();
        priceData.fPrices[1][tenorIdx] = Null<Real>();

        auto volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), true);

        double vol = volSurface->volatility(cd.tenors[tenorIdx], cd.strikes.front());

        double expectedVol = cd.vols[tenorIdx][2]->value();

        BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
    }
}


BOOST_AUTO_TEST_CASE(testCPIPriceVolSurfaceAllButOneQuoteMissing) {
    // Test case when the ZCIIS and Cap/Floors start today with using todays fixing
    CommonData cd;
    auto index = buildIndexWithForwardTermStructure(cd);

    // Remove all QUotes for a strike
    auto priceDataAllQuotes = pricesFromVolQuotes(cd, index, false, Date());
    auto priceData = pricesFromVolQuotes(cd, index, false, Date());
    for (Size strikeIdx = 0; strikeIdx < cd.strikes.size(); strikeIdx++) {
        priceData.cPrices[strikeIdx][0] = Null<Real>();
        priceData.fPrices[strikeIdx][0] = Null<Real>();
    }
    priceData.cPrices[1][0] = priceDataAllQuotes.cPrices[1][0];
    priceData.fPrices[1][0] = priceDataAllQuotes.fPrices[1][0];

    auto volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), true);

    double vol = volSurface->volatility(cd.tenors[0], cd.strikes[0]);
    double expectedVol = cd.vols[0][1]->value();
    BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);

    for (Size tenorIdx = 0; tenorIdx < cd.tenors.size(); tenorIdx++) {
        for (Size strikeIdx = 0; strikeIdx < cd.strikes.size(); strikeIdx++) {
            priceData.cPrices[strikeIdx][tenorIdx] = Null<Real>();
            priceData.fPrices[strikeIdx][tenorIdx] = Null<Real>();
        }
        priceData.cPrices[1][tenorIdx] = priceDataAllQuotes.cPrices[1][tenorIdx];
        priceData.fPrices[1][tenorIdx] = priceDataAllQuotes.fPrices[1][tenorIdx];
    }

    volSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, Date(), true);
    for (Size tenorIdx = 0; tenorIdx < cd.tenors.size(); tenorIdx++) {
        double vol = volSurface->volatility(cd.tenors[tenorIdx], cd.strikes[0]);
        double expectedVol = cd.vols[tenorIdx][1]->value();
        BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
    }
}


BOOST_AUTO_TEST_CASE(testVolatiltiySurfaceWithStartDate) {
    // Test case when the ZCIIS and Cap/Floors don't start today
    // but depend on the publishing schedule of the fixings
    CommonData cd;
    Date today(15, July, 2022);
    cd.today = today;
    cd.obsLag = 3 * Months;
    Settings::instance().evaluationDate() = today;
    std::map<Date, double> fixings{{Date(1, Mar, 2022), 100.0}};
    // the Q2 fixing not published yet, the zciis swaps and caps start on 15th Jun and
    // reference on the Q1 fixing
    Date startDate(15, Jun, 2022);
    Date lastKnownFixing(1, Jan, 2022);

    QuantLib::ext::shared_ptr<ZeroInflationIndex> curveBuildIndex = QuantLib::ext::make_shared<QuantLib::AUCPI>(Quarterly, true, false);
    for (const auto& [date, fixing] : fixings) {
        curveBuildIndex->addFixing(date, fixing);
    }

    auto curve = buildZeroInflationCurve(cd, true, curveBuildIndex, false, nullptr, startDate);

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    BOOST_CHECK_EQUAL(curve->baseDate(), lastKnownFixing);

    BOOST_CHECK_EQUAL(curve->dates()[1], Date(1, Jan, 2023));
    BOOST_CHECK_CLOSE(curve->data()[0], cd.zeroCouponQuotes[0], cd.tolerance);
    BOOST_CHECK_CLOSE(curve->data()[1], cd.zeroCouponQuotes[0], cd.tolerance);
    BOOST_CHECK_CLOSE(curve->data()[2], cd.zeroCouponQuotes[1], cd.tolerance);

    auto volSurface = buildVolSurface(cd, index, startDate);

    BOOST_CHECK_EQUAL(volSurface->baseDate(), Date(1, Jan, 2022));

    double baseCPI = index->fixing(volSurface->baseDate());

    BOOST_CHECK_CLOSE(baseCPI, 100.0, cd.tolerance);

    Matrix cPrices(cd.strikes.size(), cd.tenors.size(), 0.0);
    Matrix fPrices(cd.strikes.size(), cd.tenors.size(), 0.0);

    for (size_t i = 0; i < cd.strikes.size(); ++i) {
        for (size_t j = 0; j < cd.tenors.size(); ++j) {
            double expectedVol = cd.vols[j][i]->value();
            Date optionFixingDate = volSurface->baseDate() + cd.tenors[j];
            Date optionPaymentDate = startDate + cd.tenors[j];

            double vol = volSurface->volatility(optionFixingDate, cd.strikes[i], 0 * Days, false);
            BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
            double ttm = cd.dayCounter.yearFraction(volSurface->baseDate(), optionFixingDate);
            double atmf = index->fixing(optionFixingDate) / baseCPI;
            double strike = std::pow(1 + cd.strikes[i], ttm);
            double discountFactor = cd.discountTS->discount(optionPaymentDate);
            double volTimeFrom = cd.dayCounter.yearFraction(lastKnownFixing, optionFixingDate);
            QuantLib::BlackCalculator callPricer(Option::Call, strike, atmf, sqrt(volTimeFrom) * vol, discountFactor);
            QuantLib::BlackCalculator putPricer(Option::Put, strike, atmf, sqrt(volTimeFrom) * vol, discountFactor);

            cPrices[i][j] = callPricer.value();
            fPrices[i][j] = putPricer.value();
        }
    }

    CPICapFloorPriceData priceData;
    priceData.tenors = cd.tenors;
    priceData.cPrices = cPrices;
    priceData.fPrices = fPrices;
    priceData.cStrikes = cd.strikes;
    priceData.fStrikes = cd.strikes;

    auto priceSurface = buildVolSurfaceFromPrices(cd, priceData, index, true, startDate);

    for (size_t i = 0; i < cd.strikes.size(); ++i) {
        for (size_t j = 0; j < cd.tenors.size(); ++j) {
            double expectedVol = cd.vols[j][i]->value();
            Date optionFixingDate = priceSurface->baseDate() + cd.tenors[j];
            double vol = priceSurface->volatility(optionFixingDate, cd.strikes[i], 0 * Days, false);
            BOOST_CHECK_CLOSE(vol, expectedVol, cd.tolerance);
        }
    }



}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
