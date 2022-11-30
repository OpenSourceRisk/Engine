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
#include <ql/currencies/america.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/pricecurve.hpp>

#include <test/testmarket.hpp>

using QuantExt::CommodityIndex;
using QuantExt::CommoditySpotIndex;
using QuantExt::EquityIndex;
using QuantExt::InterpolatedPriceCurve;
using QuantExt::PriceTermStructure;

namespace testsuite {

TestMarket::TestMarket(Date asof) : MarketImpl(false) {

    TestConfigurationObjects::setConventions();
    
    asof_ = asof;

    // build discount
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.02);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.03);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] = flatRateYts(0.04);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "CHF")] = flatRateYts(0.01);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] = flatRateYts(0.005);

    // build ibor indices
    vector<pair<string, Real>> indexData = {{"EUR-EONIA", 0.01},    {"EUR-EURIBOR-6M", 0.02}, {"USD-FedFunds", 0.01},
                                            {"USD-LIBOR-3M", 0.03}, {"USD-LIBOR-6M", 0.05},   {"GBP-SONIA", 0.01},
                                            {"GBP-LIBOR-3M", 0.03}, {"GBP-LIBOR-6M", 0.04},   {"CHF-LIBOR-3M", 0.01},
                                            {"CHF-LIBOR-6M", 0.02}, {"JPY-LIBOR-6M", 0.01}};
    for (auto id : indexData) {
        Handle<IborIndex> h(parseIborIndex(id.first, flatRateYts(id.second)));
        iborIndices_[make_pair(Market::defaultConfiguration, id.first)] = h;

        // set up dummy fixings for the past 400 days
        for (Date d = asof - 400; d < asof; d++) {
            if (h->isValidFixingDate(d))
                h->addFixing(d, 0.01);
        }
    }

    // swap index
    addSwapIndex("EUR-CMS-2Y", "EUR-EONIA", Market::defaultConfiguration);
    addSwapIndex("EUR-CMS-30Y", "EUR-EONIA", Market::defaultConfiguration);
    addSwapIndex("USD-CMS-2Y", "USD-FedFunds", Market::defaultConfiguration);
    addSwapIndex("USD-CMS-30Y", "USD-FedFunds", Market::defaultConfiguration);
    addSwapIndex("GBP-CMS-2Y", "GBP-SONIA", Market::defaultConfiguration);
    addSwapIndex("GBP-CMS-30Y", "GBP-SONIA", Market::defaultConfiguration);
    addSwapIndex("CHF-CMS-2Y", "CHF-LIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("CHF-CMS-30Y", "CHF-LIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("JPY-CMS-2Y", "JPY-LIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("JPY-CMS-30Y", "JPY-LIBOR-6M", Market::defaultConfiguration);

    // add fx rates
    std::map<std::string, Handle<Quote>> quotes;
    quotes["EURUSD"] = Handle<Quote>(boost::make_shared<SimpleQuote>(1.2));
    quotes["EURGBP"] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.8));
    quotes["EURCHF"] = Handle<Quote>(boost::make_shared<SimpleQuote>(1.0));
    quotes["EURJPY"] = Handle<Quote>(boost::make_shared<SimpleQuote>(128.0));
    fx_ = boost::make_shared<FXTriangulation>(quotes);

    // build fx vols
    fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.12);
    fxVols_[make_pair(Market::defaultConfiguration, "EURGBP")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "EURCHF")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "EURJPY")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "GBPCHF")] = flatRateFxv(0.15);

    // Add Equity Spots
    equitySpots_[make_pair(Market::defaultConfiguration, "SP5")] =
        Handle<Quote>(boost::make_shared<SimpleQuote>(2147.56));
    equitySpots_[make_pair(Market::defaultConfiguration, "Lufthansa")] =
        Handle<Quote>(boost::make_shared<SimpleQuote>(12.75));

    equityVols_[make_pair(Market::defaultConfiguration, "SP5")] = flatRateFxv(0.2514);
    equityVols_[make_pair(Market::defaultConfiguration, "Lufthansa")] = flatRateFxv(0.30);

    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "SP5")] = flatRateDiv(0.01);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "Lufthansa")] =
        flatRateDiv(0.0);

    equityCurves_[make_pair(Market::defaultConfiguration, "SP5")] = Handle<EquityIndex>(boost::make_shared<EquityIndex>(
        "SP5", UnitedStates(UnitedStates::Settlement), parseCurrency("USD"), equitySpot("SP5"), yieldCurve(YieldCurveType::Discount, "USD"),
        yieldCurve(YieldCurveType::EquityDividend, "SP5")));
    equityCurves_[make_pair(Market::defaultConfiguration, "Lufthansa")] =
        Handle<EquityIndex>(boost::make_shared<EquityIndex>(
            "Lufthansa", TARGET(), parseCurrency("EUR"), equitySpot("Lufthansa"),
            yieldCurve(YieldCurveType::Discount, "EUR"), yieldCurve(YieldCurveType::EquityDividend, "Lufthansa")));

    // build swaption vols
    swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateSvs(0.20);
    swaptionCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateSvs(0.30);
    swaptionCurves_[make_pair(Market::defaultConfiguration, "GBP")] = flatRateSvs(0.25);
    swaptionCurves_[make_pair(Market::defaultConfiguration, "CHF")] = flatRateSvs(0.25);
    swaptionCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateSvs(0.25);
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "EUR")] = std::make_pair("EUR-CMS-2Y", "EUR-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "USD")] = std::make_pair("USD-CMS-2Y", "USD-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "GBP")] = std::make_pair("GBP-CMS-2Y", "GBP-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "CHF")] = std::make_pair("CHF-CMS-2Y", "CHF-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "JPY")] = std::make_pair("JPY-CMS-2Y", "JPY-CMS-30Y");

    // build cap/floor vol structures
    capFloorCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateCvs(0.0050, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateCvs(0.0060, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "GBP")] = flatRateCvs(0.0055, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "CHF")] = flatRateCvs(0.0045, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateCvs(0.0040, Normal);

    // build default curves
    defaultCurves_[make_pair(Market::defaultConfiguration, "dc")] = flatRateDcs(0.1);
    defaultCurves_[make_pair(Market::defaultConfiguration, "dc2")] = flatRateDcs(0.2);
    defaultCurves_[make_pair(Market::defaultConfiguration, "BondIssuer1")] = flatRateDcs(0.0);

    recoveryRates_[make_pair(Market::defaultConfiguration, "dc")] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "dc2")] =
        Handle<Quote>(boost::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "BondIssuer1")] =
        Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));

    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BondCurve1")] = flatRateYts(0.05);

    securitySpreads_[make_pair(Market::defaultConfiguration, "Bond1")] =
        Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));

    Handle<IborIndex> hGBP(ore::data::parseIborIndex(
        "GBP-LIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")]));
    iborIndices_[make_pair(Market::defaultConfiguration, "GBP-LIBOR-6M")] = hGBP;

    // build UKRPI fixing history
    Date cpiFixingEnd(1, asof_.month(), asof_.year());
    Date cpiFixingStart = cpiFixingEnd - Period(14, Months);
    Schedule fixingDatesUKRPI = MakeSchedule().from(cpiFixingStart).to(cpiFixingEnd).withTenor(1 * Months);
    Real fixingRatesUKRPI[] = {258.5, 258.9, 258.6, 259.8, 259.6, 259.5, 259.8, 260.6,
                               258.8, 260.0, 261.1, 261.4, 262.1, 264.3, 265.2};

    // build UKRPI index
    boost::shared_ptr<ZeroInflationIndex> ii = parseZeroInflationIndex("UKRPI");
    boost::shared_ptr<YoYInflationIndex> yi = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(ii, false);

    RelinkableHandle<ZeroInflationTermStructure> hcpi;
    bool interp = false;
    ii = boost::shared_ptr<UKRPI>(new UKRPI(interp, hcpi));
    for (Size i = 0; i < fixingDatesUKRPI.size(); i++) {
        // std::cout << i << ", " << fixingDatesUKRPI[i] << ", " << fixingRatesUKRPI[i] << std::endl;
        ii->addFixing(fixingDatesUKRPI[i], fixingRatesUKRPI[i], true);
    };

    // build EUHICPXT fixing history
    Schedule fixingDatesEUHICPXT = MakeSchedule().from(cpiFixingStart).to(cpiFixingEnd).withTenor(1 * Months);
    Real fixingRatesEUHICPXT[] = {258.5, 258.9, 258.6, 259.8, 259.6, 259.5, 259.8, 260.6,
                                  258.8, 260.0, 261.1, 261.4, 262.1, 264.3, 265.2};

    // build EUHICPXT index
    boost::shared_ptr<ZeroInflationIndex> euii = parseZeroInflationIndex("EUHICPXT");
    boost::shared_ptr<YoYInflationIndex> euyi = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(euii, false);

    RelinkableHandle<ZeroInflationTermStructure> euhcpi;
    interp = false;
    euii = boost::shared_ptr<EUHICPXT>(new EUHICPXT(interp, euhcpi));
    for (Size i = 0; i < fixingDatesEUHICPXT.size(); i++) {
        // std::cout << i << ", " << fixingDatesUKRPI[i] << ", " << fixingRatesUKRPI[i] << std::endl;
        euii->addFixing(fixingDatesEUHICPXT[i], fixingRatesEUHICPXT[i], true);
    };

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

    zeroInflationIndices_[make_pair(Market::defaultConfiguration, "EUHICPXT")] =
        makeZeroInflationIndex("EUHICPXT", datesZCII, ratesZCII, euii,
                               yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")]);
    zeroInflationIndices_[make_pair(Market::defaultConfiguration, "UKRPI")] =
        makeZeroInflationIndex("UKRPI", datesZCII, ratesZCII, ii,
                               yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")]);
    yoyInflationIndices_[make_pair(Market::defaultConfiguration, "UKRPI")] =
        makeYoYInflationIndex("UKRPI", datesZCII, ratesZCII, yi,
                              yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")]);

    cpiInflationCapFloorVolatilitySurfaces_[make_pair(Market::defaultConfiguration, "EUHICPXT")] =
        flatCpiVolSurface(0.05);
    cpiInflationCapFloorVolatilitySurfaces_[make_pair(Market::defaultConfiguration, "UKRPI")] = flatCpiVolSurface(0.04);

    // Commodity price curves and spots
    Actual365Fixed ccDayCounter;
    vector<Period> commTenors = {0 * Days, 365 * Days, 730 * Days, 1825 * Days};

    // Gold curve
    vector<Real> prices = {1155.593, 1160.9, 1168.1, 1210};
    Handle<PriceTermStructure> ptsGold(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        commTenors, prices, ccDayCounter, USDCurrency()));
    ptsGold->enableExtrapolation();
    commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_GOLD_USD")] = Handle<CommodityIndex>(
        boost::make_shared<CommoditySpotIndex>("COMDTY_GOLD_USD", NullCalendar(), ptsGold));

    // WTI Oil curve
    prices = {30.89, 41.23, 44.44, 49.18};
    Handle<PriceTermStructure> ptsOil(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        commTenors, prices, ccDayCounter, USDCurrency()));
    ptsOil->enableExtrapolation();
    commodityIndices_[make_pair(Market::defaultConfiguration, "COMDTY_WTI_USD")] = Handle<CommodityIndex>(
        boost::make_shared<CommoditySpotIndex>("COMDTY_WTI_USD", NullCalendar(), ptsOil));

    // Commodity volatilities
    commodityVols_[make_pair(Market::defaultConfiguration, "COMDTY_GOLD_USD")] = flatRateFxv(0.15);
    commodityVols_[make_pair(Market::defaultConfiguration, "COMDTY_WTI_USD")] = flatRateFxv(0.20);

    // Correlations
    correlationCurves_[make_tuple(Market::defaultConfiguration, "EUR-CMS-10Y", "EUR-CMS-1Y")] = flatCorrelation(0.15);
    correlationCurves_[make_tuple(Market::defaultConfiguration, "USD-CMS-10Y", "USD-CMS-1Y")] = flatCorrelation(0.2);
}

Handle<ZeroInflationIndex> TestMarket::makeZeroInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                              boost::shared_ptr<ZeroInflationIndex> ii,
                                                              Handle<YieldTermStructure> yts) {
    // build UKRPI index

    boost::shared_ptr<ZeroInflationTermStructure> cpiTS;
    // now build the helpers ...
    vector<boost::shared_ptr<BootstrapHelper<ZeroInflationTermStructure>>> instruments;
    for (Size i = 0; i < dates.size(); i++) {
        Handle<Quote> quote(boost::shared_ptr<Quote>(new SimpleQuote(rates[i] / 100.0)));
        boost::shared_ptr<BootstrapHelper<ZeroInflationTermStructure>> anInstrument(new ZeroCouponInflationSwapHelper(
            quote, Period(2, Months), dates[i], TARGET(), ModifiedFollowing, ActualActual(ActualActual::ISDA), ii, CPI::AsIndex, yts));
        anInstrument->unregisterWith(Settings::instance().evaluationDate());
        instruments.push_back(anInstrument);
    };
    // we can use historical or first ZCIIS for this
    // we know historical is WAY off market-implied, so use market implied flat.
    Rate baseZeroRate = rates[0] / 100.0;
    boost::shared_ptr<PiecewiseZeroInflationCurve<Linear>> pCPIts(
        new PiecewiseZeroInflationCurve<Linear>(asof_, TARGET(), ActualActual(ActualActual::ISDA), Period(2, Months), ii->frequency(),
                                                baseZeroRate, instruments));
    pCPIts->recalculate();
    cpiTS = boost::dynamic_pointer_cast<ZeroInflationTermStructure>(pCPIts);
    cpiTS->enableExtrapolation(true);
    cpiTS->unregisterWith(Settings::instance().evaluationDate());
    return Handle<ZeroInflationIndex>(parseZeroInflationIndex(index, false, Handle<ZeroInflationTermStructure>(cpiTS)));
}

Handle<YoYInflationIndex> TestMarket::makeYoYInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                            boost::shared_ptr<YoYInflationIndex> ii,
                                                            Handle<YieldTermStructure> yts) {
    // build UKRPI index

    boost::shared_ptr<YoYInflationTermStructure> yoyTS;
    // now build the helpers ...
    vector<boost::shared_ptr<BootstrapHelper<YoYInflationTermStructure>>> instruments;
    for (Size i = 0; i < dates.size(); i++) {
        Handle<Quote> quote(boost::shared_ptr<Quote>(new SimpleQuote(rates[i] / 100.0)));
        boost::shared_ptr<BootstrapHelper<YoYInflationTermStructure>> anInstrument(new YearOnYearInflationSwapHelper(
            quote, Period(2, Months), dates[i], TARGET(), ModifiedFollowing, ActualActual(ActualActual::ISDA), ii, yts));
        instruments.push_back(anInstrument);
    };
    // we can use historical or first ZCIIS for this
    // we know historical is WAY off market-implied, so use market implied flat.
    Rate baseZeroRate = rates[0] / 100.0;
    boost::shared_ptr<PiecewiseYoYInflationCurve<Linear>> pYoYts(
        new PiecewiseYoYInflationCurve<Linear>(asof_, TARGET(), ActualActual(ActualActual::ISDA), Period(2, Months), ii->frequency(),
                                               ii->interpolated(), baseZeroRate, instruments));
    pYoYts->recalculate();
    yoyTS = boost::dynamic_pointer_cast<YoYInflationTermStructure>(pYoYts);
    return Handle<YoYInflationIndex>(boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
        parseZeroInflationIndex(index, false), false, Handle<YoYInflationTermStructure>(pYoYts)));
}

void TestConfigurationObjects::setConventions() {
    boost::shared_ptr<Conventions> conventions = boost::make_shared<Conventions>();

    // add conventions
    boost::shared_ptr<ore::data::Convention> swapIndexEURConv(
        new ore::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexEURLongConv(
        new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexUSDConv(
        new ore::data::SwapIndexConvention("USD-CMS-2Y", "USD-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexUSDLongConv(
        new ore::data::SwapIndexConvention("USD-CMS-30Y", "USD-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexGBPConv(
        new ore::data::SwapIndexConvention("GBP-CMS-2Y", "GBP-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexGBPLongConv(
        new ore::data::SwapIndexConvention("GBP-CMS-30Y", "GBP-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexCHFConv(
        new ore::data::SwapIndexConvention("CHF-CMS-2Y", "CHF-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexCHFLongConv(
        new ore::data::SwapIndexConvention("CHF-CMS-30Y", "CHF-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexJPYConv(
        new ore::data::SwapIndexConvention("JPY-CMS-2Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexJPYLongConv(
        new ore::data::SwapIndexConvention("JPY-CMS-30Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));

    conventions->add(swapIndexEURConv);
    conventions->add(swapIndexEURLongConv);
    conventions->add(swapIndexUSDConv);
    conventions->add(swapIndexUSDLongConv);
    conventions->add(swapIndexGBPConv);
    conventions->add(swapIndexGBPLongConv);
    conventions->add(swapIndexCHFConv);
    conventions->add(swapIndexCHFLongConv);
    conventions->add(swapIndexJPYConv);
    conventions->add(swapIndexJPYLongConv);

    boost::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
        "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    boost::shared_ptr<ore::data::Convention> swapUSDConv(
        new ore::data::IRSwapConvention("USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360", "USD-LIBOR-3M"));
    boost::shared_ptr<ore::data::Convention> swapGBPConv(
        new ore::data::IRSwapConvention("GBP-3M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-3M"));
    boost::shared_ptr<ore::data::Convention> swapGBPLongConv(
        new ore::data::IRSwapConvention("GBP-6M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-6M"));
    boost::shared_ptr<ore::data::Convention> swapCHFConv(
        new ore::data::IRSwapConvention("CHF-3M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-3M"));
    boost::shared_ptr<ore::data::Convention> swapCHFLongConv(
        new ore::data::IRSwapConvention("CHF-6M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-6M"));
    boost::shared_ptr<ore::data::Convention> swapJPYConv(new ore::data::IRSwapConvention(
        "JPY-LIBOR-6M-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M"));

    conventions->add(swapEURConv);
    conventions->add(swapUSDConv);
    conventions->add(swapGBPConv);
    conventions->add(swapGBPLongConv);
    conventions->add(swapCHFConv);
    conventions->add(swapCHFLongConv);
    conventions->add(swapJPYConv);

    conventions->add(boost::make_shared<ore::data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(boost::make_shared<ore::data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    conventions->add(boost::make_shared<FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000", "EUR,USD"));
    conventions->add(boost::make_shared<FXConvention>("EUR-GBP-FX", "0", "EUR", "GBP", "10000", "EUR,GBP"));
    conventions->add(boost::make_shared<FXConvention>("EUR-CHF-FX", "0", "EUR", "CHF", "10000", "EUR,CHF"));
    conventions->add(boost::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));

    InstrumentConventions::instance().setConventions(conventions);
}

boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> TestConfigurationObjects::setupSimMarketData2() {
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP"});
    simMarketData->setYieldCurveNames({"BondCurve1"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 6 * Years, 7 * Years, 8 * Years, 9 * Years, 10 * Years,
                                            12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years});
    simMarketData->setIndices({"EUR-EURIBOR-6M", "GBP-LIBOR-6M"});
    simMarketData->setDefaultNames({"BondIssuer1"});
    simMarketData->setDefaultTenors(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setSecurities({"Bond1"});
    simMarketData->setSimulateSurvivalProbabilities(true);
    simMarketData->setDefaultCurveCalendars("", "TARGET");
    simMarketData->interpolation() = "LogLinear";

    simMarketData->setSwapVolTerms(
        "", {1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true);

    simMarketData->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true);
    simMarketData->setFxVolCcyPairs({"EURGBP"});
    simMarketData->setFxVolIsSurface(true);
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.2, 0.3, 0.5, 1, 2, 3});

    simMarketData->setFxCcyPairs({"EURGBP"});

    simMarketData->setSimulateCapFloorVols(false);

    return simMarketData;
}

boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> TestConfigurationObjects::setupSimMarketData5() {
    boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices(
        {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    simMarketData->swapIndices()["EUR-CMS-2Y"] = "EUR-EURIBOR-6M";
    simMarketData->swapIndices()["EUR-CMS-30Y"] = "EUR-EURIBOR-6M";

    simMarketData->setYieldCurveNames({"BondCurve1"});
    simMarketData->interpolation() = "LogLinear";

    simMarketData->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true); // false;

    simMarketData->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true); // false;
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"});
    simMarketData->setFxVolIsSurface(true);
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.2, 0.3, 0.5, 1, 2, 3});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    simMarketData->setDefaultNames({"BondIssuer1"});
    simMarketData->setDefaultTenors(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setSimulateSurvivalProbabilities(true);
    simMarketData->setSecurities({"Bond1"});
    simMarketData->setDefaultCurveCalendars("", "TARGET");

    simMarketData->setEquityNames({"SP5", "Lufthansa"});
    simMarketData->setEquityDividendTenors("SP5", {6 * Months, 1 * Years, 2 * Years});
    simMarketData->setEquityDividendTenors("Lufthansa", {6 * Months, 1 * Years, 2 * Years});

    simMarketData->setSimulateEquityVols(true);
    simMarketData->setEquityVolDecayMode("ForwardVariance");
    simMarketData->setEquityVolNames({"SP5", "Lufthansa"});
    simMarketData->setEquityVolExpiries("", {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                          5 * Years,  7 * Years, 10 * Years, 20 * Years});
    simMarketData->setEquityVolIsSurface("", false);
    simMarketData->setSimulateEquityVolATMOnly(true);
    simMarketData->setEquityVolMoneyness("", {1});

    simMarketData->setZeroInflationIndices({"UKRPI"});
    simMarketData->setZeroInflationTenors(
        "UKRPI", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setYoyInflationIndices({"UKRPI"});
    simMarketData->setYoyInflationTenors(
        "UKRPI", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});

    simMarketData->setCommodityCurveSimulate(true);
    simMarketData->setCommodityNames({"COMDTY_GOLD_USD", "COMDTY_WTI_USD"});
    simMarketData->setCommodityCurveTenors("", {0 * Days, 1 * Years, 2 * Years, 5 * Years});

    simMarketData->setCommodityVolSimulate(true);
    simMarketData->commodityVolDecayMode() = "ForwardVariance";
    simMarketData->setCommodityVolNames({"COMDTY_GOLD_USD", "COMDTY_WTI_USD"});
    simMarketData->commodityVolExpiries("COMDTY_GOLD_USD") = {1 * Years, 2 * Years, 5 * Years};
    simMarketData->commodityVolMoneyness("COMDTY_GOLD_USD") = {1.0};
    simMarketData->commodityVolExpiries("COMDTY_WTI_USD") = {1 * Years, 5 * Years};
    simMarketData->commodityVolMoneyness("COMDTY_WTI_USD") = {0.9, 0.95, 1.0, 1.05, 1.1};

    return simMarketData;
}

boost::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData2() {
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        boost::make_shared<ore::analytics::SensitivityScenarioData>();

    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                           7 * Years, 10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.05};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

    sensiData->discountCurveShiftData()["EUR"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->yieldCurveShiftData()["BondCurve1"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    sensiData->creditCurveShiftData()["BondIssuer1"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
    // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
    // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

    return sensiData;
}

boost::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData2b() {
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        boost::make_shared<ore::analytics::SensitivityScenarioData>();

    // shift curve has more points than underlying curve, has tenor points where the underlying curve hasn't, skips some
    // tenor points that occur in the underlying curve (e.g. 2Y, 3Y, 4Y)
    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {1 * Years,   15 * Months, 18 * Months, 21 * Months, 27 * Months, 30 * Months, 33 * Months,
                           42 * Months, 54 * Months, 5 * Years,   6 * Years,   7 * Years,   8 * Years,   9 * Years,
                           10 * Years,  11 * Years,  12 * Years,  13 * Years,  14 * Years,  15 * Years,  16 * Years,
                           17 * Years,  18 * Years,  19 * Years,  20 * Years};
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.05};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

    sensiData->discountCurveShiftData()["EUR"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->yieldCurveShiftData()["BondCurve1"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    sensiData->creditCurveShiftData()["BondIssuer1"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
    // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
    // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

    return sensiData;
}

boost::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData5() {
    boost::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        boost::make_shared<ore::analytics::SensitivityScenarioData>();

    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {2 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {5 * Years, 10 * Years};

    ore::analytics::SensitivityScenarioData::SpotShiftData eqsData;
    eqsData.shiftType = "Relative";
    eqsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData eqvsData;
    eqvsData.shiftType = "Relative";
    eqvsData.shiftSize = 0.01;
    eqvsData.shiftExpiries = {5 * Years};

    ore::analytics::SensitivityScenarioData::CurveShiftData zinfData;
    zinfData.shiftType = "Absolute";
    zinfData.shiftSize = 0.0001;
    zinfData.shiftTenors = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years};

    auto commodityShiftData = boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>();
    commodityShiftData->shiftType = "Relative";
    commodityShiftData->shiftSize = 0.01;
    commodityShiftData->shiftTenors = {0 * Days, 1 * Years, 2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::VolShiftData commodityVolShiftData;
    commodityVolShiftData.shiftType = "Relative";
    commodityVolShiftData.shiftSize = 0.01;
    commodityVolShiftData.shiftExpiries = {1 * Years, 2 * Years, 5 * Years};
    commodityVolShiftData.shiftStrikes = {0.9, 0.95, 1.0, 1.05, 1.1};

    sensiData->discountCurveShiftData()["EUR"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["USD"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["JPY"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["CHF"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->yieldCurveShiftData()["BondCurve1"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->creditCurveShiftData()["BondIssuer1"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolShiftData()["EUR"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["EUR"]->indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["USD"]->indexName = "USD-LIBOR-3M";

    sensiData->equityShiftData()["SP5"] = eqsData;
    sensiData->equityShiftData()["Lufthansa"] = eqsData;

    sensiData->equityVolShiftData()["SP5"] = eqvsData;
    sensiData->equityVolShiftData()["Lufthansa"] = eqvsData;

    sensiData->zeroInflationCurveShiftData()["UKRPI"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(zinfData);

    sensiData->yoyInflationCurveShiftData()["UKRPI"] =
        boost::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(zinfData);

    sensiData->commodityCurveShiftData()["COMDTY_GOLD_USD"] = commodityShiftData;
    sensiData->commodityCurrencies()["COMDTY_GOLD_USD"] = "USD";
    sensiData->commodityCurveShiftData()["COMDTY_WTI_USD"] = commodityShiftData;
    sensiData->commodityCurrencies()["COMDTY_WTI_USD"] = "USD";

    sensiData->commodityVolShiftData()["COMDTY_GOLD_USD"] = commodityVolShiftData;
    sensiData->commodityVolShiftData()["COMDTY_WTI_USD"] = commodityVolShiftData;

    return sensiData;
}

} // namespace testsuite
