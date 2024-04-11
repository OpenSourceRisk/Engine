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
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace ore::data;
using std::vector;

// Inflation CPI Swap paying fixed * CPI(t)/baseCPI * N
// with last XNL N * (CPI(T)/baseCPI - 1)

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        // valuation date
        asof_ = Date(18, July, 2016);

        // build vectors with dates and discount factors for GBP
        vector<Date> datesGBP = {asof_,
                                 asof_ + 6 * Months,
                                 asof_ + 7 * Months,
                                 asof_ + 8 * Months,
                                 asof_ + 9 * Months,
                                 asof_ + 10 * Months,
                                 asof_ + 11 * Months,
                                 asof_ + 12 * Months,
                                 asof_ + 13 * Months,
                                 asof_ + 14 * Months,
                                 asof_ + 15 * Months,
                                 asof_ + 16 * Months,
                                 asof_ + 17 * Months,
                                 asof_ + 18 * Months,
                                 asof_ + 19 * Months,
                                 asof_ + 20 * Months,
                                 asof_ + 21 * Months,
                                 asof_ + 22 * Months,
                                 asof_ + 23 * Months,
                                 asof_ + 2 * Years,
                                 asof_ + 3 * Years,
                                 asof_ + 4 * Years,
                                 asof_ + 5 * Years,
                                 asof_ + 6 * Years,
                                 asof_ + 7 * Years,
                                 asof_ + 8 * Years,
                                 asof_ + 9 * Years,
                                 asof_ + 10 * Years,
                                 asof_ + 15 * Years,
                                 asof_ + 20 * Years};

        vector<DiscountFactor> dfsGBP = {1,      0.9955, 0.9953, 0.9947, 0.9941, 0.9933, 0.9924, 0.9914,
                                         0.9908, 0.9901, 0.9895, 0.9888, 0.9881, 0.9874, 0.9868, 0.9862,
                                         0.9855, 0.9849, 0.9842, 0.9836, 0.9743, 0.9634, 0.9510, 0.9361,
                                         0.9192, 0.9011, 0.8822, 0.8637, 0.7792, 0.7079};

        // build vectors with dates and inflation zc swap rates for UKRPI
        vector<Date> datesZCII = {asof_,
                                  asof_ + 1 * Years,
                                  asof_ + 2 * Years,
                                  asof_ + 3 * Years,
                                  asof_ + 4 * Years,
                                  asof_ + 5 * Years,
                                  asof_ + 6 * Years,
                                  asof_ + 7 * Years,
                                  asof_ + 8 * Years,
                                  asof_ + 9 * Years,
                                  asof_ + 10 * Years,
                                  asof_ + 12 * Years,
                                  asof_ + 15 * Years,
                                  asof_ + 20 * Years};

        vector<Rate> ratesZCII = {2.825, 2.9425, 2.975,  2.983, 3.0,  3.01,  3.008,
                                  3.009, 3.013,  3.0445, 3.044, 3.09, 3.109, 3.108};

        // build UKRPI fixing history
        Schedule fixingDatesUKRPI =
            MakeSchedule().from(Date(1, May, 2015)).to(Date(1, July, 2016)).withTenor(1 * Months);
        Real fixingRatesUKRPI[] = {258.5, 258.9, 258.6, 259.8, 259.6, 259.5,  259.8, 260.6,
                                   258.8, 260.0, 261.1, 261.4, 262.1, -999.0, -999.0};

        // build GBP discount curve
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] =
            intDiscCurve(datesGBP, dfsGBP, ActualActual(ActualActual::ISDA), UnitedKingdom());

        // build GBP Libor index
        hGBP = Handle<IborIndex>(
            parseIborIndex("GBP-LIBOR-6M", intDiscCurve(datesGBP, dfsGBP, ActualActual(ActualActual::ISDA), UnitedKingdom())));
        iborIndices_[make_pair(Market::defaultConfiguration, "GBP-LIBOR-6M")] = hGBP;

        // add Libor 6M fixing (lag for GBP is 0d)
        hGBP->addFixing(Date(18, July, 2016), 0.0061731);

        // build UKRPI index
        QuantLib::ext::shared_ptr<UKRPI> ii;
        QuantLib::ext::shared_ptr<ZeroInflationTermStructure> cpiTS;
        RelinkableHandle<ZeroInflationTermStructure> hcpi;
        ii = QuantLib::ext::shared_ptr<UKRPI>(new UKRPI(hcpi));
        for (Size i = 0; i < fixingDatesUKRPI.size(); i++) {
            // std::cout << i << ", " << fixingDatesUKRPI[i] << ", " << fixingRatesUKRPI[i] << std::endl;
            ii->addFixing(fixingDatesUKRPI[i], fixingRatesUKRPI[i], true);
        };
        // now build the helpers ...
        vector<QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure>>> instruments;
        for (Size i = 0; i < datesZCII.size(); i++) {
            Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(ratesZCII[i] / 100.0)));
            QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure>> anInstrument(
                new ZeroCouponInflationSwapHelper(
                    quote, Period(2, Months), datesZCII[i], UnitedKingdom(), ModifiedFollowing, ActualActual(ActualActual::ISDA), ii,
                    CPI::AsIndex, yieldCurves_.at(make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP"))));
            ;
            instruments.push_back(anInstrument);
        };
        // we can use historical or first ZCIIS for this
        // we know historical is WAY off market-implied, so use market implied flat.
        Rate baseZeroRate = ratesZCII[0] / 100.0;
        QuantLib::ext::shared_ptr<PiecewiseZeroInflationCurve<Linear>> pCPIts(new PiecewiseZeroInflationCurve<Linear>(
            asof_, UnitedKingdom(), ActualActual(ActualActual::ISDA), Period(2, Months), ii->frequency(),
            baseZeroRate, instruments));
        pCPIts->recalculate();
        cpiTS = QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(pCPIts);
        hUKRPI = Handle<ZeroInflationIndex>(
            parseZeroInflationIndex("UKRPI", Handle<ZeroInflationTermStructure>(cpiTS)));
        zeroInflationIndices_[make_pair(Market::defaultConfiguration, "UKRPI")] = hUKRPI;
    }

    Handle<IborIndex> hGBP;
    Handle<ZeroInflationIndex> hUKRPI;

private:
    Handle<YieldTermStructure> intDiscCurve(vector<Date> dates, vector<DiscountFactor> dfs, DayCounter dc,
                                            Calendar cal) {
        QuantLib::ext::shared_ptr<YieldTermStructure> idc(
            new QuantLib::InterpolatedDiscountCurve<LogLinear>(dates, dfs, dc, cal));
        return Handle<YieldTermStructure>(idc);
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CPISwapTests)

BOOST_AUTO_TEST_CASE(testCPISwapPrice) {

    BOOST_TEST_MESSAGE("Testing CPI Swap Price...");

    // build market
    Date today(18, July, 2016);
    Settings::instance().evaluationDate() = today;
    QuantLib::ext::shared_ptr<TestMarket> market = QuantLib::ext::make_shared<TestMarket>();
    Date marketDate = market->asofDate();
    BOOST_CHECK_EQUAL(today, marketDate);
    Settings::instance().evaluationDate() = marketDate;

    // Test if GBP discount curve is empty
    Handle<YieldTermStructure> dts = market->discountCurve("GBP");
    QL_REQUIRE(!dts.empty(), "GBP discount curve not found");
    BOOST_CHECK_CLOSE(market->discountCurve("GBP")->discount(today + 1 * Years), 0.9914, 0.0001);

    // Test if GBP Libor curve is empty
    Handle<IborIndex> iis = market->iborIndex("GBP-LIBOR-6M");
    QL_REQUIRE(!iis.empty(), "GBP LIBOR 6M ibor Index not found");
    BOOST_TEST_MESSAGE(
        "CPISwap: Projected Libor fixing: " << market->iborIndex("GBP-LIBOR-6M")->forecastFixing(today + 1 * Years));

    // Test if GBP discount curve is empty
    Handle<ZeroInflationIndex> infidx = market->zeroInflationIndex("UKRPI");
    QL_REQUIRE(!infidx.empty(), "UKRPI inflation index not found");
    BOOST_TEST_MESSAGE("CPISwap: Projected UKRPI rate: " << infidx->fixing(today + 1 * Years));

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
    string conv = "MF";
    string rule = "Forward";
    ScheduleData scheduleLibor(ScheduleRules(start, end, "6M", "UK", conv, conv, rule));
    ScheduleData scheduleCPI(ScheduleRules(start, end, "1Y", "UK", conv, conv, rule));

    // Leg variables
    bool isInArrears = false;
    string dc = "ACT/ACT";
    vector<Real> notional(1, 10000000);
    string paymentConvention = "F";

    // GBP Libor Leg
    bool isPayerLibor = true;
    string indexLibor = "GBP-LIBOR-6M";
    vector<Real> spread(1, 0);
    LegData legLibor(QuantLib::ext::make_shared<FloatingLegData>(indexLibor, 0, isInArrears, spread), isPayerLibor, "GBP",
                     scheduleLibor, "A365F", notional, vector<string>(), paymentConvention);

    // GBP CPI Leg
    bool isPayerCPI = false;
    string indexCPI = "UKRPI";
    Real baseCPI = 210.0;
    string CPIlag = "2M";
    std::vector<double> fixedRate(1, 0.02);
    bool interpolated = false;
    LegData legCPI(
        QuantLib::ext::make_shared<CPILegData>(indexCPI, start, baseCPI, CPIlag, (interpolated ? "Linear" : "Flat"), fixedRate),
        isPayerCPI, "GBP", scheduleCPI, dc, notional, vector<string>(), paymentConvention, false, true);

    // Build swap trades
    QuantLib::ext::shared_ptr<Trade> CPIswap(new ore::data::Swap(env, legLibor, legCPI));

    // engine data and factory
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // build swaps and portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    CPIswap->id() = "CPI_Swap";

    portfolio->add(CPIswap);
    portfolio->build(engineFactory);

    // check CPI swap NPV against pure QL pricing
    Schedule floatSchedule(startDate, endDate, 6 * Months, UnitedKingdom(), ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false);
    Schedule cpiSchedule(startDate, endDate, 1 * Years, UnitedKingdom(), ModifiedFollowing, ModifiedFollowing,
                         DateGeneration::Forward, false);
    Leg floatLeg = IborLeg(floatSchedule, *market->hGBP).withNotionals(10000000.0);
    Leg cpiLeg = CPILeg(cpiSchedule, *market->hUKRPI, baseCPI, 2 * Months)
                     .withFixedRates(0.02)
                     .withNotionals(10000000)
                     .withObservationInterpolation(CPI::Flat)
                     .withPaymentDayCounter(ActualActual(ActualActual::ISDA))
                     .withPaymentAdjustment(Following);
    auto pricer = QuantLib::ext::make_shared<CPICouponPricer>(market->hGBP->forwardingTermStructure());
    for (auto const& c : cpiLeg) {
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<CPICoupon>(c))
            cpn->setPricer(pricer);
    }

    QuantLib::Swap qlSwap(floatLeg, cpiLeg);
    auto dscEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(market->hGBP->forwardingTermStructure());
    qlSwap.setPricingEngine(dscEngine);
    BOOST_TEST_MESSAGE("Leg 1 NPV: ORE = "
                       << QuantLib::ext::static_pointer_cast<QuantLib::Swap>(CPIswap->instrument()->qlInstrument())->legNPV(0)
                       << " QL = " << qlSwap.legNPV(0));
    BOOST_TEST_MESSAGE("Leg 2 NPV: ORE = "
                       << QuantLib::ext::static_pointer_cast<QuantLib::Swap>(CPIswap->instrument()->qlInstrument())->legNPV(1)
                       << " QL = " << qlSwap.legNPV(1));
    BOOST_CHECK_CLOSE(CPIswap->instrument()->NPV(), qlSwap.NPV(), 1E-8); // this is 1E-10 rel diff
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
