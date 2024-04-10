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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
#include <ql/pricingengines/inflation/inflationcapfloorengines.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

// FX Swap test
class TestMarket : public MarketImpl {
public:
    TestMarket(Date asof) : MarketImpl(false) {
        // valuation date

        dc = ActualActual(ActualActual::ISDA);
        bdc = Following;
        cal = TARGET();

        // Add EUR notional curve
        Handle<YieldTermStructure> nominalTs(QuantLib::ext::make_shared<FlatForward>(0, cal, 0.005, dc));
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = nominalTs;

        // Add EUHICPXT YoY Inflation Curve
        QuantLib::ext::shared_ptr<ZeroInflationIndex> zcindex = QuantLib::ext::shared_ptr<EUHICPXT>(new EUHICPXT());
        ;
        QuantLib::ext::shared_ptr<YoYInflationIndex> index =
            QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zcindex, false);

        std::vector<Date> datesZCII = {asof + 1 * Years, asof + 2 * Years, asof + 5 * Years, asof + 10 * Years,
                                       asof + 20 * Years};
        std::vector<Rate> ratesZCII = {1.1625, 1.23211, 1.36019, 1.51199, 1.74773};

        std::vector<QuantLib::ext::shared_ptr<YoYInflationTraits::helper>> instruments;
        for (Size i = 0; i < datesZCII.size(); i++) {
            Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(ratesZCII[i] / 100)));
            QuantLib::ext::shared_ptr<YoYInflationTraits::helper> anInstrument =
                QuantLib::ext::make_shared<YearOnYearInflationSwapHelper>(quote, Period(3, Months), datesZCII[i], cal, bdc, dc,
                                                                  index, nominalTs);
            instruments.push_back(anInstrument);
        };
        QuantLib::ext::shared_ptr<YoYInflationTermStructure> yoyTs = QuantLib::ext::shared_ptr<PiecewiseYoYInflationCurve<Linear>>(
            new PiecewiseYoYInflationCurve<Linear>(asof, TARGET(), Actual365Fixed(), Period(3, Months), Monthly,
                                                   index->interpolated(), ratesZCII[0] / 100, instruments));

        yoyInflationIndices_[make_pair(Market::defaultConfiguration, "EUHICPXT")] =
            Handle<YoYInflationIndex>(QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(
                parseZeroInflationIndex("EUHICPXT"), false, Handle<YoYInflationTermStructure>(yoyTs)));

        // Add EUHICPXT yoy volatility term structure
        QuantLib::ext::shared_ptr<QuantLib::ConstantYoYOptionletVolatility> volSurface =
            QuantLib::ext::make_shared<QuantLib::ConstantYoYOptionletVolatility>(0.01, 0, cal, bdc, dc, Period(3, Months),
                                                                         Monthly, index->interpolated(), -1.0, 100.0,
                                                                         VolatilityType::Normal);

        yoyCapFloorVolSurfaces_[make_pair(Market::defaultConfiguration, "EUHICPXT")] =
            Handle<QuantExt::YoYOptionletVolatilitySurface>(volSurface);
    }

    BusinessDayConvention bdc;
    DayCounter dc;
    Calendar cal;
};

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InflationCapFloorTests)

BOOST_AUTO_TEST_CASE(testYoYCapFloor) {

    BOOST_TEST_MESSAGE("Testing YoY Cap Price...");

    // build market
    Date today(18, July, 2016);
    Settings::instance().evaluationDate() = today;

    QuantLib::ext::shared_ptr<TestMarket> market = QuantLib::ext::make_shared<TestMarket>(today);
    // Test if EUR yoy inflation curve is empty
    Handle<YoYInflationIndex> infidx = market->yoyInflationIndex("EUHICPXT");
    QL_REQUIRE(!infidx.empty(), "EUHICPXT inflation index not found");

    // envelope
    Envelope env("CP");

    // Start/End date
    Date startDate = today;
    Date endDate = today + 5 * Years;

    // date 2 string
    std::ostringstream oss;
    oss << io::iso_date(startDate);
    string start(oss.str());
    oss.str("");
    oss.clear();
    oss << io::iso_date(endDate);
    string end(oss.str());

    // Schedules
    string conv = "F";
    string rule = "Forward";
    ScheduleData scheduleYY(ScheduleRules(start, end, "1Y", "TARGET", conv, conv, rule));

    // Leg variables
    string dc = "ACT/ACT";
    vector<Real> notional(1, 10000000);
    string paymentConvention = "F";

    // EUR YoY Leg
    bool isPayerYY = false;
    string indexYY = "EUHICPXT";
    string lag = "3M";
    LegData legYY(QuantLib::ext::make_shared<YoYLegData>(indexYY, lag, 0), isPayerYY, "EUR", scheduleYY, dc, notional,
                  vector<string>(), paymentConvention, false, true);

    std::vector<double> caps(1, 0.009);
    // Build capfloor trades
    QuantLib::ext::shared_ptr<Trade> yyCap(new ore::data::CapFloor(env, "Long", legYY, caps, std::vector<double>()));

    // engine data and factory
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("YYCapFloor") = "YYCapModel";
    engineData->engine("YYCapFloor") = "YYCapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // build capfloor and portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    yyCap->id() = "YoY_Cap";

    portfolio->add(yyCap);
    portfolio->build(engineFactory);

    // check YoY cap NPV against pure QL pricing
    Schedule schedule(startDate, endDate, 1 * Years, TARGET(), Following, Following, DateGeneration::Forward, false);
    Handle<YieldTermStructure> nominalTs = market->discountCurve("EUR");
    Leg yyLeg =
        yoyInflationLeg(schedule, TARGET(), market->yoyInflationIndex("EUHICPXT").currentLink(), Period(3, Months))
            .withNotionals(10000000)
            .withPaymentDayCounter(ActualActual(ActualActual::ISDA))
            .withPaymentAdjustment(Following)
            .withRateCurve(nominalTs);

    QuantLib::ext::shared_ptr<YoYInflationCapFloor> qlCap(new YoYInflationCap(yyLeg, caps));

    Handle<QuantLib::YoYOptionletVolatilitySurface> hovs = market->yoyCapFloorVol("EUHICPXT");
    auto dscEngine = QuantLib::ext::make_shared<YoYInflationBachelierCapFloorEngine>(
        market->yoyInflationIndex("EUHICPXT").currentLink(), hovs, nominalTs);
    qlCap->setPricingEngine(dscEngine);
    BOOST_CHECK_CLOSE(yyCap->instrument()->NPV(), qlCap->NPV(), 1E-8); // this is 1E-10 rel diff
    BOOST_CHECK_NO_THROW(yyCap->additionalData());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
