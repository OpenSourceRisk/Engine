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

#include <test/oredtestmarket.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolcube.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/blackvariancecurve3.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/yoyinflationcurveobserverstatic.hpp>
#include <qle/termstructures/zeroinflationcurveobserverstatic.hpp>

using QuantExt::BlackVarianceCurve3;
using QuantExt::CrossCcyBasisSwapHelper;
using QuantExt::DefaultProbabilityHelper;
using QuantExt::EquityIndex2;
using QuantExt::SwaptionVolCubeWithATM;

OredTestMarket::OredTestMarket(Date asof, bool swapVolCube) : MarketImpl(false) {
    asof_ = asof;

    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();
        
    // add conventions
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURConv(
        new ore::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexEURLongConv(
        new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexUSDConv(
        new ore::data::SwapIndexConvention("USD-CMS-2Y", "USD-3M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexUSDLongConv(
        new ore::data::SwapIndexConvention("USD-CMS-30Y", "USD-3M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexGBPConv(
        new ore::data::SwapIndexConvention("GBP-CMS-2Y", "GBP-3M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexGBPLongConv(
        new ore::data::SwapIndexConvention("GBP-CMS-30Y", "GBP-6M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexCHFConv(
        new ore::data::SwapIndexConvention("CHF-CMS-2Y", "CHF-3M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexCHFLongConv(
        new ore::data::SwapIndexConvention("CHF-CMS-30Y", "CHF-6M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexJPYConv(
        new ore::data::SwapIndexConvention("JPY-CMS-2Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexJPYLongConv(
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

    QuantLib::ext::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
        "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapUSDConv(
        new ore::data::IRSwapConvention("USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360", "USD-LIBOR-3M"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapGBPConv(
        new ore::data::IRSwapConvention("GBP-3M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-3M"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapGBPLongConv(
        new ore::data::IRSwapConvention("GBP-6M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-6M"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapCHFConv(
        new ore::data::IRSwapConvention("CHF-3M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-3M"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapCHFLongConv(
        new ore::data::IRSwapConvention("CHF-6M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-6M"));
    QuantLib::ext::shared_ptr<ore::data::Convention> swapJPYConv(new ore::data::IRSwapConvention(
        "JPY-LIBOR-6M-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M"));

    conventions->add(swapEURConv);
    conventions->add(swapUSDConv);
    conventions->add(swapGBPConv);
    conventions->add(swapGBPLongConv);
    conventions->add(swapCHFConv);
    conventions->add(swapCHFLongConv);
    conventions->add(swapJPYConv);

    InstrumentConventions::instance().setConventions(conventions);
    
    // build discount
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.02);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = flatRateYts(0.03);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")] = flatRateYts(0.04);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "CHF")] = flatRateYts(0.01);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] = flatRateYts(0.005);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "CAD")] = flatRateYts(0.005);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "SEK")] = flatRateYts(0.005);

    // build ibor indices
    vector<pair<string, Real>> indexData = {
        {"EUR-EONIA", 0.01},    {"EUR-EURIBOR-3M", 0.015}, {"EUR-EURIBOR-6M", 0.02}, {"USD-FedFunds", 0.01},
        {"USD-LIBOR-1M", 0.02}, {"USD-LIBOR-3M", 0.03},    {"USD-LIBOR-6M", 0.05},   {"GBP-SONIA", 0.01},
        {"GBP-LIBOR-3M", 0.03}, {"GBP-LIBOR-6M", 0.04},    {"CHF-LIBOR-3M", 0.01},   {"CHF-TOIS", 0.02},
        {"CHF-LIBOR-6M", 0.02}, {"JPY-LIBOR-6M", 0.01},    {"JPY-TONAR", 0.01},      {"JPY-LIBOR-3M", 0.01},
        {"CAD-CDOR-3M", 0.02},  {"CAD-CORRA", 0.01},       {"SEK-STIBOR-3M", 0.02}};

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
    std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
    quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
    quotes["EURGBP"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.8));
    quotes["EURCHF"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    quotes["EURCAD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    quotes["EURSEK"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    quotes["EURJPY"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(128.0));
    fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

    // build fx vols
    fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.12);
    fxVols_[make_pair(Market::defaultConfiguration, "EURGBP")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "EURCHF")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "EURJPY")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "GBPCHF")] = flatRateFxv(0.15);
    // Add Equity Spots
    equitySpots_[make_pair(Market::defaultConfiguration, "SP5")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(2147.56));
    equitySpots_[make_pair(Market::defaultConfiguration, "Lufthansa")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(12.75));

    equityVols_[make_pair(Market::defaultConfiguration, "SP5")] = flatRateFxv(0.2514);
    equityVols_[make_pair(Market::defaultConfiguration, "Lufthansa")] = flatRateFxv(0.30);

    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "SP5")] = flatRateDiv(0.01);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "Lufthansa")] =
        flatRateDiv(0.0);

    equityCurves_[make_pair(Market::defaultConfiguration, "SP5")] = Handle<EquityIndex2>(QuantLib::ext::make_shared<EquityIndex2>(
        "SP5", UnitedStates(UnitedStates::Settlement), parseCurrency("USD"), equitySpot("SP5"), yieldCurve(YieldCurveType::Discount, "USD"),
        yieldCurve(YieldCurveType::EquityDividend, "SP5")));
    equityCurves_[make_pair(Market::defaultConfiguration, "Lufthansa")] =
        Handle<EquityIndex2>(QuantLib::ext::make_shared<EquityIndex2>(
            "Lufthansa", TARGET(), parseCurrency("EUR"), equitySpot("Lufthansa"),
            yieldCurve(YieldCurveType::Discount, "EUR"), yieldCurve(YieldCurveType::EquityDividend, "Lufthansa")));

    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "EUR")] = std::make_pair("EUR-CMS-2Y", "EUR-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "USD")] = std::make_pair("USD-CMS-2Y", "USD-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "GBP")] = std::make_pair("GBP-CMS-2Y", "GBP-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "CHF")] = std::make_pair("CHF-CMS-2Y", "CHF-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "JPY")] = std::make_pair("JPY-CMS-2Y", "JPY-CMS-30Y");

    // build swaption vols

    if (swapVolCube) {
        vector<Real> shiftStrikes = {-0.02, -0.01, -0.005, -0.0025, 0.0, 0.0025, 0.005, 0.01, 0.02};
        vector<Period> optionTenors = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years,
                                       3 * Years, 5 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};
        vector<Period> swapTenors = {1 * Years, 2 * Years,  3 * Years,  4 * Years,  5 * Years,
                                     7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years};
        DayCounter dc = Actual365Fixed();
        Calendar cal = TARGET();
        BusinessDayConvention bdc = Following;
        vector<vector<Handle<Quote>>> parQuotes(
            optionTenors.size(),
            vector<Handle<Quote>>(swapTenors.size(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02))));

        vector<vector<Real>> shift(optionTenors.size(), vector<Real>(swapTenors.size(), 0.0));
        vector<string> ccys = {"USD", "JPY"};
        QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atm(new SwaptionVolatilityMatrix(
            asof_, cal, bdc, optionTenors, swapTenors, parQuotes, dc, true, QuantLib::Normal, shift));

        Handle<SwaptionVolatilityStructure> hATM(atm);
        vector<vector<Handle<Quote>>> cubeQuotes(
            optionTenors.size() * swapTenors.size(),
            vector<Handle<Quote>>(shiftStrikes.size(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02))));

        for (auto name : ccys) {
            Handle<SwapIndex> si = swapIndex(swapIndexBase(name));
            Handle<SwapIndex> ssi = swapIndex(shortSwapIndexBase(name));

            QuantLib::ext::shared_ptr<SwaptionVolatilityCube> tmp(new QuantExt::SwaptionVolCube2(
                hATM, optionTenors, swapTenors, shiftStrikes, cubeQuotes, *si, *ssi, false, true, false));
            tmp->enableExtrapolation();

            Handle<SwaptionVolatilityStructure> svp =
                Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<SwaptionVolCubeWithATM>(tmp));
            swaptionCurves_[make_pair(Market::defaultConfiguration, name)] = svp;
        }

    } else {
        swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateSvs(0.20);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateSvs(0.30);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "GBP")] = flatRateSvs(0.25);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "CHF")] = flatRateSvs(0.25);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateSvs(0.25);
    }

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

    recoveryRates_[make_pair(Market::defaultConfiguration, "dc")] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "dc2")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "BondIssuer1")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));

    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BondCurve1")] = flatRateYts(0.05);

    securitySpreads_[make_pair(Market::defaultConfiguration, "Bond1")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));

    cdsVols_[make_pair(Market::defaultConfiguration, "dc")] =
        Handle<QuantExt::CreditVolCurve>(QuantLib::ext::make_shared<QuantExt::CreditVolCurveWrapper>(flatRateFxv(0.12)));
}

Handle<YieldTermStructure> OredTestMarket::flatRateYts(Real forward) {
    QuantLib::ext::shared_ptr<YieldTermStructure> yts(
        new FlatForward(Settings::instance().evaluationDate(), forward, ActualActual(ActualActual::ISDA)));
    return Handle<YieldTermStructure>(yts);
}

Handle<YieldTermStructure> OredTestMarket::flatRateDiv(Real dividend) {
    QuantLib::ext::shared_ptr<YieldTermStructure> yts(
        new FlatForward(Settings::instance().evaluationDate(), dividend, ActualActual(ActualActual::ISDA)));
    return Handle<YieldTermStructure>(yts);
}

Handle<BlackVolTermStructure> OredTestMarket::flatRateFxv(Volatility forward) {
    QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(
        new BlackConstantVol(Settings::instance().evaluationDate(), NullCalendar(), forward, Actual365Fixed()));
    return Handle<BlackVolTermStructure>(fxv);
}

Handle<QuantLib::SwaptionVolatilityStructure> OredTestMarket::flatRateSvs(Volatility forward, VolatilityType type,
                                                                          Real shift) {
    QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
        new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                 ModifiedFollowing, forward, Actual365Fixed(), type, shift));
    return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
}

Handle<QuantExt::CreditCurve> OredTestMarket::flatRateDcs(Volatility forward) {
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> dcs(new FlatHazardRate(asof_, forward, ActualActual(ActualActual::ISDA)));
    return Handle<QuantExt::CreditCurve>(
        QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(dcs)));
}

Handle<OptionletVolatilityStructure> OredTestMarket::flatRateCvs(Volatility vol, VolatilityType type, Real shift) {
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ts(new QuantLib::ConstantOptionletVolatility(
        Settings::instance().evaluationDate(), NullCalendar(), ModifiedFollowing, vol, ActualActual(ActualActual::ISDA), type, shift));
    return Handle<OptionletVolatilityStructure>(ts);
}
