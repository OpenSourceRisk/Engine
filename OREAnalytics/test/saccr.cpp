/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include "saccr.hpp"

#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <orea/engine/observationmode.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>
#include <orea/engine/saccr.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <ored/portfolio/builders/commodityswap.hpp>
#include <ored/portfolio/builders/commodityswaption.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxtouchoption.hpp>
#include <test/oreatoplevelfixture.hpp>

#include <ql/currencies/america.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/termstructures/pricecurve.hpp>
#include <qle/utilities/inflation.hpp>


using ore::test::TopLevelFixture;
using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace {

class LocalTestMarket : public MarketImpl {
public:
    LocalTestMarket() : MarketImpl(false) {
        asof_ = Date(14, April, 2016);

        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.059);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.06);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] = flatRateYts(0.04);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] = flatRateYts(0.04);

        // add index
        Handle<IborIndex> h0(parseIborIndex("USD-SIFMA", flatRateYts(0.01)));
        iborIndices_[make_pair(Market::defaultConfiguration, "USD-SIFMA")] = h0;
        // set up dummy fixings for the past 400 days
        for (Date d = asof_ - 400; d < asof_; d++) {
            if (h0->isValidFixingDate(d))
                h0->addFixing(d, 0.01);
        }

        Handle<IborIndex> h1(parseIborIndex("USD-LIBOR-3M", flatRateYts(0.01)));
        iborIndices_[make_pair(Market::defaultConfiguration, "USD-LIBOR-3M")] = h1;
        // set up dummy fixings for the past 400 days
        for (Date d = asof_ - 400; d < asof_; d++) {
            if (h1->isValidFixingDate(d))
                h1->addFixing(d, 0.01);
        }

        Handle<IborIndex> h2(parseIborIndex("EUR-EURIBOR-6M", flatRateYts(0.06)));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = h2;
        // set up dummy fixings for the past 400 days
        for (Date d = asof_ - 400; d < asof_; d++) {
            if (h2->isValidFixingDate(d))
                h2->addFixing(d, 0.06);
        }

        Handle<IborIndex> h3(parseIborIndex("EUR-EURIBOR-3M", flatRateYts(0.06)));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-3M")] = h3;
        // set up dummy fixings for the past 400 days
        for (Date d = asof_ - 400; d < asof_; d++) {
            if (h3->isValidFixingDate(d))
                h3->addFixing(d, 0.06);
        }
       
        Handle<IborIndex> h4(parseIborIndex("GBP-LIBOR-3M", flatRateYts(0.06)));
        iborIndices_[make_pair(Market::defaultConfiguration, "GBP-LIBOR-3M")] = h4;
        // set up dummy fixings for the past 400 days
        for (Date d = asof_ - 400; d < asof_; d++) {
            if (h4->isValidFixingDate(d))
                h4->addFixing(d, 0.06);
        }
        
        Handle<IborIndex> h5(parseIborIndex("JPY-LIBOR-3M", flatRateYts(0.06)));
        iborIndices_[make_pair(Market::defaultConfiguration, "JPY-LIBOR-3M")] = h5;
        // set up dummy fixings for the past 400 days
        for (Date d = asof_ - 400; d < asof_; d++) {
            if (h5->isValidFixingDate(d))
                h5->addFixing(d, 0.06);
        }

        // add fx rates
        std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
        quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.1197));
        quotes["GBPUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.3113));
        quotes["USDJPY"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(108.86));
        fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

	// add fx conventions
        auto conventions = QuantLib::ext::make_shared<Conventions>();
        conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000", "EUR,USD"));
        conventions->add(QuantLib::ext::make_shared<FXConvention>("GBP-USD-FX", "0", "GBP", "USD", "10000", "GBP,USD"));
        conventions->add(QuantLib::ext::make_shared<FXConvention>("USD-JPY-FX", "0", "USD", "JPY", "10000", "USD,JPY"));
        conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));
        conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-GBP-FX", "0", "EUR", "GBP", "10000", "EUR,GBP"));
        InstrumentConventions::instance().setConventions(conventions);

        // build fx vols
        fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.5);
        fxVols_[make_pair(Market::defaultConfiguration, "GBPUSD")] = flatRateFxv(0.5);
        fxVols_[make_pair(Market::defaultConfiguration, "EURGBP")] = flatRateFxv(0.5);
        fxVols_[make_pair(Market::defaultConfiguration, "EURJPY")] = flatRateFxv(0.5);

        // add swaption vols
        swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateSvs(0.5);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateSvs(0.5);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateSvs(0.5);

        // add inf
        // build vectors with dates and inflation zc swap rates for UKRPI
        vector<Date> datesZCII = {asof_,
                                  asof_ + 2 * Years,
                                  asof_ + 5 * Years,
                                  asof_ + 20 * Years};
        vector<Rate> ratesZCII = {2.825, 3.0, 3.109, 3.108};

        // build UKRPI fixing history
        Schedule fixingDatesUKRPI =
            MakeSchedule().from(Date(1, May, 2015)).to(Date(1, April, 2016)).withTenor(1 * Months);

        // build UKRPI index
        QuantLib::ext::shared_ptr<UKRPI> ii;
        QuantLib::ext::shared_ptr<ZeroInflationTermStructure> cpiTS;
        RelinkableHandle<ZeroInflationTermStructure> hcpi;
        ii = QuantLib::ext::shared_ptr<UKRPI>(new UKRPI(hcpi));
        for (Size i = 0; i < fixingDatesUKRPI.size(); i++) {
            ii->addFixing(fixingDatesUKRPI[i], 258, true);
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
        auto obsLag = Period(2, Months);
        auto frequency = ii->frequency();
        Date baseDate =
            QuantExt::ZeroInflation::curveBaseDate(false, asof_, obsLag, frequency, ii);
        QuantLib::ext::shared_ptr<PiecewiseZeroInflationCurve<Linear>> pCPIts(new PiecewiseZeroInflationCurve<Linear>(
            asof_, baseDate, obsLag, frequency, ActualActual(ActualActual::ISDA), instruments));
        pCPIts->recalculate();
        cpiTS = QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(pCPIts);
        Handle<ZeroInflationIndex> hUKRPI = Handle<ZeroInflationIndex>(
            parseZeroInflationIndex("UKRPI", Handle<ZeroInflationTermStructure>(cpiTS)));
        zeroInflationIndices_[make_pair(Market::defaultConfiguration, "UKRPI")] = hUKRPI;

        // Commodity price curves and spots
        Actual365Fixed ccDayCounter;
        vector<Period> commTenors = {0 * Days, 365 * Days, 730 * Days, 1825 * Days};

        // Gold curve
        vector<Real> prices = {1155.593, 1160.9, 1168.1, 1210};
        Handle<PriceTermStructure> ptsGold(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
            commTenors, prices, ccDayCounter, USDCurrency()));
        ptsGold->enableExtrapolation();
        commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_GOLD_USD")] = Handle<CommodityIndex>(
            QuantLib::ext::make_shared<CommoditySpotIndex>("COMDTY_GOLD_USD", NullCalendar(), ptsGold));

        // WTI Oil curve
        prices = {30.89, 41.23, 44.44, 49.18};
        Handle<PriceTermStructure> ptsOil(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
            commTenors, prices, ccDayCounter, USDCurrency()));
        ptsOil->enableExtrapolation();
        commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_WTI_USD")] = Handle<CommodityIndex>(
            QuantLib::ext::make_shared<CommoditySpotIndex>("COMDTY_WTI_USD", NullCalendar(), ptsOil));

        // Livestock Lean Hogs
        prices = {115.593, 110.9, 118.1, 120};
        Handle<PriceTermStructure> ptsHogs(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
            commTenors, prices, ccDayCounter, USDCurrency()));
        ptsHogs->enableExtrapolation();
        commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_HOG_USD")] = Handle<CommodityIndex>(
            QuantLib::ext::make_shared<CommoditySpotIndex>("COMDTY_HOG_USD", NullCalendar(), ptsHogs));

        // Freight Dry
        prices = {125.593, 120.9, 128.1, 130};
        Handle<PriceTermStructure> ptsFreight(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
            commTenors, prices, ccDayCounter, EURCurrency()));
        ptsFreight->enableExtrapolation();
        commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_FREIGHT_EUR")] = Handle<CommodityIndex>(
            QuantLib::ext::make_shared<CommoditySpotIndex>("COMDTY_FREIGHT_EUR", NullCalendar(), ptsFreight));

        // NA Power ERCOT
        prices = {1205.593, 1200.9, 1208.1, 1300};
        Handle<PriceTermStructure> ptsPower(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(
            commTenors, prices, ccDayCounter, USDCurrency()));
        ptsPower->enableExtrapolation();
        commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_POWER_USD")] = Handle<CommodityIndex>(
            QuantLib::ext::make_shared<CommoditySpotIndex>("COMDTY_POWER_USD", NullCalendar(), ptsPower));
        
        // Commodity volatilities
        commodityVols_[make_pair(Market::defaultConfiguration, "COMDTY_GOLD_USD")] = flatRateFxv(0.15);
        commodityVols_[make_pair(Market::defaultConfiguration, "COMDTY_WTI_USD")] = flatRateFxv(0.20);
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(new BlackConstantVol(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<BlackVolTermStructure>(fxv);
    }
    Handle<QuantLib::SwaptionVolatilityStructure> flatRateSvs(Volatility forward,
                                                              VolatilityType type = ShiftedLognormal, Real shift = 0.0) {
        QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                 ModifiedFollowing, forward, Actual365Fixed(), type, shift));
        return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
    }

};


QuantLib::ext::shared_ptr<EngineData> engineData() {
    // Create the engine data
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();

    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";

    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";

    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";

    data->model("FxForward") = "DiscountedCashflows";
    data->engine("FxForward") = "DiscountingFxForwardEngine";

    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";

    data->model("FxBarrierOption") = "GarmanKohlhagen";
    data->engine("FxBarrierOption") = "AnalyticBarrierEngine";
    
    data->model("FxTouchOption") = "GarmanKohlhagen";
    data->engine("FxTouchOption") = "AnalyticDigitalAmericanEngine";
    
    data->model("CommoditySwap") = "DiscountedCashflows";
    data->engine("CommoditySwap") = "CommoditySwapEngine";

    data->model("CommoditySwaption") = "Black";
    data->engine("CommoditySwaption") = "AnalyticalApproximation";

    data->model("CommodityForward") = "DiscountedCashflows";
    data->engine("CommodityForward") = "DiscountingCommodityForwardEngine";
    
    return data;
}

QuantLib::ext::shared_ptr<EngineFactory> engineFactory(const QuantLib::ext::shared_ptr<Market>& market) {
    // Create the engine factory and register the builders
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(engineData(), market);
    return factory;
}

QuantLib::ext::shared_ptr<SACCR> run_saccr(const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    SavedSettings backup;

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;
    string nettingSet = "NS";
    string nettingSet2 = "NS_2";
    string baseCurrency = "USD";
    // Initial market

    testsuite::TestConfigurationObjects::setConventions();
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<LocalTestMarket>();

    // Create the portfolio
    QuantLib::ext::shared_ptr<EngineFactory> factory = engineFactory(initMarket);
    
    portfolio->build(factory);

    QuantLib::ext::shared_ptr<NettingSetManager> nettingSetManager = QuantLib::ext::make_shared<NettingSetManager>();
    NettingSetDefinition n(nettingSet, "Bilateral", "USD", "USD-FedFunds", 0, 0, 0, 0, 0, "FIXED", "1D", "1D", "2W", 0, 0, { "USD"});
    QuantLib::ext::shared_ptr<NettingSetDefinition> nd =
        QuantLib::ext::make_shared<NettingSetDefinition>(n);
    nettingSetManager->add(nd);
    
    NettingSetDefinition n2(nettingSet2);
    QuantLib::ext::shared_ptr<NettingSetDefinition> nd2 =
        QuantLib::ext::make_shared<NettingSetDefinition>(n2);
    nettingSetManager->add(nd2);

    QuantLib::ext::shared_ptr<CounterpartyManager> cpManager = QuantLib::ext::make_shared<CounterpartyManager>();
    QuantLib::ext::shared_ptr<CounterpartyInformation> cp = QuantLib::ext::make_shared<CounterpartyInformation>("CP", false, ore::data::CounterpartyCreditQuality::NR, Null<Real>(), 0.5);
    cpManager->add(cp);

    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> collateralBalances = QuantLib::ext::make_shared<ore::data::CollateralBalances>();
    QuantLib::ext::shared_ptr<ore::data::CollateralBalance> cb = QuantLib::ext::make_shared<ore::data::CollateralBalance>(nettingSet, baseCurrency, 0, 0);
    collateralBalances->add(cb);
    QuantLib::ext::shared_ptr<ore::data::CollateralBalance> cb2 = QuantLib::ext::make_shared<ore::data::CollateralBalance>(nettingSet2, baseCurrency, 0, 0);
    collateralBalances->add(cb2);

    QuantLib::ext::shared_ptr<SimmBasicNameMapper> nameMapper = QuantLib::ext::make_shared<SimmBasicNameMapper>();
    nameMapper->addMapping("COMDTY_GOLD_USD", "Precious Metals Gold"); // metals
    nameMapper->addMapping("COMDTY_WTI_USD", "Crude oil Americas"); // energy
    nameMapper->addMapping("COMDTY_POWER_USD", "NA Power ERCOT"); // energy
    nameMapper->addMapping("COMDTY_HOG_USD", "Livestock Lean Hogs"); // agriculture
    nameMapper->addMapping("COMDTY_FREIGHT_EUR", "Freight Dry"); // other 

    QuantLib::ext::shared_ptr<SimmBucketMapperBase> bucketMapper = QuantLib::ext::make_shared<SimmBucketMapperBase>();
    using RT = CrifRecord::RiskType;
    bucketMapper->addMapping(RT::Commodity, "Precious Metals Gold", "12");
    bucketMapper->addMapping(RT::Commodity, "Crude oil Americas", "2");
    bucketMapper->addMapping(RT::Commodity, "NA Power ERCOT", "8");
    bucketMapper->addMapping(RT::Commodity, "Livestock Lean Hogs", "15");
    bucketMapper->addMapping(RT::Commodity, "Freight Dry", "10");

    map<SACCR::ReportType, QuantLib::ext::shared_ptr<Report>> reports;
    SACCR saccr(portfolio, nettingSetManager, cpManager, initMarket, baseCurrency, collateralBalances,
                QuantLib::ext::make_shared<CollateralBalances>(), nameMapper, bucketMapper, nullptr, reports);

    return QuantLib::ext::make_shared<SACCR>(saccr);
}
} // namespace

namespace testsuite {

void SaccrTest::testSACCR_HedgingSets() {

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;
    testsuite::TestConfigurationObjects::setConventions();

    // Create the portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    /*portfolio->add(testsuite::buildSwap(
        "1_Swap_USD", "USD", true, 10000.0, 0, 10, 0.01, 0.00, "1Y", "ACT/ACT", "6M", "ACT/ACT", "USD-LIBOR-3M", TARGET(), 2, false, "NS"));
    portfolio->add(testsuite::buildEuropeanSwaption(
        "2_Swaption_EUR", "Long", "EUR", true, 6250.0, 1, 10, 0.05, 0.00, "1Y", "ACT/ACT", "6M", "ACT/ACT", "EUR-EURIBOR-6M", "Physical", "NS"));*/
    portfolio->add(testsuite::buildFxOption(
					    "3_FXOption_EUR", "Long", "Put", 10, "EUR", 1000, "USD", 1200, 0, "", "", "NS_2"));
    portfolio->add(testsuite::buildFxOption(
					    "4_FXOption_USD", "Long", "Put", 10, "USD", 1200, "EUR", 1000, 0, "", "", "NS"));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "5_XCCY_Basis_Swap", "EUR", 10000000, "USD", 10000000, 0, 15, 0.0000, 0.0000, "3M",
        "A360", "EUR-EURIBOR-3M", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS_2", false));
   /* portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "6_Basis_Swap", "EUR", 10000000, "EUR", 10000000, 0, 15, 0.0000, 0.0000, "6M",
        "A360", "EUR-EURIBOR-6M", TARGET(), "3M", "A360", "EUR-EURIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "7_Basis_Swap", "EUR", 10000000, "EUR", 10000000, 0, 15, 0.0000, 0.0000, "3M",
        "A360", "EUR-EURIBOR-3M", TARGET(), "6M", "A360", "EUR-EURIBOR-6M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "8_BMA_Basis_Swap", "USD", 10000000, "USD", 10000000, 0, 15, 0.0000, 0.0000, "3M",
        "A360", "USD-LIBOR-3M", TARGET(), "3M", "A360", "USD-SIFMA",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "9_BMA_Basis_Swap", "USD", 10000000, "USD", 10000000, 0, 15, 0.0000, 0.0000, "3M",
        "A360", "USD-SIFMA", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCPIInflationSwap(
        "10_CPI_Basis_Swap", "GBP", true, 10000000, 0, 15, 0.0000, "3M",
        "A360", "GBP-LIBOR-3M", "3M", "A360", "UKRPI",
        201.0, "2M", false, 0.005));*/
    portfolio->add(testsuite::buildFxForward(
        "11_FxForward", 15, "GBP", 1000, "USD", 1200, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "12_FxForward", 15, "USD", 1200, "GBP", 1000, "NS_2"));
    portfolio->add(testsuite::buildCommoditySwap(
        "13_Commodity_Swap", "USD", false, 3000, 0, 15, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_GOLD_USD",
        TARGET(), 2, true, "NS", 1000));
    portfolio->add(testsuite::buildCommoditySwap(
        "14_Commodity_Swap", "USD", false, 3000, 0, 15, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_WTI_USD",
        TARGET(), 2, true, "NS", 1000));
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "15_Commodity_Swap", "USD", false, 3000, 0, 15, "3M", "A360", "COMDTY_WTI_USD", 
        "COMDTY_GOLD_USD", TARGET(), 2, true, "NS"));     
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "16_Commodity_Swap", "USD", false, 3000, 0, 15, "3M", "A360", "COMDTY_GOLD_USD", 
        "COMDTY_GOLD_USD", TARGET(), 2, true, "NS"));     
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "17_Commodity_Swap", "USD", false, 3000, 0, 15, "3M", "A360", "COMDTY_GOLD_USD", 
        "COMDTY_WTI_USD", TARGET(), 2, true, "NS"));     
    portfolio->add(testsuite::buildCommodityForward(
        "18_Commodity_Forward", "USD", "COMDTY_WTI_USD", 3000, 15, 100, "Short", "NS", TARGET()));     
    portfolio->add(testsuite::buildCommodityForward(
        "19_Commodity_Forward", "USD", "COMDTY_GOLD_USD", 3000, 15, 100, "Short", "NS", TARGET()));     
    portfolio->add(testsuite::buildCommodityForward(
        "20_Commodity_Forward", "USD", "COMDTY_HOG_USD", 3000, 15, 100, "Short", "NS", TARGET()));     
    portfolio->add(testsuite::buildCommodityForward(
        "21_Commodity_Forward", "EUR", "COMDTY_FREIGHT_EUR", 3000, 15, 100, "Short", "NS", TARGET()));     
    portfolio->add(testsuite::buildCommoditySwap(
        "22_Commodity_Swap", "USD", false, 3000, 0, 15, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_HOG_USD",
        TARGET(), 2, true, "NS", 1000));
    portfolio->add(testsuite::buildCommoditySwap(
        "23_Commodity_Swap", "EUR", false, 3000, 0, 15, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_FREIGHT_EUR",
        TARGET(), 2, true, "NS", 1000));
    portfolio->add(testsuite::buildCommoditySwap(
        "24_Commodity_Swap", "USD", false, 3000, 0, 15, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_POWER_USD",
        TARGET(), 2, true, "NS", 1000));
    portfolio->add(testsuite::buildCommodityForward(
        "25_Commodity_Forward", "USD", "COMDTY_POWER_USD", 3000, 15, 100, "Short", "NS", TARGET()));     
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "26_Commodity_Swap", "USD", false, 3000, 0, 15, "3M", "A360", "COMDTY_GOLD_USD", 
        "COMDTY_POWER_USD", TARGET(), 2, true, "NS"));     
    portfolio->add(testsuite::buildFxBarrierOption(
        "27_FX_Barrier_Option", "Long", "Call", 10, "EUR", 1000, "USD", 1200, "NS_2", "UpAndOut", 1.3));     
    portfolio->add(testsuite::buildFxBarrierOption(
        "28_FX_Barrier_Option", "Short", "Put", 10, "USD", 1200, "EUR", 1000, "NS", "UpAndOut", 1.3));

    // Get the cached results
    // clang-format off
    vector<ore::analytics::SACCR::TradeData> expectedResults = {
            // id,                      tradeType,                  nettingSet, assetClass,                     hedgingSet,                                     hedgingSubset,              NPV,    npvCcy, currentNnl, delta,  d, MF,       M,  S, E,  T, fwd, K, numNominalFlows
            /*
            {"1_Swap_USD",              "Swap",                 "NS",       SACCR::AssetClass::IR,          "USD",                                          "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"2_Swaption_EUR",          "Swap",                 "NS",       SACCR::AssetClass::IR,          "EUR",                                          "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},*/
            {"3_FXOption_EUR",          "FxOption",             "NS_2",     SACCR::AssetClass::FX,          "EURUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"4_FXOption_USD",          "FxOption",             "NS",       SACCR::AssetClass::FX,          "EURUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"5_XCCY_Basis_Swap",       "Swap",                 "NS_2",     SACCR::AssetClass::FX,          "EURUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            /*
            {"6_Basis_Swap",            "Swap",                 "NS",       SACCR::AssetClass::IR,          "EUR-BASIS-3M-6M",                              "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"7_Basis_Swap",            "Swap",                 "NS",       SACCR::AssetClass::IR,          "EUR-BASIS-3M-6M",                              "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"8_BMA_Basis_Swap",        "Swap",                 "NS",       SACCR::AssetClass::IR,          "USD-BASIS-BMA",                                "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"9_BMA_Basis_Swap",        "Swap",                 "NS",       SACCR::AssetClass::IR,          "USD-BASIS-BMA",                                "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"10_CPI_Basis_Swap",       "Swap",                 "NS",       SACCR::AssetClass::IR,          "GBP-BASIS-IBOR-INFLATION",                     "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},*/
            {"11_FxForward",            "FxForward",            "NS",       SACCR::AssetClass::FX,          "GBPUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"12_FxForward",            "FxForward",            "NS_2",     SACCR::AssetClass::FX,          "GBPUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"13_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "Metal",                                        "Precious Metals Gold",     1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"14_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "Energy",                                       "Crude oil",                1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"15_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_WTI_USD",     "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"16_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "Metal",                                        "Precious Metals Gold",     1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"17_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_WTI_USD",     "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"18_Commodity_Forward",    "CommodityForward",     "NS",       SACCR::AssetClass::Commodity,   "Energy",                                       "Crude oil",                1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"19_Commodity_Forward",    "CommodityForward",     "NS",       SACCR::AssetClass::Commodity,   "Metal",                                        "Precious Metals Gold",     1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"20_Commodity_Forward",    "CommodityForward",     "NS",       SACCR::AssetClass::Commodity,   "Agriculture",                                  "Livestock Lean Hogs",      1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"21_Commodity_Forward",    "CommodityForward",     "NS",       SACCR::AssetClass::Commodity,   "Other",                                        "Freight Dry",              1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"22_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "Agriculture",                                  "Livestock Lean Hogs",      1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"23_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "Other",                                        "Freight Dry",              1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"24_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "Energy",                                       "Power",                    1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"25_Commodity_Forward",    "CommodityForward",     "NS",       SACCR::AssetClass::Commodity,   "Energy",                                       "Power",                    1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"26_Commodity_Swap",       "CommoditySwap",        "NS",       SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_POWER_USD",   "Power",                    1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"27_FX_Barrier_Option",    "FxBarrierOption",      "NS_2",     SACCR::AssetClass::FX,          "EURUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
            {"28_FX_Barrier_Option",    "FxBarrierOption",      "NS",       SACCR::AssetClass::FX,          "EURUSD",                                       "",                         1,      "USD",  1,          1,      1, 0.294174, 10, 0, 10, 0, 0,   0, 0},
        }; 
    // clang-format on

    BOOST_TEST_MESSAGE("run saccr");
    QuantLib::ext::shared_ptr<SACCR> saccr = run_saccr(portfolio);
    BOOST_TEST_MESSAGE("run saccr OK");
    vector<ore::analytics::SACCR::TradeData> tradeData = saccr->tradeData();
    for (auto& td : tradeData) {
        BOOST_TEST_MESSAGE(td.id << " "
            << td.type << " ["
            << td.nettingSetDetails << "] "
            << td.assetClass << " "
            << td.hedgingSet << " " 
            << td.hedgingSubset
            );
    }

    BOOST_CHECK(tradeData.size() == expectedResults.size());
    for (auto td : tradeData) {
        BOOST_TEST_MESSAGE(td.id << " "
            << td.type << " ["
            << td.nettingSetDetails << "] "
            << td.assetClass << " "
            << td.hedgingSet );
        bool found = false;
        for (auto r : expectedResults) {
            if (r.id == td.id) {
                BOOST_CHECK(td.type == r.type);
                BOOST_CHECK(td.nettingSetDetails == r.nettingSetDetails);
                BOOST_CHECK(td.assetClass == r.assetClass);
                BOOST_CHECK(td.hedgingSet == r.hedgingSet);
                BOOST_CHECK(td.hedgingSubset == r.hedgingSubset);
                found = true;
            }
        }
        BOOST_CHECK(found);
    }
}

void SaccrTest::testSACCR_Delta() {
    //Testing pairs of trades that should have delta with opposite signs

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;
    // Create the portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    
    // Hedging set GBPUSD, Delta with opposite signs
    portfolio->add(testsuite::buildFxForward(
        "1_FxForward", 15, "GBP", 1000, "USD", 1200, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "2_FxForward", 15, "USD", 1200, "GBP", 1000, "NS"));
    
    // Hedging set EURUSD, Delta with opposite signs 
    portfolio->add(testsuite::buildFxOption(
					    "3_FXOption", "Long", "Call", 10, "EUR", 1000, "USD", 1200, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "4_FXOption", "Short", "Call", 10, "EUR", 1000, "USD", 1200, 0, "", "", "NS"));
   
    portfolio->add(testsuite::buildFxOption(
        "5_FXOption", "Long", "Put", 10, "EUR", 1000, "USD", 1200, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "6_FXOption", "Short", "Put", 10, "EUR", 1000, "USD", 1200, 0, "", "", "NS"));
    
    // Hedging set EURUSD, Delta with opposite signs 
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "7_XCCY_Basis_Swap", "EUR", 10000000, "USD", 10000000, 0, 20, 0.0000, 0.0000, "3M",
        "A360", "USD-LIBOR-3M", TARGET(), "3M", "A360", "EUR-EURIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "8_XCCY_Basis_Swap", "USD", 10000000, "EUR", 10000000, 0, 20, 0.0000, 0.0000, "3M",
        "A360", "EUR-EURIBOR-3M", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));

    // Hedging set EURUSD, Delta with opposite signs
    portfolio->add(testsuite::buildFxTouchOption(
        "9_FXTouchOption", "Long", 10, "EUR", "USD", 1000, "NS", "UpAndIn", 1.3));
    portfolio->add(testsuite::buildFxTouchOption(
        "10_FXTouchOption", "Short", 10, "EUR", "USD", 1000, "NS", "UpAndIn", 1.3));

    // Hedging set EURUSD, Delta with opposite signs
    portfolio->add(testsuite::buildFxBarrierOption(
        "13_FXBarrierOption", "Long", "Call", 10, "USD", 1200, "EUR", 1000, "NS", "UpAndIn", 1.3));
    portfolio->add(testsuite::buildFxBarrierOption(
        "14_FXBarrierOption", "Short", "Call", 10, "USD", 1200, "EUR", 1000, "NS", "UpAndIn", 1.3));

    portfolio->add(testsuite::buildFxBarrierOption(
        "15_FXBarrierOption", "Long", "Put", 10, "USD", 1200, "EUR", 1000, "NS", "UpAndIn", 1.3));
    portfolio->add(testsuite::buildFxBarrierOption(
        "16_FXBarrierOption", "Short", "Put", 10, "USD", 1200, "EUR", 1000, "NS", "UpAndIn", 1.3));
    
    // Hedging set Energy, Delta with opposite signs
    portfolio->add(testsuite::buildCommodityForward(
        "17_Commodity_Forward", "USD", "COMDTY_WTI_USD", 3000, 14, 100, "Long", "NS", TARGET()));     
    portfolio->add(testsuite::buildCommodityForward(
        "18_Commodity_Forward", "USD", "COMDTY_WTI_USD", 3000, 14, 100, "Short", "NS", TARGET()));     

    // Hedging set Agriculture, Delta with opposite signs
    portfolio->add(testsuite::buildCommoditySwap(
        "19_Commodity_Swap", "USD", false, 3500, 0, 5, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_HOG_USD",
        TARGET(), 2, true, "NS_2", 10000));
    portfolio->add(testsuite::buildCommoditySwap(
        "20_Commodity_Swap", "USD", true, 3500, 0, 5, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_HOG_USD",
        TARGET(), 2, true, "NS_2", 10000));

    // Hedging set COMDTY_GOLD_USD/COMDTY_POWER_USD, Delta with opposite signs
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "21_Commodity_Swap", "USD", false, 3000, 0, 15, "3M", "A360", "COMDTY_GOLD_USD", 
        "COMDTY_POWER_USD", TARGET(), 2, true, "NS"));     
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "22_Commodity_Swap", "USD", true, 3000, 0, 15, "3M", "A360", "COMDTY_GOLD_USD", 
        "COMDTY_POWER_USD", TARGET(), 2, true, "NS"));     

    Real nullReal = QuantLib::Null<Real>();
    Size nullSize = QuantLib::Null<Size>();
    // Get the cached results
    // clang-format off
    vector<ore::analytics::SACCR::TradeData> expectedResults = {
            // id,                   tradeType,           nettingSet,   assetClass,                     hedgingSet,                                   hedgingSubset,             NPV,            npvCcy, currentNnl,  delta,       d,          MF,         M,          S,           E,          T,          price,     K,          numNominalFlows
            {"1_FxForward",          "FxForward",        "NS",          SACCR::AssetClass::FX,          "GBPUSD",                                     "",                        231.77348,      "USD",  1311.3,      1,           1311.3,     0.294174,   15.0008,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"2_FxForward",          "FxForward",        "NS",          SACCR::AssetClass::FX,          "GBPUSD",                                     "",                        -231.9027,      "USD",  1311.3,      -1,          1311.3,     0.294174,   15.0008,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"4_FXOption",           "FxOption",         "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        -346.373435,    "USD",  1119.7,      -0.52791867, 1119.7,     0.294174,   9.99804,    nullReal,    nullReal,   9.99804,    1.10856,   1.2,        nullSize},
            {"3_FXOption",           "FxOption",         "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        346.373435,     "USD",  1119.7,      0.52791867,  1119.7,     0.294174,   9.99804,    nullReal,    nullReal,   9.99804,    1.10856,   1.2,        nullSize},
            {"5_FXOption",           "FxOption",         "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        384.2728,       "USD",  1119.7,      -0.47208,    1119.7,     0.294174,   9.99804,    nullReal,    nullReal,   9.99804,    1.10856,   1.2,        nullSize},
            {"6_FXOption",           "FxOption",         "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        -384.2728,      "USD",  1119.7,      0.47208,     1119.7,     0.294174,   9.99804,    nullReal,    nullReal,   9.99804,    1.10856,   1.2,        nullSize},
            {"7_XCCY_Basis_Swap",    "Swap",             "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        -5677715.67395, "USD",  11197000,    1,           11197000,   0.294174,   20.0109,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"8_XCCY_Basis_Swap",    "Swap",             "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        5677715.67395,  "USD",  11197000,    -1,          11197000,   0.294174,   20.0109,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"9_FXTouchOption",      "FxTouchOption",    "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        605.51186,      "USD",  1119.7,      0.460679,    1119.7,     0.29417,    9.998,      nullReal,    nullReal,   9.998,      1.10856,   1.3,        nullSize},
            {"10_FXTouchOption",     "FxTouchOption",    "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        -605.51186,     "USD",  1119.7,      -0.460679,   1119.7,     0.29417,    9.99803,    nullReal,    nullReal,   9.99803,    1.10856,   1.3,        nullSize},
            {"13_FXBarrierOption",   "FxBarrierOption",  "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        383.7724,       "USD",  1119.7,      -0.47208,    1119.7,     0.29417,    9.9980,     nullReal,    nullReal,   9.9980,     1.10856,   1.2,        nullSize},
            {"14_FXBarrierOption",   "FxBarrierOption",  "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        -383.7724,      "USD",  1119.7,      0.47208,     1119.7,     0.29417,    9.998,      nullReal,    nullReal,   9.998,      1.10856,   1.2,        nullSize},
            {"15_FXBarrierOption",   "FxBarrierOption",  "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        167.1018,       "USD",  1119.7,      0.5279,      1119.7,     0.29417,    9.9980,     nullReal,    nullReal,   9.9980,     1.10856,   1.2,        nullSize},
            {"16_FXBarrierOption",   "FxBarrierOption",  "NS",          SACCR::AssetClass::FX,          "EURUSD",                                     "",                        -167.1018,      "USD",  1119.7,      -0.5279,     1119.7,     0.29417,    9.9980,     nullReal,    nullReal,   9.9980,     1.10856,   1.2,        nullSize},
            {"17_Commodity_Forward", "CommodityForward", "NS",          SACCR::AssetClass::Commodity,   "Energy",                                     "Crude oil",               -47377.177,     "USD",  190251.9452, 1,           190251.9452, 0.29417,    14.0007,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"18_Commodity_Forward", "CommodityForward", "NS",          SACCR::AssetClass::Commodity,   "Energy",                                     "Crude oil",               47377.177,      "USD",  190251.9452, -1,          190251.9452, 0.29417,    14.0007,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"19_Commodity_Swap",    "CommoditySwap",    "NS_2",        SACCR::AssetClass::Commodity,   "Agriculture",                                "Livestock Lean Hogs",     592816231.07,   "USD",  400300.00,   -1,          400300.00,   1,          5.0117,     nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"20_Commodity_Swap",    "CommoditySwap",    "NS_2",        SACCR::AssetClass::Commodity,   "Agriculture",                                "Livestock Lean Hogs",     -592816231.07,  "USD",  400300.00,   1,           400300.00,   1,          5.0117,     nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"21_Commodity_Swap",    "CommoditySwap",    "NS",          SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_POWER_USD", "Power",                   -13972705.1169, "USD",  142191.78,     -1,        142191.78,   0.29417,    15.0089,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize},
            {"22_Commodity_Swap",    "CommoditySwap",    "NS",          SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_POWER_USD", "Power",                   13972705.1169,  "USD",  142191.78,     1,         142191.78,   0.29417,    15.0089,    nullReal,    nullReal,   nullReal,   nullReal,  nullReal,   nullSize}
        }; 
    // clang-format on

    QuantLib::ext::shared_ptr<SACCR> saccr = run_saccr(portfolio);
    vector<ore::analytics::SACCR::TradeData> tradeData = saccr->tradeData();
    Real tolerance = 0.07;
    BOOST_CHECK(tradeData.size() == expectedResults.size());
    CumulativeNormalDistribution N;
    for (auto td : tradeData) {
        BOOST_TEST_MESSAGE(td.id << ", "
            << setprecision(16)
            << td.type << ", ["
            << td.nettingSetDetails << "], "
            << td.assetClass << ", "
            << td.hedgingSet << ", "
            << td.NPV << ", "
            << td.npvCcy << ", "
            << td.currentNotional << ", "
            << td.delta << ", "
            << td.d << ", "
            << td.MF << ", "
            << td.M << ", "
            << td.S << ", "
            << td.E << ", "
            << td.T << ", "
            << td.price << ", "
            << td.strike << ", "
            << td.numNominalFlows
            );
        bool found = false;
        for (auto r : expectedResults) {
            if (r.id == td.id) {
            
                BOOST_CHECK(td.assetClass == r.assetClass);
                BOOST_CHECK(td.hedgingSet == r.hedgingSet);
                BOOST_CHECK(td.hedgingSubset == r.hedgingSubset);
                BOOST_CHECK_CLOSE(td.NPV, r.NPV, tolerance);
                BOOST_CHECK_CLOSE(td.currentNotional, r.currentNotional, tolerance);
                BOOST_CHECK_CLOSE(td.delta, r.delta, tolerance);
                BOOST_CHECK_CLOSE(td.d, r.d, tolerance);
                BOOST_CHECK_CLOSE(td.S, r.S, tolerance);
                BOOST_CHECK_CLOSE(td.E, r.E, tolerance);
                BOOST_CHECK_CLOSE(td.M, r.M, tolerance);
                BOOST_CHECK_CLOSE(td.MF, r.MF, tolerance);
                BOOST_CHECK_CLOSE(td.T, r.T, tolerance);
                BOOST_CHECK_CLOSE(td.price,  r.price, tolerance);
                BOOST_CHECK_CLOSE(td.strike, r.strike, tolerance);
                BOOST_CHECK(td.numNominalFlows == r.numNominalFlows);
                found = true;
            }
        }
        BOOST_CHECK(found);
    }
}

void SaccrTest::testSACCR_CurrentNotional() {

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;
    // Create the portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    
    // currentNtl == 1000GBP == 1311.3USD
    portfolio->add(testsuite::buildFxForward(
        "1_FxForward", 15, "GBP", 1000, "USD", 1200, "NS"));
    // currentNtl == MAX(1000GBP, 1200EUR) == MAX(1131USD,1343.64USD)
    portfolio->add(testsuite::buildFxForward(
        "2_FxForward", 16, "GBP", 1000, "EUR", 1200, "NS"));
    
    // currentNtl == 1000EUR == 1119.7USD
    portfolio->add(testsuite::buildFxOption(
        "3_FXOption_EUR", "Long", "Call", 10, "EUR", 1000, "USD", 1200, 0, "", "", "NS"));
    // currentNtl == MAX(1000GBP, 1200EUR) == MAX(1311.3USD,1343.64USD)
    portfolio->add(testsuite::buildFxOption(
        "4_FXOption_GBP", "Long", "Call", 11, "EUR", 1200, "GBP", 1000, 0, "", "", "NS"));
   
    // currentNtl == 10000000EUR == 11197000USD
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "5_XCCY_Basis_Swap", "EUR", 10000000, "USD", 10000000, 0, 20, 0.0000, 0.0000, "3M",
        "A360", "EUR-EURIBOR-3M", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    // currentNtl == MAX(10000000EUR, 9000000GBP) ==  11801700USD
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "6_XCCY_Basis_Swap", "EUR", 10000000, "GBP", 9000000, 0, 5, 0.0000, 0.0000, "3M",
        "A360", "EUR-EURIBOR-3M", TARGET(), "3M", "A360", "GBP-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    // currentNtl == AVG(EUR_LEG) == 10866806USD
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "7_XCCY_Basis_Swap", "EUR", 10000000, "USD", 10000000, 0, 15, 0.0000, 0.0000, "3M",
        "A360", "EUR-EURIBOR-3M", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", true, "3M"));

    // currentNtl == 1000EUR == 1119.7USD
    portfolio->add(testsuite::buildFxOption(
        "8_FXOption_EUR", "Long", "Call", 5, "USD", 1200, "EUR", 1000, 0, "", "", "NS"));
    // currentNtl == MAX(1000GBP, 1200EUR) == MAX( 1311USD,1343.64USD)
    portfolio->add(testsuite::buildFxOption(
        "9_FXOption_USD", "Long", "Call", 10, "GBP", 1000, "EUR", 1200, 0, "", "", "NS"));
    
    // currentNtl == 1000EUR == 1119.7USD
    portfolio->add(testsuite::buildFxTouchOption(
        "10_FXTouchOption_EUR", "Long", 10, "EUR", "USD", 1000, "NS", "UpAndIn", 1.3));
    // currentNtl == 1000GBP == 1311USD
    portfolio->add(testsuite::buildFxTouchOption(
        "11_FXTouchOption_USD", "Long", 12, "GBP", "EUR", 1000, "NS", "UpAndIn", 1.3));

    // currentNtl == 1000EUR == 1119.7USD (the non-base ccy)
    portfolio->add(testsuite::buildFxBarrierOption(
        "12_FXBarrierOption_EUR", "Long", "Call", 10, "USD", 1200, "EUR", 1000, "NS", "UpAndIn", 1.3));
    // currentNtl == MAX(1000GBP, 1200EUR) == MAX( 1311USD,1343.64USD) (max of non base ccy)
    portfolio->add(testsuite::buildFxBarrierOption(
        "13_FXBarrierOption_USD", "Long", "Call", 13, "GBP", 1000, "EUR", 1200, "NS", "UpAndIn", 1.3));

    // currentNtl == Price * Quantity * fx = 30.89 * 3000 * 1 = 92670.00
    portfolio->add(testsuite::buildCommodityForward(
        "14_Commodity_Forward", "USD", "COMDTY_WTI_USD", 3000, 14, 100, "Short", "NS", TARGET()));     
    // currentNtl == Price * Quantity * fx = 1155.593 * 4000 * 1 = 4622372.00
    portfolio->add(testsuite::buildCommodityForward(
        "15_Commodity_Forward", "USD", "COMDTY_GOLD_USD", 4000, 17, 100, "Short", "NS", TARGET()));     
    // currentNtl == Price * Quantity * fx = 115.593 * 3500 * 1 = 404575.50
    portfolio->add(testsuite::buildCommodityForward(
        "16_Commodity_Forward", "USD", "COMDTY_HOG_USD", 3500, 30, 100, "Short", "NS", TARGET()));     
    // currentNtl == Price * Quantity * fx = 125.593 * 1000 * 1.1197 = 140626.48
    portfolio->add(testsuite::buildCommodityForward(
        "17_Commodity_Forward", "EUR", "COMDTY_FREIGHT_EUR", 1000, 10, 100, "Short", "NS", TARGET())); 

    // currentNtl == Price * Quantity * fx = 115.593 * 3500 * 1 = 404575.50
    portfolio->add(testsuite::buildCommoditySwap(
        "18_Commodity_Swap", "USD", false, 3500, 0, 5, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_HOG_USD",
        TARGET(), 2, true, "NS_2", 10000));

    // currentNtl == Price * Quantity * fx = 125.593 * 1000 * 1.1197 
    portfolio->add(testsuite::buildCommoditySwap(
        "19_Commodity_Swap", "EUR", false, 1000, 0, 10, 52.51, "3M",
        "A360", "3M", "A360", "COMDTY_FREIGHT_EUR",
        TARGET(), 2, true, "NS_2", 10000));

    // currentNtl == MAX(Price1 * Quantity1 * fx1, Price2 * Quantity2 * fx2)  = MAX(30.89 * 3000 * 1, 1155.593 * 3000 * 1) = 3466779.00 
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "20_Commodity_Swap", "USD", false, 3000, 0, 8, "3M", "A360", "COMDTY_WTI_USD", 
        "COMDTY_GOLD_USD", TARGET(), 2, true, "NS_2"));
    portfolio->add(testsuite::buildCommodityBasisSwap(
        "21_Commodity_Swap", "USD", false, 3000, 0, 9, "3M", "A360", "COMDTY_GOLD_USD", 
        "COMDTY_WTI_USD", TARGET(), 2, true, "NS_2"));

    Real nullReal = QuantLib::Null<Real>();
    Size nullSize = QuantLib::Null<Size>();
    // Get the cached results
    // clang-format off
    vector<ore::analytics::SACCR::TradeData> expectedResults = {
            // id,                     tradeType,           nettingSet,     assetClass,                     hedgingSet,                                 hedgingSubset,          NPV,        npvCcy,     currentNnl,         delta,      d,                  MF,         M,       S,        E,        T,         price,      K,          numNominalFlows
            {"1_FxForward",            "FxForward",         "NS",           SACCR::AssetClass::FX,          "GBPUSD",                                   "",                     229.045,    "USD",      1311.3,             1,          1311.3,             0.294174,   15.0008, nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"2_FxForward",            "FxForward",         "NS",           SACCR::AssetClass::FX,          "EURGBP",                                   "",                     175.623,    "USD",      1343.64,            -1,         1343.64,            0.294174,   16,      nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"3_FXOption_EUR",         "FxOption",          "NS",           SACCR::AssetClass::FX,          "EURUSD",                                   "",                     338.702,    "USD",      1119.7,             0.52791867, 1119.7,             0.294174,   9.99804, nullReal, nullReal, 9.99804,   1.10856,    1.2,        nullSize},
            {"4_FXOption_GBP",         "FxOption",          "NS",           SACCR::AssetClass::FX,          "EURGBP",                                   "",                     516.015,    "USD",      1343.64,            0.75459809, 1343.64,            0.294174,   10.998,  nullReal, nullReal, 10.998,    1.05233,    0.8333333,  nullSize},
            {"5_XCCY_Basis_Swap",      "Swap",              "NS",           SACCR::AssetClass::FX,          "EURUSD",                                   "",                     4436420.25, "USD",      11197000,           1,          11197000,           0.294174,   20.0109, nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"6_XCCY_Basis_Swap",      "Swap",              "NS",           SACCR::AssetClass::FX,          "EURGBP",                                   "",                     -460998,    "USD",      11801700,           1,          11801700,           0.294174,   5.0117,  nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"7_XCCY_Basis_Swap",      "Swap",              "NS",           SACCR::AssetClass::FX,          "EURUSD",                                   "",                     4305867.78, "USD",      10871051.916119652, 1,          10871051.916119652, 0.294174,   15.008,  nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"8_FXOption_EUR",         "FxOption",          "NS",           SACCR::AssetClass::FX,          "EURUSD",                                   "",                     311.45,     "USD",      1119.7,             -0.472081,  1119.7,             0.294174,   4.998,   nullReal, nullReal, 4.998,     1.11412,    1.2,        nullSize},
            {"9_FXOption_USD",         "FxOption",          "NS",           SACCR::AssetClass::FX,          "EURGBP",                                   "",                     353.98,     "USD",      1343.64,            -0.24540,   1343.64,            0.294174,   9.998,   nullReal, nullReal, 9.998,     1.03252,    0.8333,     nullSize},
            {"10_FXTouchOption_EUR",   "FxTouchOption",     "NS",           SACCR::AssetClass::FX,          "EURUSD",                                   "",                     1,          "USD",      1119.7,             1,          1119.7,             0.29417,    9.998,   nullReal, nullReal, 9.998,     1.10856,    1.3,        nullSize},
            {"11_FXTouchOption_USD",   "FxTouchOption",     "NS",           SACCR::AssetClass::FX,          "EURGBP",                                   "",                     1,          "USD",      1311.3,             1,          1311.3,             0.29417,    12.0109, nullReal, nullReal, 12.0109,   1.07278,    1.3,        nullSize},
            {"12_FXBarrierOption_EUR", "FxBarrierOption",   "NS",           SACCR::AssetClass::FX,          "EURUSD",                                   "",                     1,          "USD",      1119.7,             1,          1119.7,             0.29417,    9.9980,  nullReal, nullReal, 9.9980,    1.10856,    1.2,        nullSize},
            {"13_FXBarrierOption_USD", "FxBarrierOption",   "NS",           SACCR::AssetClass::FX,          "EURGBP",                                   "",                     1,          "USD",      1343.64,            1,          1343.64,            0.29417,    13.0035, nullReal, nullReal, 13.0035,   1.0932,     0.833333,   nullSize},
            {"14_Commodity_Forward",   "CommodityForward",  "NS",           SACCR::AssetClass::Commodity,   "Energy",                                   "Crude oil",            1,          "USD",      190251.94520547945, 1,          190251.94520547945, 0.29417,    14.0007, nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"15_Commodity_Forward",   "CommodityForward",  "NS",           SACCR::AssetClass::Commodity,   "Metal",                                    "Precious Metals Gold", 1,          "USD",      5511012.2374429237, 1,          5511012.2374429237, 0.29417,    16.998,  nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"16_Commodity_Forward",   "CommodityForward",  "NS",           SACCR::AssetClass::Commodity,   "Agriculture",                              "Livestock Lean Hogs",  1,          "USD",      475471.32420091343, 1,          475471.32420091343, 0.29417,    30.0035, nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"17_Commodity_Forward",   "CommodityForward",  "NS",           SACCR::AssetClass::Commodity,   "Other",                                    "Freight Dry",          1,          "USD",      149110.60238356164, 1,          149110.60238356164, 0.29417,    9.998,   nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"18_Commodity_Swap",      "CommoditySwap",     "NS",           SACCR::AssetClass::Commodity,   "Agriculture",                              "Livestock Lean Hogs",  1,          "USD",      400300.00,          1,          400300.00,          1,          5.0117,  nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"19_Commodity_Swap",      "CommoditySwap",     "NS",           SACCR::AssetClass::Commodity,   "Other",                                    "Freight Dry",          1,          "USD",      139258.81,          1,          139258.81,          1,          10.0144, nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"20_Commodity_Swap",      "CommoditySwap",     "NS",           SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_WTI_USD", "",                     1,          "USD",      3370179.12,         1,          3370179.12,         1,          8.0109,  nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize},
            {"21_Commodity_Swap",      "CommoditySwap",     "NS",           SACCR::AssetClass::Commodity,   "COMM-COMDTY_GOLD_USD/COMM-COMDTY_WTI_USD", "",                     1,          "USD",      3370179.12,         1,          3370179.12,         1,          9.01995, nullReal, nullReal, nullReal,  nullReal,   nullReal,   nullSize}
        }; 
    // clang-format on

    QuantLib::ext::shared_ptr<SACCR> saccr = run_saccr(portfolio);
    vector<ore::analytics::SACCR::TradeData> tradeData = saccr->tradeData();
    Real tolerance = 0.05;
    BOOST_CHECK(tradeData.size() == expectedResults.size());
    CumulativeNormalDistribution N;
    for (auto td : tradeData) {
        BOOST_TEST_MESSAGE(td.id << " "
            << setprecision(16)
            << td.type << " ["
            << td.nettingSetDetails << "] "
            << td.assetClass << " "
            << td.hedgingSet << " "
            << td.NPV << " "
            << td.npvCcy << " "
            << td.currentNotional << " "
            << td.delta << " "
            << td.d << " "
            << td.MF << " "
            << td.M << " "
            << td.S << " "
            << td.E << " "
            << td.T << " "
            << td.numNominalFlows << " "
            << td.price << " "
            << td.strike );
        bool found = false;
        for (auto r : expectedResults) {
            if (r.id == td.id) {
	      BOOST_TEST_MESSAGE("Checking " << td.id);
                BOOST_CHECK(td.assetClass == r.assetClass);
                BOOST_CHECK(td.hedgingSet == r.hedgingSet);
                BOOST_CHECK(td.hedgingSubset == r.hedgingSubset);
                BOOST_CHECK_CLOSE(td.currentNotional, r.currentNotional, tolerance);
                BOOST_CHECK_CLOSE(td.d, r.d, tolerance);
                BOOST_CHECK_CLOSE(td.S, r.S, tolerance);
                BOOST_CHECK_CLOSE(td.E, r.E, tolerance);
                BOOST_CHECK_CLOSE(td.M, r.M, tolerance);
                BOOST_CHECK_CLOSE(td.MF, r.MF, tolerance);
                BOOST_CHECK_CLOSE(td.T, r.T, tolerance);
                BOOST_CHECK_CLOSE(td.price, r.price, tolerance);
                BOOST_CHECK_CLOSE(td.strike, r.strike, tolerance);
                BOOST_CHECK_CLOSE(td.SD, r.SD, tolerance);
                found = true;
            }
        }
        BOOST_CHECK(found);
    }
}

void SaccrTest::testSACCR_FxPortfolio() {

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;
    // Create the portfolio
    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    
    portfolio->add(testsuite::buildFxOption(
        "FX_CALL_OPTION_EURUSD", "Long", "Call", 2, "EUR", 1000000, "USD", 1150000, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "FX_CALL_OPTION_USDEUR", "Long", "Call", 2, "USD", 1150000, "EUR", 1000000, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "FX_CALL_OPTION_EURGBP", "Long", "Call", 2, "EUR", 9510000, "GBP", 11000000, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "FX_CALL_OPTION_GBPEUR", "Long", "Call", 2, "GBP", 11000000, "EUR", 9510000, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "FX_CALL_OPTION_JPYEUR", "Long", "Call", 3, "JPY", 125000000, "EUR", 1000000, 0, "", "", "NS"));
    portfolio->add(testsuite::buildFxOption(
        "FX_CALL_OPTION_EURJPY", "Long", "Put", 3, "EUR", 1000000, "JPY", 125000000, 0, "", "", "NS"));
    
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_EURUSD", 10, "EUR", 1000000, "USD", 1300000, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_USDEUR", 10, "USD", 1300000, "EUR", 1000000, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_EURGBP", 10, "EUR", 11000000, "GBP", 9000000, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_GBPEUR", 10, "GBP", 9000000, "EUR", 11000000, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_EURJPY", 5, "EUR", 1000000, "JPY", 125000000, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_JPYEUR", 5, "JPY", 125000000, "EUR", 1000000, "NS"));
    portfolio->add(testsuite::buildFxForward(
        "FXFwd_GBPUSD", 10, "GBP", 9700000, "USD", 11000000, "NS"));

    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "XCCY_Swap_EURUSD", "EUR", 30000000, "USD", 33900000, 0, 10, 0.0000, 0.0000, "6M",
        "A360", "EUR-EURIBOR-6M", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "XCCY_Swap_USDGBP", "USD", 33900000, "GBP", 30000000, 0, 10, 0.0000, 0.0000, "6M",
        "A360", "USD-LIBOR-3M", TARGET(), "3M", "A360", "USD-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));
    portfolio->add(testsuite::buildCrossCcyBasisSwap(
        "XCCY_Swap_EURJPY", "EUR", 30000000, "JPY", 33900000, 0, 10, 0.0000, 0.0000, "6M",
        "A360", "EUR-EURIBOR-6M", TARGET(), "3M", "A360", "JPY-LIBOR-3M",
        TARGET(), 2, true, false, false, false, false, false, "NS", false));

    // Get the cached results
    // clang-format off
    vector<ore::analytics::SACCR::TradeData> expectedResults = {
            // id,                    tradeType,   nettingSet,  assetClass,             hedgingSet, hedgingSubset   NPV,            npvCcy, currentNnl,         delta,          d,                  MF,                 M,          S, E, T,        price,      K,          numNominalFlows
            {"FX_CALL_OPTION_EURUSD", "FxOption",  "NS",        SACCR::AssetClass::FX,  "EURUSD",   "",             374277.80,      "USD",  1119700,            0.488418,       1119700,            0.29417,            6.26927,    0, 0, 6.26927,  1.1127,     1.15,       0},
            {"FX_CALL_OPTION_USDEUR", "FxOption",  "NS",        SACCR::AssetClass::FX,  "EURUSD",   "",             358379.35,      "USD",  1119700,            -0.51158,       1119700,            0.29417,            6.26927,    0, 0, 6.26927,  0.89841,    0.86957,    0},
            {"FX_CALL_OPTION_EURGBP", "FxOption",  "NS",        SACCR::AssetClass::FX,  "EURGBP",   "",             6499972.63,     "USD",  14424300,           0.126301,       14424300,           0.29417,            6.26927,    0, 0, 6.26927,  0.9619,     1.15668,    0},
            {"FX_CALL_OPTION_GBPEUR", "FxOption",  "NS",        SACCR::AssetClass::FX,  "EURGBP",   "",             2630982.62,     "USD",  14424300,           -0.8736987,     14424300,           0.29417,            6.26927,    0, 0, 6.26927,  1.03961,    0.864545,   0},
            {"FX_CALL_OPTION_JPYEUR", "FxOption",  "NS",        SACCR::AssetClass::FX,  "EURJPY",   "",             334512.75,      "USD",  1148263.83,         -0.40036698,    1148263.83,         0.29417,            7.27475,    0, 0, 7.27475,  0.007145,   0.008,      0},
            {"FX_CALL_OPTION_EURJPY", "FxOption",  "NS",        SACCR::AssetClass::FX,  "EURJPY",   "",             463916.06,      "USD",  1148263.83,         -0.400367,      1148263.83,         0.29417,            7.27475,    0, 0, 7.27475,  139.96,     125,        0},
            {"FXFwd_EURUSD",          "FxForward", "NS",        SACCR::AssetClass::FX,  "EURUSD",   "",             -69754.27,      "USD",  1119700,            1,              1119700,            0.29417,            14.26927,   0, 0, 0,        0,          0,          0},
            {"FXFwd_USDEUR",          "FxForward", "NS",        SACCR::AssetClass::FX,  "EURUSD",   "",             69571.02,       "USD",  1119700,            -1,             1119700,            0.29417,            14.26927,   0, 0, 0,        0,          0,          0},
            {"FXFwd_EURGBP",          "FxForward", "NS",        SACCR::AssetClass::FX,  "EURGBP",   "",             -1361793.10,    "USD",  12316700,           1,              12316700,           0.29417,            14.26927,   0, 0, 0,        0,          0,          0},
            {"FXFwd_GBPEUR",          "FxForward", "NS",        SACCR::AssetClass::FX,  "EURGBP",   "",             1361793.10,     "USD",  12316700,           -1,             12316700,           0.29417,            14.26927,   0, 0, 0,        0,          0,          0},
            {"FXFwd_EURJPY",          "FxForward", "NS",        SACCR::AssetClass::FX,  "EURJPY",   "",             -144509.22,     "USD",  1148263.825096454,  1,              1148263.83,         0.29417,            9.26927,    0, 0, 0,        0,          0,          0},
            {"FXFwd_JPYEUR",          "FxForward", "NS",        SACCR::AssetClass::FX,  "EURJPY",   "",             144509.22,      "USD",  1148263.825096454,  -1,             1148263.83,         0.29417,            9.26927,    0, 0, 0,        0,          0,          0},
            {"FXFwd_GBPUSD",          "FxForward", "NS",        SACCR::AssetClass::FX,  "GBPUSD",   "",             2515011.19,     "USD",  12719610,           1,              12719610,           0.29417,            14.26927,   0, 0, 0,        0,          0,          0},
            {"XCCY_Swap_EURUSD",      "Swap",      "NS",        SACCR::AssetClass::FX,  "EURUSD",   "",             9872405.39,     "USD",  33591000,           1,              33591000,           0.29417,            14.27475,   0, 0, 0,        0,          0,          0},
            {"XCCY_Swap_USDGBP",      "Swap",      "NS",        SACCR::AssetClass::FX,  "GBPUSD",   "",             -774478.25,     "USD",  39339000.00000001,  -1,             39339000.00000001,  0.29417,            14.27475,   0, 0, 0,        0,          0,          0},
            {"XCCY_Swap_EURJPY",      "Swap",      "NS",        SACCR::AssetClass::FX,  "EURJPY",   "",             11701527.16,    "USD",  33591000,           1,              33591000,           0.2941742027072761, 14.27475,   0, 0, 0,        0,          0,          0}
        }; 
    // clang-format on

    QuantLib::ext::shared_ptr<SACCR> saccr = run_saccr(portfolio);
    vector<ore::analytics::SACCR::TradeData> tradeData = saccr->tradeData();
    Real tolerance = 0.3;
    BOOST_CHECK(tradeData.size() == expectedResults.size());
    CumulativeNormalDistribution N;
    for (auto td : tradeData) {
        BOOST_TEST_MESSAGE(td.id << ", "
            << setprecision(16)
            << td.type << ", ["
            << td.nettingSetDetails << "], "
            << td.assetClass << ", "
            << td.hedgingSet << ", "
            << td.NPV << ", "
            << td.npvCcy << ", "
            << td.currentNotional << ", "
            << td.delta << ", "
            << td.d << ", "
            << td.MF << ", "
            << td.M << ", "
            << td.S << ", "
            << td.E << ", "
            << td.T << ", "
            << td.price << ", "
            << td.strike << ", "
            << td.numNominalFlows
            );
        
        bool found = false;
        for (auto r : expectedResults) {
            if (r.id == td.id) {
                BOOST_CHECK_CLOSE(td.delta, r.delta, tolerance);
                BOOST_CHECK(td.assetClass == r.assetClass);
                BOOST_CHECK(td.hedgingSet == r.hedgingSet);
                BOOST_CHECK_CLOSE(td.currentNotional, r.currentNotional, tolerance);
                BOOST_CHECK_CLOSE(td.d, r.d, tolerance);
                found = true;
            }
        }
        BOOST_CHECK(found);
    }
}

void SaccrTest::testSACCR_FlippedFxOptions() {
    // A ATMF FxOption should return the same EAD even if the trade is flipped
    // i.e An FxOption with BoughtCurrency CCY1, SoldCurrency CCY2 Call is
    // equal to an FxOption with BoughtCurrency CCY2, SoldCurrency CCY1 Put.
    // In this test we check that this is the case.
    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;
    Real tolerance = 1E-8;

    QuantLib::ext::shared_ptr<Portfolio> portfolio = QuantLib::ext::make_shared<Portfolio>();
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<LocalTestMarket>();

    // Test 1: EUR/USD
    Real eurNotional = 1000000;
    Real usdNotional = eurNotional * initMarket->fxIndex("EURUSD")->fixing(today + 2 * Years);
    portfolio->add(testsuite::buildFxOption("FX_CALL_OPTION_EURUSD", "Long", "Call", 2, "EUR", eurNotional,
                                                    "USD", usdNotional, 0, "", "", "NS"));
    BOOST_TEST_MESSAGE("run");
    QuantLib::ext::shared_ptr<SACCR> saccr = run_saccr(portfolio);
    BOOST_TEST_MESSAGE("run ok");
    Real EAD_CALL = saccr->EAD("NS");

    portfolio->clear();
    portfolio->add(testsuite::buildFxOption("FX_PUT_OPTION_USDEUR", "Long", "Put", 2, "USD", usdNotional, "EUR",
                                                    eurNotional, 0, "", "", "NS"));
    saccr = run_saccr(portfolio);
    Real EAD_PUT = saccr->EAD("NS");

    BOOST_CHECK_CLOSE(EAD_CALL, EAD_PUT, tolerance);

    // Test 2: EUR/GBP
    eurNotional = 9510000;
    Real gbpNotional = eurNotional * initMarket->fxIndex("EURGBP")->fixing(TARGET().adjust(today + 2 * Years));
    portfolio->clear();
    portfolio->add(testsuite::buildFxOption("FX_CALL_OPTION_EURGBP", "Long", "Call", 2, "EUR", eurNotional,
                                                    "GBP", gbpNotional, 0, "", "", "NS"));
    saccr = run_saccr(portfolio);
    EAD_CALL = saccr->EAD("NS");

    portfolio->clear();
    portfolio->add(testsuite::buildFxOption("FX_PUT_OPTION_GBPEUR", "Long", "Put", 2, "GBP", gbpNotional, "EUR",
                                                    eurNotional, 0, "", "", "NS"));
    saccr = run_saccr(portfolio);
    EAD_PUT = saccr->EAD("NS");

    BOOST_CHECK_CLOSE(EAD_CALL, EAD_PUT, tolerance);

    // Test 3: JPY/EUR
    Real jpyNotional = 125000000;
    eurNotional = jpyNotional * initMarket->fxIndex("JPYEUR")->fixing(today + 3 * Years);
    portfolio->clear();
    portfolio->add(testsuite::buildFxOption("FX_CALL_OPTION_JPYEUR", "Long", "Call", 3, "JPY", jpyNotional,
                                                    "EUR", eurNotional, 0, "", "", "NS"));
    saccr = run_saccr(portfolio);
    EAD_CALL = saccr->EAD("NS");

    portfolio->clear();
    portfolio->add(testsuite::buildFxOption("FX_PUT_OPTION_EURJPY", "Long", "Put", 3, "EUR", eurNotional, "JPY",
                                                    jpyNotional, 0, "", "", "NS"));
    saccr = run_saccr(portfolio);
    EAD_PUT = saccr->EAD("NS");

    BOOST_CHECK_CLOSE(EAD_CALL, EAD_PUT, tolerance);
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SACCRTest)

BOOST_AUTO_TEST_CASE(HedgingSets){ 
    BOOST_TEST_MESSAGE("Testing SACCR Hedging Sets");
    SaccrTest::testSACCR_HedgingSets();
}

BOOST_AUTO_TEST_CASE(currentNotional) {
    BOOST_TEST_MESSAGE("Testing SACCR Current Notional");
    SaccrTest::testSACCR_CurrentNotional();
}

BOOST_AUTO_TEST_CASE(Delta) {
    BOOST_TEST_MESSAGE("Testing SACCR Delta");
    SaccrTest::testSACCR_Delta();
}

BOOST_AUTO_TEST_CASE(FXPortfolio) {
    BOOST_TEST_MESSAGE("Testing SACCR FX Portfolio");
    SaccrTest::testSACCR_FxPortfolio();
}

BOOST_AUTO_TEST_CASE(FlippedFXOptions) {
    BOOST_TEST_MESSAGE("Testing SACCR Flipped FX Options");
    SaccrTest::testSACCR_FlippedFxOptions();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testsuite
