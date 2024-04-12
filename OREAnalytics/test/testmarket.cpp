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

#include <ql/currencies/america.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/interpolatedyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/interpolatedzeroinflationcurve.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/iterativebootstrap.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolcube.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/termstructures/blackvariancecurve3.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/yoyinflationcurveobserverstatic.hpp>
#include <qle/termstructures/zeroinflationcurveobserverstatic.hpp>

#include <orea/scenario/sensitivityscenariodata.hpp>

#include <iostream>

#include <test/testmarket.hpp>

using namespace QuantExt;
using ore::analytics::ShiftType;

namespace testsuite {

namespace {

vector<QuantLib::ext::shared_ptr<RateHelper>>
parRateCurveHelpers(const string& ccy, const vector<string>& parInst, const vector<Period>& parTenor,
                    const vector<Handle<Quote>>& parVal,
                    const Handle<YieldTermStructure> exDiscount = Handle<YieldTermStructure>(),
                    // fx spot to base ccy
                    const Handle<Quote>& fxSpot = Handle<Quote>(),
                    // base currency discount for xccy par instruments
                    const Handle<YieldTermStructure>& fgnDiscount = Handle<YieldTermStructure>(),
                    // pointer to the market that is being built
                    ore::data::Market* market = NULL) {

    BOOST_ASSERT(parInst.size() == parTenor.size());
    BOOST_ASSERT(parInst.size() == parVal.size());
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    vector<QuantLib::ext::shared_ptr<RateHelper>> instruments;
    for (Size i = 0; i < parInst.size(); ++i) {
        Handle<Quote> parRateQuote = parVal[i];
        Period tenor = parTenor[i];
        QuantLib::ext::shared_ptr<RateHelper> rateHelper;
        if (parInst[i] == "DEP") {
            QuantLib::ext::shared_ptr<Convention> conv = conventions->get(ccy + "-DEP-CONVENTIONS");
            QuantLib::ext::shared_ptr<DepositConvention> depConv = QuantLib::ext::dynamic_pointer_cast<DepositConvention>(conv);
            std::ostringstream o;
            o << depConv->index() << "-" << io::short_period(tenor);
            string indexName = o.str();
            QuantLib::ext::shared_ptr<IborIndex> index = parseIborIndex(indexName);
            rateHelper = QuantLib::ext::make_shared<DepositRateHelper>(parRateQuote, index);
        } else if (parInst[i] == "FRA") {
            QuantLib::ext::shared_ptr<Convention> conv = conventions->get(ccy + "-FRA-CONVENTIONS");
            QuantLib::ext::shared_ptr<FraConvention> fraConv = QuantLib::ext::dynamic_pointer_cast<FraConvention>(conv);
            BOOST_ASSERT(fraConv);
            BOOST_ASSERT(tenor > fraConv->index()->tenor());
            Period startTenor = tenor - fraConv->index()->tenor();
            rateHelper = QuantLib::ext::make_shared<FraRateHelper>(parRateQuote, startTenor, fraConv->index());
        } else if (parInst[i] == "IRS") {
            QuantLib::ext::shared_ptr<Convention> conv = conventions->get(ccy + "-6M-SWAP-CONVENTIONS");
            QuantLib::ext::shared_ptr<IRSwapConvention> swapConv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv);
            BOOST_ASSERT(swapConv);
            rateHelper = QuantLib::ext::make_shared<SwapRateHelper>(
                parRateQuote, tenor, swapConv->fixedCalendar(), swapConv->fixedFrequency(), swapConv->fixedConvention(),
                swapConv->fixedDayCounter(), swapConv->index(), Handle<Quote>(), 0 * Days, exDiscount);
        } else if (parInst[i] == "OIS") {
            QuantLib::ext::shared_ptr<Convention> conv = conventions->get(ccy + "-OIS-CONVENTIONS");
            QuantLib::ext::shared_ptr<OisConvention> oisConv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv);
            BOOST_ASSERT(oisConv);
            rateHelper = QuantLib::ext::make_shared<QuantExt::OISRateHelper>(
                oisConv->spotLag(), tenor, parRateQuote, oisConv->index(), oisConv->fixedDayCounter(),
                oisConv->fixedCalendar(), oisConv->paymentLag(), oisConv->eom(), oisConv->fixedFrequency(),
                oisConv->fixedConvention(), oisConv->fixedPaymentConvention(), oisConv->rule(), exDiscount, true);
        } else if (parInst[i] == "FXF") {
            QuantLib::ext::shared_ptr<Convention> conv = conventions->get(ccy + "-FX-CONVENTIONS");
            QuantLib::ext::shared_ptr<FXConvention> fxConv = QuantLib::ext::dynamic_pointer_cast<FXConvention>(conv);
            BOOST_ASSERT(fxConv);
            QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*parRateQuote)
                ->setValue(0.0); // set the fwd and basis points to zero for these tests
            bool isFxBaseCurrencyCollateralCurrency = false;
            // the fx swap rate helper interprets the fxSpot as of the spot date, our fx spot here
            // is as of today, therefore we set up the fx spot helper with zero settlement days
            // and compute the tenor such that the correct maturity date is still matched
            Date today = Settings::instance().evaluationDate();
            Date spotDate = fxConv->advanceCalendar().advance(today, fxConv->spotDays() * Days);
            Date endDate = fxConv->advanceCalendar().advance(spotDate, tenor);
            rateHelper = QuantLib::ext::make_shared<FxSwapRateHelper>(parRateQuote, fxSpot, (endDate - today) * Days, 0,
                                                              NullCalendar(), Unadjusted, false,
                                                              isFxBaseCurrencyCollateralCurrency, fgnDiscount);
        } else if (parInst[i] == "XBS") {
            QuantLib::ext::shared_ptr<Convention> conv = conventions->get(ccy + "-XCCY-BASIS-CONVENTIONS");
            QuantLib::ext::shared_ptr<CrossCcyBasisSwapConvention> basisConv =
                QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(conv);
            BOOST_ASSERT(market);
            QuantLib::ext::shared_ptr<IborIndex> flatIndex =
                *market->iborIndex(basisConv->flatIndexName(), Market::defaultConfiguration);
            QuantLib::ext::shared_ptr<IborIndex> spreadIndex =
                *market->iborIndex(basisConv->spreadIndexName(), Market::defaultConfiguration);
            BOOST_ASSERT(basisConv);
            BOOST_ASSERT(flatIndex);
            BOOST_ASSERT(*flatIndex->forwardingTermStructure());
            BOOST_ASSERT(spreadIndex);
            BOOST_ASSERT(*spreadIndex->forwardingTermStructure());
            BOOST_ASSERT(*fgnDiscount);
            QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*parRateQuote)
                ->setValue(0.0);        // set the fwd and basis points to zero for these tests
            bool flatIsDomestic = true; // assumes fxSpot is in form 1*BaseCcy = X*Ccy
            rateHelper = QuantLib::ext::make_shared<CrossCcyBasisSwapHelper>(
                parRateQuote, fxSpot, basisConv->settlementDays(), basisConv->settlementCalendar(), tenor,
                basisConv->rollConvention(), flatIndex, spreadIndex, fgnDiscount, exDiscount, basisConv->eom(),
                flatIsDomestic);
        } else {
            BOOST_ERROR("Unrecognised par rate instrument in curve construction - " << i);
        }
        instruments.push_back(rateHelper);
    }
    return instruments;
}

vector<QuantLib::ext::shared_ptr<DefaultProbabilityHelper>>
parRateCurveHelpers(const string& name, const vector<Period>& parTenor, const vector<Handle<Quote>>& parVal,
                    const Handle<YieldTermStructure> exDiscount = Handle<YieldTermStructure>(),
                    // pointer to the market that is being built
                    ore::data::Market* market = NULL) {
    BOOST_ASSERT(parTenor.size() == parVal.size());
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    vector<QuantLib::ext::shared_ptr<DefaultProbabilityHelper>> instruments;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < parTenor.size(); ++i) {
        Handle<Quote> parRateQuote = parVal[i];
        Period tenor = parTenor[i];
        QuantLib::ext::shared_ptr<QuantExt::CdsHelper> rateHelper;
        QuantLib::ext::shared_ptr<Convention> conv = conventions->get("CDS-STANDARD-CONVENTIONS");
        QuantLib::ext::shared_ptr<CdsConvention> cdsConv = QuantLib::ext::dynamic_pointer_cast<CdsConvention>(conv);
        BOOST_ASSERT(cdsConv);
        BOOST_ASSERT(market);
        Real recoveryRate = market->recoveryRate(name, Market::defaultConfiguration)->value();

        BOOST_ASSERT(*exDiscount);
        rateHelper = QuantLib::ext::make_shared<QuantExt::SpreadCdsHelper>(
            parRateQuote, tenor, cdsConv->settlementDays(), cdsConv->calendar(), cdsConv->frequency(),
            cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(), recoveryRate, exDiscount,
            true, QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault, today + cdsConv->settlementDays());
        instruments.push_back(rateHelper);
    }
    return instruments;
}

Handle<YieldTermStructure> parRateCurve(const Date& asof, const vector<QuantLib::ext::shared_ptr<RateHelper>>& rateHelpers) {
    Handle<YieldTermStructure> yieldTs(QuantLib::ext::make_shared<PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>>(
        asof, rateHelpers, ActualActual(ActualActual::ISDA),
        PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>::bootstrap_type(1.0e-12)));
    yieldTs->enableExtrapolation();
    return yieldTs;
}

Handle<DefaultProbabilityTermStructure>
parRateCurve(const Date& asof, const vector<QuantLib::ext::shared_ptr<QuantExt::DefaultProbabilityHelper>>& rateHelpers) {
    Handle<DefaultProbabilityTermStructure> dps(
        QuantLib::ext::make_shared<QuantLib::PiecewiseDefaultCurve<QuantLib::SurvivalProbability, QuantLib::Linear>>(
            asof, rateHelpers, Actual360()));
    dps->enableExtrapolation();
    return dps;
}

} // namespace


TestMarket::TestMarket(Date asof, bool swapVolCube) : MarketImpl(false) {

    TestConfigurationObjects::setConventions();
    
    asof_ = asof;

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
        {"EUR-EONIA", 0.01},    {"EUR-EURIBOR-3M", 0.015}, {"EUR-EURIBOR-6M", 0.02},
        {"USD-FedFunds", 0.01}, {"USD-LIBOR-1M", 0.02},    {"USD-LIBOR-3M", 0.03},    {"USD-LIBOR-6M", 0.05},
        {"GBP-SONIA", 0.01},    {"GBP-LIBOR-3M", 0.03},    {"GBP-LIBOR-6M", 0.04},
        {"CHF-LIBOR-3M", 0.01}, {"CHF-TOIS", 0.02},        {"CHF-LIBOR-6M", 0.02},
        {"JPY-LIBOR-6M", 0.01}, {"JPY-TONAR", 0.01},       {"JPY-LIBOR-3M", 0.01},
        {"CAD-CDOR-3M", 0.02},  {"CAD-CORRA", 0.01},
        {"SEK-STIBOR-3M", 0.02}};

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
    defaultCurves_[make_pair(Market::defaultConfiguration, "BondIssuer0")] = flatRateDcs(0.0);
    defaultCurves_[make_pair(Market::defaultConfiguration, "BondIssuer1")] = flatRateDcs(0.0);

    recoveryRates_[make_pair(Market::defaultConfiguration, "dc")] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "dc2")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "BondIssuer0")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)); 
    recoveryRates_[make_pair(Market::defaultConfiguration, "BondIssuer1")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));

    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BondCurve0")] = flatRateYts(0.05);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BondCurve1")] = flatRateYts(0.05);

    securitySpreads_[make_pair(Market::defaultConfiguration, "Bond0")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    securitySpreads_[make_pair(Market::defaultConfiguration, "Bond1")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));

    cdsVols_[make_pair(Market::defaultConfiguration, "dc")] =
        Handle<QuantExt::CreditVolCurve>(QuantLib::ext::make_shared<QuantExt::CreditVolCurveWrapper>(flatRateFxv(0.12)));
    
    Handle<IborIndex> hGBP(ore::data::parseIborIndex(
        "GBP-LIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "GBP")]));
    // FIXME: We have defined that above already
    iborIndices_[make_pair(Market::defaultConfiguration, "GBP-LIBOR-6M")] = hGBP;

    // Some test cases need a different definition of UKRPI index, curve and vol structure
    // We there fore added the new UKROi as UKRP1 and keep the "original" below.
    
    // build inflation indices
    auto zeroIndex = Handle<ZeroInflationIndex>(QuantLib::ext::make_shared<UKRPI>(true, flatZeroInflationCurve(0.02, 0.01)));
    zeroInflationIndices_[make_pair(Market::defaultConfiguration, "UKRP1")] = zeroIndex;
    yoyInflationIndices_[make_pair(Market::defaultConfiguration, "UKRP1")] = Handle<YoYInflationIndex>(
        QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(*zeroIndex, false, flatYoYInflationCurve(0.02, 0.01)));

    // build inflation cap / floor vol curves
    yoyCapFloorVolSurfaces_[make_pair(Market::defaultConfiguration, "UKRP1")] =
        flatYoYOptionletVolatilitySurface(0.0040);

    // build UKRPI fixing history
    Date cpiFixingEnd(1, asof_.month(), asof_.year());
    Date cpiFixingStart = cpiFixingEnd - Period(14, Months);
    Schedule fixingDatesUKRPI = MakeSchedule().from(cpiFixingStart).to(cpiFixingEnd).withTenor(1 * Months);
    Real fixingRatesUKRPI[] = {258.5, 258.9, 258.6, 259.8, 259.6, 259.5, 259.8, 260.6,
                               258.8, 260.0, 261.1, 261.4, 262.1, 264.3, 265.2};

    // build UKRPI index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> ii = parseZeroInflationIndex("UKRPI");
    QuantLib::ext::shared_ptr<YoYInflationIndex> yi = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(ii, false);

    RelinkableHandle<ZeroInflationTermStructure> hcpi;
    ii = QuantLib::ext::shared_ptr<UKRPI>(new UKRPI(hcpi));
    for (Size i = 0; i < fixingDatesUKRPI.size(); i++) {
        // std::cout << i << ", " << fixingDatesUKRPI[i] << ", " << fixingRatesUKRPI[i] << std::endl;
        ii->addFixing(fixingDatesUKRPI[i], fixingRatesUKRPI[i], true);
    };

    // build EUHICPXT fixing history
    Schedule fixingDatesEUHICPXT = MakeSchedule().from(cpiFixingStart).to(cpiFixingEnd).withTenor(1 * Months);
    Real fixingRatesEUHICPXT[] = {258.5, 258.9, 258.6, 259.8, 259.6, 259.5, 259.8, 260.6,
                                  258.8, 260.0, 261.1, 261.4, 262.1, 264.3, 265.2};

    // build EUHICPXT index
    QuantLib::ext::shared_ptr<ZeroInflationIndex> euii = parseZeroInflationIndex("EUHICPXT");
    QuantLib::ext::shared_ptr<YoYInflationIndex> euyi = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(euii, false);

    RelinkableHandle<ZeroInflationTermStructure> euhcpi;
    euii = QuantLib::ext::shared_ptr<EUHICPXT>(new EUHICPXT(euhcpi));
    for (Size i = 0; i < fixingDatesEUHICPXT.size(); i++) {
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

    // Commodity volatilities
    commodityVols_[make_pair(Market::defaultConfiguration, "COMDTY_GOLD_USD")] = flatRateFxv(0.15);
    commodityVols_[make_pair(Market::defaultConfiguration, "COMDTY_WTI_USD")] = flatRateFxv(0.20);

    // Correlations
    correlationCurves_[make_tuple(Market::defaultConfiguration, "EUR-CMS-10Y", "EUR-CMS-1Y")] = flatCorrelation(0.15);
    correlationCurves_[make_tuple(Market::defaultConfiguration, "USD-CMS-10Y", "USD-CMS-1Y")] = flatCorrelation(0.2);
}

Handle<YieldTermStructure> TestMarket::flatRateYts(Real forward) {
    QuantLib::ext::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), forward, ActualActual(ActualActual::ISDA)));
    return Handle<YieldTermStructure>(yts);
}

Handle<BlackVolTermStructure> TestMarket::flatRateFxv(Volatility forward) {
    QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(
            new BlackConstantVol(Settings::instance().evaluationDate(), NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
    return Handle<BlackVolTermStructure>(fxv);
}

Handle<YieldTermStructure> TestMarket::flatRateDiv(Real dividend) {
    QuantLib::ext::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), dividend, ActualActual(ActualActual::ISDA)));
    return Handle<YieldTermStructure>(yts);
}

Handle<QuantLib::SwaptionVolatilityStructure>
TestMarket::flatRateSvs(Volatility forward, VolatilityType type, Real shift) {
    QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                     ModifiedFollowing, forward, ActualActual(ActualActual::ISDA), type, shift));
    return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
}

Handle<QuantExt::CreditCurve> TestMarket::flatRateDcs(Volatility forward) {
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> dcs(
            new FlatHazardRate(asof_, forward, ActualActual(ActualActual::ISDA)));
    return Handle<QuantExt::CreditCurve>(QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(dcs)));
}

Handle<OptionletVolatilityStructure> TestMarket::flatRateCvs(Volatility vol, VolatilityType type, Real shift) {
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ts(
            new QuantLib::ConstantOptionletVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                      ModifiedFollowing, vol, ActualActual(ActualActual::ISDA), type, shift));
    return Handle<OptionletVolatilityStructure>(ts);
}

Handle<QuantExt::CorrelationTermStructure> TestMarket::flatCorrelation(Real correlation) {
    QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure> ts(
            new QuantExt::FlatCorrelation(Settings::instance().evaluationDate(), correlation, ActualActual(ActualActual::ISDA)));
    return Handle<QuantExt::CorrelationTermStructure>(ts);
}

Handle<CPICapFloorTermPriceSurface> TestMarket::flatRateCps(Handle<ZeroInflationIndex> infIndex,
                                                            const std::vector<Rate> cStrikes, std::vector<Rate> fStrikes,
                                                            std::vector<Period> cfMaturities, Matrix cPrice, Matrix fPrice) {
    QuantLib::ext::shared_ptr<CPICapFloorTermPriceSurface> ts(
            new InterpolatedCPICapFloorTermPriceSurface<QuantLib::Bilinear>(
                1.0, 0.0, infIndex->availabilityLag(), infIndex->zeroInflationTermStructure()->calendar(), Following,
                ActualActual(ActualActual::ISDA), infIndex.currentLink(), CPI::AsIndex, discountCurve(infIndex->currency().code()), cStrikes, fStrikes, cfMaturities,
                cPrice, fPrice));
    return Handle<CPICapFloorTermPriceSurface>(ts);
}
    
Handle<QuantLib::CPIVolatilitySurface> TestMarket::flatCpiVolSurface(Volatility v) {
    Natural settleDays = 0;
    Calendar cal = TARGET();
    BusinessDayConvention bdc = Following;
    DayCounter dc = Actual365Fixed();
    Period lag = 2 * Months;
    Frequency freq = Annual;
    bool interp = false;
    QuantLib::ext::shared_ptr<QuantLib::ConstantCPIVolatility> cpiCapFloorVolSurface =
        QuantLib::ext::make_shared<QuantLib::ConstantCPIVolatility>(v, settleDays, cal, bdc, dc, lag, freq, interp);
    return Handle<QuantLib::CPIVolatilitySurface>(cpiCapFloorVolSurface);
}

Handle<ZeroInflationIndex> TestMarket::makeZeroInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                              QuantLib::ext::shared_ptr<ZeroInflationIndex> ii,
                                                              Handle<YieldTermStructure> yts) {
    // build UKRPI index
    QuantLib::ext::shared_ptr<ZeroInflationTermStructure> cpiTS;
    // now build the helpers ...
    vector<QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure>>> instruments;
    for (Size i = 0; i < dates.size(); i++) {
        Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(rates[i] / 100.0)));
        QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure>> anInstrument(new ZeroCouponInflationSwapHelper(
            quote, Period(2, Months), dates[i], TARGET(), ModifiedFollowing, ActualActual(ActualActual::ISDA), ii, CPI::AsIndex, yts));
        anInstrument->unregisterWith(Settings::instance().evaluationDate());
        instruments.push_back(anInstrument);
    };
    // we can use historical or first ZCIIS for this
    // we know historical is WAY off market-implied, so use market implied flat.
    Rate baseZeroRate = rates[0] / 100.0;
    QuantLib::ext::shared_ptr<PiecewiseZeroInflationCurve<Linear>> pCPIts(
        new PiecewiseZeroInflationCurve<Linear>(asof_, TARGET(), ActualActual(ActualActual::ISDA), Period(2, Months), ii->frequency(),
                                                baseZeroRate, instruments));
    pCPIts->recalculate();
    cpiTS = QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(pCPIts);
    cpiTS->enableExtrapolation(true);
    cpiTS->unregisterWith(Settings::instance().evaluationDate());
    return Handle<ZeroInflationIndex>(parseZeroInflationIndex(index, Handle<ZeroInflationTermStructure>(cpiTS)));
}

Handle<YoYInflationIndex> TestMarket::makeYoYInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                            QuantLib::ext::shared_ptr<YoYInflationIndex> ii,
                                                            Handle<YieldTermStructure> yts) {
    // build UKRPI index
    QuantLib::ext::shared_ptr<YoYInflationTermStructure> yoyTS;
    // now build the helpers ...
    vector<QuantLib::ext::shared_ptr<BootstrapHelper<YoYInflationTermStructure>>> instruments;
    for (Size i = 0; i < dates.size(); i++) {
        Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(rates[i] / 100.0)));
        QuantLib::ext::shared_ptr<BootstrapHelper<YoYInflationTermStructure>> anInstrument(new YearOnYearInflationSwapHelper(
            quote, Period(2, Months), dates[i], TARGET(), ModifiedFollowing, ActualActual(ActualActual::ISDA), ii, yts));
        instruments.push_back(anInstrument);
    };
    // we can use historical or first ZCIIS for this
    // we know historical is WAY off market-implied, so use market implied flat.
    Rate baseZeroRate = rates[0] / 100.0;
    QuantLib::ext::shared_ptr<PiecewiseYoYInflationCurve<Linear>> pYoYts(
        new PiecewiseYoYInflationCurve<Linear>(asof_, TARGET(), ActualActual(ActualActual::ISDA), Period(2, Months), ii->frequency(),
                                               ii->interpolated(), baseZeroRate, instruments));
    pYoYts->recalculate();
    yoyTS = QuantLib::ext::dynamic_pointer_cast<YoYInflationTermStructure>(pYoYts);
    return Handle<YoYInflationIndex>(QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(
        parseZeroInflationIndex(index), false, Handle<YoYInflationTermStructure>(pYoYts)));
}

Handle<ZeroInflationTermStructure> TestMarket::flatZeroInflationCurve(Real inflationRate, Rate nominalRate) {
    Date today = Settings::instance().evaluationDate();
    Period lag = 2 * Months;
    std::vector<Date> dates;
    dates.push_back(today - lag);
    dates.push_back(today + 1 * Years);
    std::vector<Real> rates(dates.size(), inflationRate);
    auto curve = QuantLib::ext::make_shared<QuantLib::InterpolatedZeroInflationCurve<Linear>>(
        today, NullCalendar(), QuantLib::ActualActual(ActualActual::ISDA), 2 * Months, Monthly, dates, rates);
    curve->enableExtrapolation();
    return Handle<ZeroInflationTermStructure>(curve);
}

Handle<YoYInflationTermStructure> TestMarket::flatYoYInflationCurve(Real inflationRate, Rate nominalRate) {
    Date today = Settings::instance().evaluationDate();
    Period lag = 2 * Months;
    std::vector<Date> dates;
    dates.push_back(today - lag);
    dates.push_back(today + 1 * Years);
    std::vector<Real> rates(dates.size(), inflationRate);
    auto curve = QuantLib::ext::make_shared<QuantLib::InterpolatedYoYInflationCurve<Linear>>(
        today, NullCalendar(), QuantLib::ActualActual(ActualActual::ISDA), 2 * Months, Monthly, false, dates, rates);
    curve->enableExtrapolation();
    return Handle<YoYInflationTermStructure>(curve);
}

Handle<YoYOptionletVolatilitySurface> TestMarket::flatYoYOptionletVolatilitySurface(Real normalVol) {
    auto qlTs = QuantLib::ext::make_shared<ConstantYoYOptionletVolatility>(
        normalVol, 0, NullCalendar(), Unadjusted, ActualActual(ActualActual::ISDA), 2 * Months, Monthly, false, -1.0, 100.0, Normal);
    return Handle<YoYOptionletVolatilitySurface>(qlTs);
}

TestMarketParCurves::TestMarketParCurves(const Date& asof) : MarketImpl(false) {
    asof_ = asof;

    TestConfigurationObjects::setConventions();

    vector<pair<string, Real>> ccys = {{"EUR", 0.02}, {"USD", 0.03}, {"GBP", 0.04}, {"CHF", 0.02}};
    vector<pair<string, Real>> o_ccys = {{"JPY", 0.005}};
    vector<pair<string, Real>> x_ccys = {{"CHF", 0.02}};
    vector<pair<string, Real>> d_names = {{"dc", 0.001}, {"dc2", 0.001}, {"dc3", 0.001}};
    map<string, string> d_ccys;
    d_ccys["dc"] = "USD";
    d_ccys["dc2"] = "EUR";
    d_ccys["dc3"] = "GBP";
    vector<Period> parTenor = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years, 3 * Years,
                               5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};
    vector<Period> parTenor2 = {3 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,
                                5 * Years,  10 * Years, 13 * Years, 15 * Years, 20 * Years};
    vector<Period> parTenor3 = {6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years};

    for (auto it : ccys) {
        string ccy = it.first;
        Real parRate = it.second;
        vector<string> parInst;
        if (ccy == "JPY")
            parInst = {"OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS"};
        else
            parInst = {"DEP", "DEP", "DEP", "DEP", "FRA", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};
        vector<Real> parRates(parInst.size(), parRate);
        createDiscountCurve(ccy, parInst, parTenor, parRates);
    }
    for (auto it : o_ccys) {
        string ccy = it.first;
        Real parRate = it.second;
        vector<string> parInst;
        parInst = {"OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS"};
        vector<Real> parRates(parInst.size(), parRate);
        createDiscountCurve(ccy, parInst, parTenor, parRates);
    }
        
    // add fx rates
    // add fx rates
    std::map<std::string, QuantLib::Handle<QuantLib::Quote>> quotes;
    quotes["EURUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
    quotes["EURGBP"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.8));
    quotes["EURCHF"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    quotes["EURJPY"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(128.0));
    fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

    recoveryRates_[make_pair(Market::defaultConfiguration, "dc")] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "dc2")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));
    recoveryRates_[make_pair(Market::defaultConfiguration, "dc3")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.4));

    for (auto it : d_names) {
        string name = it.first;
        Real parRate = it.second;
        string ccy = d_ccys[name];
        vector<string> parInst;
        parInst = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
        vector<Real> parRates(parInst.size(), parRate);

        createDefaultCurve(name, ccy, parInst, parTenor2, parRates);
    }
    // build ibor indices
    vector<pair<string, Real>> indexData = {
        {"EUR-EONIA", 0.02},      {"EUR-EURIBOR-2W", 0.02}, {"EUR-EURIBOR-1M", 0.02}, {"EUR-EURIBOR-3M", 0.02},
        {"EUR-EURIBOR-6M", 0.02}, {"USD-FedFunds", 0.03},   {"USD-LIBOR-2W", 0.03},   {"USD-LIBOR-1M", 0.03},
        {"USD-LIBOR-3M", 0.03},   {"USD-LIBOR-6M", 0.03},   {"GBP-SONIA", 0.04},      {"GBP-LIBOR-2W", 0.04},
        {"GBP-LIBOR-1M", 0.04},   {"GBP-LIBOR-3M", 0.04},   {"GBP-LIBOR-6M", 0.04},   {"JPY-TONAR", 0.005},
        {"JPY-LIBOR-2W", 0.005},  {"JPY-LIBOR-1M", 0.005},  {"JPY-LIBOR-3M", 0.005},  {"JPY-LIBOR-6M", 0.005}};
    vector<pair<string, Real>> singleCurve_indexData = {{"CHF-LIBOR-3M", 0.02}, {"CHF-LIBOR-6M", 0.02}};
    for (auto id : indexData) {
        string idxName = id.first;
        string ccy = idxName.substr(0, 3);
        vector<string> parInst = {"DEP", "DEP", "DEP", "DEP", "FRA", "IRS", "IRS",
                                  "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};
        vector<Real> parRates(parInst.size(), id.second);
        createIborIndex(idxName, parInst, parTenor, parRates, false);
    }
    for (auto id : singleCurve_indexData) {
        string idxName = id.first;
        string ccy = idxName.substr(0, 3);
        vector<string> parInst = {"DEP", "DEP", "DEP", "DEP", "FRA", "IRS", "IRS",
                                  "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};
        vector<Real> parRates(parInst.size(), id.second);
        createIborIndex(idxName, parInst, parTenor, parRates, true);
    }
    // now build the remaining discount curves that have cross-currency dependencies
    for (auto it : x_ccys) {
        string baseCcy = "EUR";
        string ccy = it.first;
        Real parRate = it.second;
        vector<string> parInst;
        parInst = {"FXF", "FXF", "FXF", "FXF", "FXF", "XBS", "XBS", "XBS", "XBS", "XBS", "XBS", "XBS", "XBS"};
        vector<Real> parRates(parInst.size(), parRate);
        BOOST_ASSERT(ccy != baseCcy);
        createXccyDiscountCurve(ccy, baseCcy, parInst, parTenor, parRates);
    }

    // swap index
    addSwapIndex("EUR-CMS-2Y", "EUR-EURIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("EUR-CMS-30Y", "EUR-EURIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("USD-CMS-2Y", "USD-FedFunds", Market::defaultConfiguration);
    addSwapIndex("USD-CMS-30Y", "USD-FedFunds", Market::defaultConfiguration);
    addSwapIndex("GBP-CMS-2Y", "GBP-SONIA", Market::defaultConfiguration);
    addSwapIndex("GBP-CMS-30Y", "GBP-SONIA", Market::defaultConfiguration);
    addSwapIndex("CHF-CMS-2Y", "CHF-LIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("CHF-CMS-30Y", "CHF-LIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("JPY-CMS-2Y", "JPY-LIBOR-6M", Market::defaultConfiguration);
    addSwapIndex("JPY-CMS-30Y", "JPY-LIBOR-6M", Market::defaultConfiguration);

    // build fx vols
    fxVols_[make_pair(Market::defaultConfiguration, "EURUSD")] = flatRateFxv(0.12);
    fxVols_[make_pair(Market::defaultConfiguration, "EURGBP")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "EURCHF")] = flatRateFxv(0.15);
    fxVols_[make_pair(Market::defaultConfiguration, "EURJPY")] = flatRateFxv(0.15);

    // build cap/floor vol structures
    capFloorCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateCvs(0.0050, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateCvs(0.0060, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "GBP")] = flatRateCvs(0.0055, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "CHF")] = flatRateCvs(0.0045, Normal);
    capFloorCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateCvs(0.0040, Normal);

    // build swaption vols
    vector<pair<string, Real>> swapvolrates = {
        {"EUR", 0.2}, {"USD", 0.30}, {"GBP", 0.25}, {"CHF", 0.25}, {"JPY", 0.25}};
    vector<Period> swapTenors = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years, 3 * Years,
                                 5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};
    vector<Period> swapTerms = {1 * Years, 2 * Years,  3 * Years,  4 * Years,  5 * Years,
                                7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years};

    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "EUR")] = std::make_pair("EUR-CMS-2Y", "EUR-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "USD")] = std::make_pair("USD-CMS-2Y", "USD-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "GBP")] = std::make_pair("GBP-CMS-2Y", "GBP-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "CHF")] = std::make_pair("CHF-CMS-2Y", "CHF-CMS-30Y");
    swaptionIndexBases_[make_pair(Market::defaultConfiguration, "JPY")] = std::make_pair("JPY-CMS-2Y", "JPY-CMS-30Y");

    vector<Real> swapStrikes = {-0.02, -0.005, 0, 0.005, 0.02};
    for (auto it : swapvolrates) {
        string name = it.first;
        Real parRate = it.second;
        vector<Real> parRates(swapTenors.size() * swapTerms.size() * swapStrikes.size(), parRate);
        createSwaptionVolCurve(name, swapTenors, swapTerms, swapStrikes, parRates);
    }

    // Add Equity Spots
    equitySpots_[make_pair(Market::defaultConfiguration, "SP5")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(2147.56));
    equitySpots_[make_pair(Market::defaultConfiguration, "Lufthansa")] =
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(12.75));

    vector<pair<string, Real>> eqvolrates = {{"SP5", 0.2514}, {"Lufthansa", 0.30}};
    map<string, string> currencyMap;
    currencyMap["SP5"] = "USD";
    currencyMap["Lufthansa"] = "EUR";
    for (auto it : eqvolrates) {
        string name = it.first;
        Real parRate = it.second;
        vector<Real> parRates(parTenor.size(), parRate);
        createEquityVolCurve(name, currencyMap[name], parTenor, parRates);
    }

    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "SP5")] = flatRateYts(0.01);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::EquityDividend, "Lufthansa")] =
        flatRateYts(0.0);

    vector<string> parInst = {"DEP", "DEP", "DEP", "DEP", "FRA", "IRS", "IRS",
                              "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};
    vector<Real> parRates1(parInst.size(), 0.03);
    vector<Real> parRates2(parInst.size(), 0.02);

    equityCurves_[make_pair(Market::defaultConfiguration, "SP5")] = Handle<EquityIndex2>(QuantLib::ext::make_shared<EquityIndex2>(
        "SP5", UnitedStates(UnitedStates::Settlement), parseCurrency("USD"), equitySpot("SP5"), yieldCurve(YieldCurveType::Discount, "USD"),
        yieldCurve(YieldCurveType::EquityDividend, "SP5")));
    equityCurves_[make_pair(Market::defaultConfiguration, "Lufthansa")] =
        Handle<EquityIndex2>(QuantLib::ext::make_shared<EquityIndex2>(
            "Lufthansa", TARGET(), parseCurrency("EUR"), equitySpot("Lufthansa"),
            yieldCurve(YieldCurveType::Discount, "EUR"), yieldCurve(YieldCurveType::EquityDividend, "Lufthansa")));

    vector<pair<string, Real>> cdsrates = {{"dc", 0.12}, {"dc2", 0.1313}, {"dc3", 0.14}};
    for (auto it : cdsrates) {
        string name = it.first;
        Real parRate = it.second;
        vector<Real> parRates(parTenor3.size(), parRate);
        createCdsVolCurve(name, parTenor3, parRates);
    }

    vector<pair<string, vector<Real>>> bcrates = {{"Tranch1", {0.1, 0.2, 0.3, 0.4, 0.5, 0.6}}};
    vector<Period> bctenors = {1 * Days, 2 * Days};
    vector<string> lossLevel = {"0.03", "0.06", "0.09", "0.12", "0.22", "1.00"};
    for (auto it : bcrates) {
        string name = it.first;
        vector<Real> corrRates = it.second;
        createBaseCorrel(name, bctenors, lossLevel, corrRates);
    }

    Date cpiFixingEnd(1, asof_.month(), asof_.year());
    Date cpiFixingStart = cpiFixingEnd - Period(14, Months);
    Schedule fixingDatesUKRPI = MakeSchedule().from(cpiFixingStart).to(cpiFixingEnd).withTenor(1 * Months);
    Real fixingRatesUKRPI[] = {258.5, 258.9, 258.6, 259.8, 259.6, 259.5,  259.8, 260.6,
                               258.8, 260.0, 261.1, 261.4, 262.1, -264.3, -265.2};

    RelinkableHandle<ZeroInflationTermStructure> hcpi;
    QuantLib::ext::shared_ptr<ZeroInflationIndex> ii = QuantLib::ext::shared_ptr<UKRPI>(new UKRPI(hcpi));
    for (Size i = 0; i < fixingDatesUKRPI.size(); i++) {
        ii->addFixing(fixingDatesUKRPI[i], fixingRatesUKRPI[i], true);
    };

    vector<pair<string, vector<Real>>> zirates = {
        {"UKRPI", {2.825, 2.9425, 2.975, 2.983, 3.0, 3.01, 3.008, 3.009, 3.013}}};
    vector<Period> zitenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                               7 * Years,  10 * Years, 15 * Years, 20 * Years};
    for (auto it : zirates) {
        string index = it.first;
        vector<Real> parRates = it.second;
        vector<string> parInst = {"ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS"};
        createZeroInflationIndex(index, parInst, zitenors, parRates, true);
    }
    vector<pair<string, vector<Real>>> yyrates = {
        {"UKRPI", {/*2.825,*/ 2.9425, 2.975, 2.983, 3.0, 3.01, 3.008, 3.009, 3.013}}};
    vector<Period> yytenors = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                               7 * Years, 10 * Years, 15 * Years, 20 * Years};
    for (auto it : yyrates) {
        string index = it.first;
        vector<Real> parRates = it.second;
        vector<string> parInst = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
        createYoYInflationIndex(index, parInst, yytenors, parRates, true);
    }
}

void TestMarketParCurves::createDiscountCurve(const string& ccy, const vector<string>& parInst,
                                              const vector<Period>& parTenor, const vector<Real>& parRates) {
    discountRateHelperInstMap_[ccy] = parInst;
    discountRateHelperTenorsMap_[ccy] = parTenor;
    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i]));
    discountRateHelperValuesMap_[ccy] = parQuotes;
    discountRateHelpersMap_[ccy] = parRateCurveHelpers(ccy, parInst, parTenor, discountRateHelperValuesMap_[ccy]);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, ccy)] =
        parRateCurve(asof_, discountRateHelpersMap_[ccy]);
}

void TestMarketParCurves::createXccyDiscountCurve(const string& ccy, const string& baseCcy,
                                                  const vector<string>& parInst, const vector<Period>& parTenor,
                                                  const vector<Real>& parRates) {
    discountRateHelperInstMap_[ccy] = parInst;
    discountRateHelperTenorsMap_[ccy] = parTenor;
    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i]));
    discountRateHelperValuesMap_[ccy] = parQuotes;
    Handle<Quote> fxSpot = this->fxSpot(ccy + baseCcy, Market::defaultConfiguration);
    Handle<YieldTermStructure> baseDiscount = this->discountCurve(baseCcy, Market::defaultConfiguration);
    Handle<YieldTermStructure> ccyDiscountHandle =
        Handle<YieldTermStructure>(); // leave unlinked, as this is the curve we are building
    discountRateHelpersMap_[ccy] = parRateCurveHelpers(ccy, parInst, parTenor, discountRateHelperValuesMap_[ccy],
                                                       ccyDiscountHandle, fxSpot, baseDiscount, this);
    yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, ccy)] =
        parRateCurve(asof_, discountRateHelpersMap_[ccy]);
}

void TestMarketParCurves::createIborIndex(const string& idxName, const vector<string>& parInst,
                                          const vector<Period>& parTenor, const vector<Real>& parRates,
                                          bool singleCurve) {
    string ccy = idxName.substr(0, 3);
    indexCurveRateHelperInstMap_[idxName] = parInst;
    indexCurveRateHelperTenorsMap_[idxName] = parTenor;
    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i]));
    indexCurveRateHelperValuesMap_[idxName] = parQuotes;
    indexCurveRateHelpersMap_[idxName] = parRateCurveHelpers(
        ccy, parInst, parTenor, indexCurveRateHelperValuesMap_[idxName],
        singleCurve ? Handle<YieldTermStructure>()
                    : yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, ccy)]);
    Handle<IborIndex> h(parseIborIndex(idxName, parRateCurve(asof_, indexCurveRateHelpersMap_[idxName])));
    iborIndices_[make_pair(Market::defaultConfiguration, idxName)] = h;

    // set up dummy fixings for the past 400 days
    for (Date d = asof_ - 400; d < asof_; d++) {
        if (h->isValidFixingDate(d))
            h->addFixing(d, 0.01);
    }
}

void TestMarketParCurves::createDefaultCurve(const string& name, const string& ccy, const vector<string>& parInst,
                                             const vector<Period>& parTenor, const vector<Real>& parRates) {
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    defaultRateHelperInstMap_[name] = parInst;
    defaultRateHelperTenorsMap_[name] = parTenor;
    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i]));
    defaultRateHelperValuesMap_[name] = parQuotes;
    QuantLib::ext::shared_ptr<Convention> conv = conventions->get("CDS-STANDARD-CONVENTIONS");
    Handle<YieldTermStructure> baseDiscount = this->discountCurve(ccy, Market::defaultConfiguration);
    defaultRateHelpersMap_[name] =
        parRateCurveHelpers(name, parTenor, defaultRateHelperValuesMap_[name], baseDiscount, this);

    defaultCurves_[make_pair(Market::defaultConfiguration, name)] = Handle<QuantExt::CreditCurve>(
        QuantLib::ext::make_shared<QuantExt::CreditCurve>(parRateCurve(asof_, defaultRateHelpersMap_[name])));
}

void TestMarketParCurves::createCdsVolCurve(const string& name, const vector<Period>& parTenor,
                                            const vector<Real>& parRates) {
    cdsVolRateHelperTenorsMap_[name] = parTenor;
    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i]));
    cdsVolRateHelperValuesMap_[name] = parQuotes;

    DayCounter dc = Actual365Fixed();
    Calendar cal = TARGET();
    BusinessDayConvention bdc = Following;
    vector<Volatility> atmVols(parQuotes.size());
    vector<Date> dates(parQuotes.size());
    vector<Time> times(parQuotes.size());

    for (Size i = 0; i < parQuotes.size(); i++) {
        dates[i] = asof_ + parTenor[i];
        atmVols[i] = parQuotes[i]->value();
        times[i] = dc.yearFraction(asof_, dates[i]);
    }

    // QuantLib::ext::shared_ptr<BlackVolTermStructure> vol(new BlackVarianceCurve(asof_, dates, atmVols, dc));
    QuantLib::ext::shared_ptr<BlackVolTermStructure> vol(new BlackVarianceCurve3(0, cal, bdc, dc, times, parQuotes));
    vol->enableExtrapolation();
    cdsVols_[make_pair(Market::defaultConfiguration, name)] = Handle<QuantExt::CreditVolCurve>(
        QuantLib::ext::make_shared<QuantExt::CreditVolCurveWrapper>(Handle<BlackVolTermStructure>(vol)));
}

void TestMarketParCurves::createEquityVolCurve(const string& name, const string& ccy, const vector<Period>& parTenor,
                                               const vector<Real>& parRates) {
    equityVolRateHelperTenorsMap_[name] = parTenor;
    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i]));
    equityVolRateHelperValuesMap_[name] = parQuotes;

    DayCounter dc = Actual365Fixed();
    // use calendat based on ccy, to align with sim market
    Calendar cal = parseCalendar(ccy);
    BusinessDayConvention bdc = Following;
    vector<Date> dates(parQuotes.size());
    vector<Time> times(parQuotes.size());

    for (Size i = 0; i < parQuotes.size(); i++) {
        dates[i] = cal.advance(asof_, parTenor[i]);
        times[i] = dc.yearFraction(asof_, dates[i]);
    }
    QuantLib::ext::shared_ptr<BlackVolTermStructure> vol(new BlackVarianceCurve3(0, cal, bdc, dc, times, parQuotes));
    vol->enableExtrapolation();
    equityVols_[make_pair(Market::defaultConfiguration, name)] = Handle<BlackVolTermStructure>(vol);
}

void TestMarketParCurves::createBaseCorrel(const string& name, const vector<Period>& tenors,
                                           const vector<string>& lossLevel, const vector<Real> quotes) {
    Natural settlementDays = 0;
    Calendar calendar = TARGET();
    BusinessDayConvention bdc = Following;
    DayCounter dc = Actual365Fixed();

    vector<Handle<Quote>> allQuotes(quotes.size());
    vector<vector<Handle<Quote>>> correls(quotes.size());
    for (Size i = 0; i < quotes.size(); i++) {
        Handle<Quote> sq(QuantLib::ext::make_shared<SimpleQuote>(quotes[i]));
        allQuotes[i] = sq;
        vector<Handle<Quote>> qt(tenors.size(), sq);
        correls[i] = qt;
    }
    baseCorrRateHelperTenorsMap_[name] = {1 * Days};
    baseCorrRateHelperValuesMap_[name] = allQuotes;
    baseCorrLossLevelsMap_[name] = lossLevel;

    vector<Real> ll_quotes(lossLevel.size());
    for (Size j = 0; j < lossLevel.size(); j++) {
        ll_quotes[j] = parseReal(lossLevel[j]);
    }
    auto bcts =
        QuantLib::ext::make_shared<QuantExt::InterpolatedBaseCorrelationTermStructure<Bilinear>>(settlementDays, calendar, bdc, tenors, ll_quotes,
                                                                 correls, dc);
    bcts->enableExtrapolation(true);
    baseCorrelations_[make_pair(Market::defaultConfiguration, name)] =
        Handle<QuantExt::BaseCorrelationTermStructure>(bcts);
}

void TestMarketParCurves::createSwaptionVolCurve(const string& name, const vector<Period>& optionTenors,
                                                 const vector<Period>& swapTenors, const vector<Real>& strikeSpreads,
                                                 const vector<Real>& parRates) {
    DayCounter dc = Actual365Fixed();
    Calendar cal = TARGET();
    BusinessDayConvention bdc = Following;
    swaptionVolRateHelperTenorsMap_[name] = optionTenors;
    swaptionVolRateHelperSwapTenorsMap_[name] = swapTenors;

    vector<vector<Handle<Quote>>> parQuotes(optionTenors.size(), vector<Handle<Quote>>(swapTenors.size()));
    vector<vector<Handle<Quote>>> cubeQuotes(optionTenors.size() * swapTenors.size(),
                                             vector<Handle<Quote>>(strikeSpreads.size(), Handle<Quote>()));
    vector<Handle<Quote>> allQuotes(parRates.size());

    vector<vector<Real>> shift(optionTenors.size(), vector<Real>(swapTenors.size(), 0.0));
    for (Size i = 0; i < optionTenors.size(); ++i) {
        for (Size j = 0; j < swapTenors.size(); ++j) {
            for (Size k = 0; k < strikeSpreads.size(); ++k) {
                Size l = (i * swapTenors.size() * strikeSpreads.size()) + j * strikeSpreads.size() + k;
                Handle<Quote> quote = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[l]));
                if (close_enough(strikeSpreads[k], 0.0))
                    parQuotes[i][j] = quote;

                cubeQuotes[i * swapTenors.size() + j][k] = quote;
                allQuotes[l] = quote;
            }
        }
    }
    swaptionVolRateHelperValuesMap_[name] = allQuotes;
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atm(new SwaptionVolatilityMatrix(
        asof_, cal, bdc, optionTenors, swapTenors, parQuotes, dc, true, QuantLib::Normal, shift));

    Handle<SwaptionVolatilityStructure> hATM(atm);
    Handle<SwapIndex> si = swapIndex(swapIndexBase(name));
    Handle<SwapIndex> ssi = swapIndex(shortSwapIndexBase(name));

    QuantLib::ext::shared_ptr<SwaptionVolatilityCube> tmp(new QuantExt::SwaptionVolCube2(
        hATM, optionTenors, swapTenors, strikeSpreads, cubeQuotes, *si, *ssi, false, true, false));
    tmp->enableExtrapolation();

    Handle<SwaptionVolatilityStructure> svp =
        Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<SwaptionVolCubeWithATM>(tmp));

    swaptionCurves_[make_pair(Market::defaultConfiguration, name)] = svp;
}

void TestMarketParCurves::createZeroInflationIndex(const string& idxName, const vector<string>& parInst,
                                                   const vector<Period>& parTenor, const vector<Real>& parRates,
                                                   bool singleCurve) {
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    zeroInflationRateHelperInstMap_[idxName] = parInst;
    zeroInflationRateHelperTenorsMap_[idxName] = parTenor;

    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i] / 100));
    zeroInflationRateHelperValuesMap_[idxName] = parQuotes;

    QuantLib::ext::shared_ptr<ZeroInflationIndex> zii = parseZeroInflationIndex(idxName);
    string ccy = zii->currency().code();
    QuantLib::ext::shared_ptr<ore::data::InflationSwapConvention> conv =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InflationSwapConvention>(conventions->get(idxName));

    std::vector<QuantLib::ext::shared_ptr<ZeroInflationTraits::helper>> instruments;
    ;
    for (Size i = 0; i < parTenor.size(); i++) {
        instruments.push_back(QuantLib::ext::make_shared<ZeroCouponInflationSwapHelper>(
            parQuotes[i], conv->observationLag(), asof_ + parTenor[i], conv->infCalendar(), conv->infConvention(),
            conv->dayCounter(), zii, conv->interpolated() ? QuantLib::CPI::Linear : QuantLib::CPI::Flat,
            yieldCurve(YieldCurveType::Discount, ccy, Market::defaultConfiguration)));
    }
    QuantLib::ext::shared_ptr<ZeroInflationTermStructure> zeroCurve;
    Real baseRate = parQuotes[0]->value();
    zeroCurve = QuantLib::ext::shared_ptr<PiecewiseZeroInflationCurve<Linear>>(
        new PiecewiseZeroInflationCurve<Linear>(asof_, conv->infCalendar(), conv->dayCounter(), conv->observationLag(),
                                                zii->frequency(), baseRate, instruments));
    Handle<ZeroInflationTermStructure> its(zeroCurve);
    its->enableExtrapolation();
    QuantLib::ext::shared_ptr<ZeroInflationIndex> i =
        ore::data::parseZeroInflationIndex(idxName, Handle<ZeroInflationTermStructure>(its));
    Handle<ZeroInflationIndex> zh(i);
    zeroInflationIndices_[make_pair(Market::defaultConfiguration, idxName)] = zh;
}

void TestMarketParCurves::createYoYInflationIndex(const string& idxName, const vector<string>& parInst,
                                                  const vector<Period>& parTenor, const vector<Real>& parRates,
                                                  bool singleCurve) {
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    yoyInflationRateHelperInstMap_[idxName] = parInst;
    yoyInflationRateHelperTenorsMap_[idxName] = parTenor;

    vector<Handle<Quote>> parQuotes(parRates.size());
    for (Size i = 0; i < parRates.size(); ++i)
        parQuotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parRates[i] / 100));
    yoyInflationRateHelperValuesMap_[idxName] = parQuotes;

    QuantLib::ext::shared_ptr<ZeroInflationIndex> zii = parseZeroInflationIndex("UKRPI");
    QuantLib::ext::shared_ptr<YoYInflationIndex> yi = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zii, false);
    string ccy = zii->currency().code();
    QuantLib::ext::shared_ptr<ore::data::InflationSwapConvention> conv =
        QuantLib::ext::dynamic_pointer_cast<ore::data::InflationSwapConvention>(conventions->get(idxName));

    std::vector<QuantLib::ext::shared_ptr<YoYInflationTraits::helper>> instruments;
    for (Size i = 0; i < parTenor.size(); i++) {
        instruments.push_back(QuantLib::ext::make_shared<YearOnYearInflationSwapHelper>(
            parQuotes[i], conv->observationLag(), asof_ + parTenor[i], conv->infCalendar(), conv->infConvention(),
            conv->dayCounter(), yi, yieldCurve(YieldCurveType::Discount, ccy, Market::defaultConfiguration)));
    }
    QuantLib::ext::shared_ptr<YoYInflationTermStructure> yoyCurve;

    Real baseRate = parQuotes[0]->value();
    yoyCurve = QuantLib::ext::shared_ptr<PiecewiseYoYInflationCurve<Linear>>(
        new PiecewiseYoYInflationCurve<Linear>(asof_, conv->fixCalendar(), conv->dayCounter(), conv->observationLag(),
                                               yi->frequency(), conv->interpolated(), baseRate, instruments));
    yoyCurve->enableExtrapolation();
    Handle<YoYInflationTermStructure> its(yoyCurve);
    QuantLib::ext::shared_ptr<YoYInflationIndex> i(yi->clone(its));
    Handle<YoYInflationIndex> zh(i);
    yoyInflationIndices_[make_pair(Market::defaultConfiguration, idxName)] = zh;
}

Handle<YieldTermStructure> TestMarketParCurves::flatRateYts(Real forward) {
    QuantLib::ext::shared_ptr<YieldTermStructure> yts(
        new FlatForward(Settings::instance().evaluationDate(), forward, ActualActual(ActualActual::ISDA)));
    return Handle<YieldTermStructure>(yts);
}
Handle<BlackVolTermStructure> TestMarketParCurves::flatRateFxv(Volatility forward) {
    QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(
        new BlackConstantVol(Settings::instance().evaluationDate(), NullCalendar(), forward, Actual365Fixed()));
    return Handle<BlackVolTermStructure>(fxv);
}
Handle<QuantLib::SwaptionVolatilityStructure> TestMarketParCurves::flatRateSvs(Volatility forward, VolatilityType type,
                                                                               Real shift) {
    QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
        new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                 ModifiedFollowing, forward, ActualActual(ActualActual::ISDA), type, shift));
    return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
}
Handle<DefaultProbabilityTermStructure> TestMarketParCurves::flatRateDcs(Volatility forward) {
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> dcs(new FlatHazardRate(asof_, forward, ActualActual(ActualActual::ISDA)));
    return Handle<DefaultProbabilityTermStructure>(dcs);
}
Handle<OptionletVolatilityStructure> TestMarketParCurves::flatRateCvs(Volatility vol, VolatilityType type, Real shift) {
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ts(new QuantLib::ConstantOptionletVolatility(
        Settings::instance().evaluationDate(), NullCalendar(), ModifiedFollowing, vol, ActualActual(ActualActual::ISDA), type, shift));
    return Handle<OptionletVolatilityStructure>(ts);
}

void TestConfigurationObjects::setConventions() {
    QuantLib::ext::shared_ptr<Conventions> conventions = QuantLib::ext::make_shared<Conventions>();

    // add conventions
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("USD-CMS-1Y", "USD-3M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("USD-CMS-2Y", "USD-3M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("USD-CMS-30Y", "USD-3M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("GBP-CMS-2Y", "GBP-3M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("GBP-CMS-30Y", "GBP-6M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("CHF-CMS-2Y", "CHF-3M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("CHF-CMS-30Y", "CHF-6M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("JPY-CMS-1Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("JPY-CMS-2Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));
    conventions->add(QuantLib::ext::make_shared<ore::data::SwapIndexConvention>("JPY-CMS-30Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));

    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360", "USD-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("GBP-3M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("CHF-3M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "CHF-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "CHF-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("JPY-LIBOR-6M-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "JP", "S", "MF", "ACT", "JPY-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "US", "S", "MF", "30/360", "USD-LIBOR-6M"));

    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    conventions->add(QuantLib::ext::make_shared<ore::data::FraConvention>("EUR-FRA-CONVENTIONS", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::FraConvention>("USD-FRA-CONVENTIONS", "USD-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::FraConvention>("GBP-FRA-CONVENTIONS", "GBP-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::FraConvention>("JPY-FRA-CONVENTIONS", "JPY-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::FraConvention>("CHF-FRA-CONVENTIONS", "CHF-LIBOR-6M"));

    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000", "EUR,USD"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-GBP-FX", "0", "EUR", "GBP", "10000", "EUR,GBP"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-CHF-FX", "0", "EUR", "CHF", "10000", "EUR,CHF"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));

    conventions->add(QuantLib::ext::make_shared<ore::data::FXConvention>("CHF-FX-CONVENTIONS", "0", "CHF", "EUR", "10000",
                                                                 "CHF,EUR", "true"));

    conventions->add(QuantLib::ext::make_shared<ore::data::OisConvention>(
        "JPY-OIS-CONVENTIONS", "2", "JPY-TONAR", "ACT/365", "JPY", "1", "false", "Annual", "MF", "MF", "Backward"));

    conventions->add(QuantLib::ext::make_shared<ore::data::CdsConvention>(
        "CDS-STANDARD-CONVENTIONS", "0", "WeekendsOnly", "Quarterly", "Following", "CDS2015", "A360", "true", "true"));

    conventions->add(QuantLib::ext::make_shared<ore::data::CrossCcyBasisSwapConvention>(
        "CHF-XCCY-BASIS-CONVENTIONS", "2", "CHF,EUR", "MF", "EUR-EURIBOR-6M", "CHF-LIBOR-6M", "false"));

    conventions->add(QuantLib::ext::make_shared<ore::data::InflationSwapConvention>("UKRPI", "UK", "MF", "A365", "UKRPI",
                                                                            "false", "3M", "false", "UK", "MF"));
    conventions->add(QuantLib::ext::make_shared<ore::data::InflationSwapConvention>("UKRP1", "UK", "MF", "A365", "UKRPI",
                                                                            "false", "3M", "false", "UK", "MF"));
    InstrumentConventions::instance().setConventions(conventions);
}

QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>
TestConfigurationObjects::setupSimMarketData(bool hasSwapVolCube, bool hasYYCapVols) {
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveTenors("",
                                       {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                        5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices({"EUR-EURIBOR-2W", "EUR-EURIBOR-1M", "EUR-EURIBOR-3M", "EUR-EURIBOR-6M", "USD-LIBOR-2W",
                               "USD-LIBOR-1M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-2W", "GBP-LIBOR-1M",
                               "GBP-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-2W", "JPY-LIBOR-1M",
                               "JPY-LIBOR-3M", "JPY-LIBOR-6M", "JPY-TONAR"});
    simMarketData->interpolation() = "LogLinear";
    simMarketData->swapIndices()["EUR-CMS-2Y"] = "EUR-EURIBOR-6M";
    simMarketData->swapIndices()["EUR-CMS-30Y"] = "EUR-EURIBOR-6M";

    simMarketData->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years, 7 * Years, 10 * Years,
                                        15 * Years, 20 * Years, 30 * Years});
    simMarketData->setSwapVolExpiries("",
                                      {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                       5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true); // false;
    if (hasSwapVolCube) {
        simMarketData->setSwapVolIsCube("", true);
        simMarketData->simulateSwapVolATMOnly() = false;
        simMarketData->setSwapVolStrikeSpreads("", {-0.02, -0.005, 0.0, 0.005, 0.02});
    }

    simMarketData->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true); // false;
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});
    simMarketData->setFxVolIsSurface(true);
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.5, 1, 1.5, 2, 2.5, 3});
    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    simMarketData->setDefaultNames({"dc", "dc2", "dc3"});
    simMarketData->setDefaultTenors("", {3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years,
                                         13 * Years, 15 * Years, 20 * Years});
    simMarketData->setSimulateSurvivalProbabilities(true);
    simMarketData->setSimulateRecoveryRates(false);
    simMarketData->setDefaultCurveCalendars("", "TARGET");

    simMarketData->setSimulateCdsVols(true);
    simMarketData->cdsVolExpiries() = {6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years};
    simMarketData->cdsVolDecayMode() = "ForwardVariance";
    simMarketData->setCdsVolNames({"dc", "dc2", "dc3"});

    simMarketData->setEquityNames({"SP5", "Lufthansa"});
    simMarketData->setEquityDividendTenors("SP5", {6 * Months, 1 * Years, 2 * Years});
    simMarketData->setEquityDividendTenors("Lufthansa", {6 * Months, 1 * Years, 2 * Years});

    simMarketData->setSimulateEquityVols(true);
    simMarketData->setEquityVolDecayMode("ForwardVariance");
    simMarketData->setEquityVolNames({"SP5", "Lufthansa"});
    simMarketData->setEquityVolExpiries("",
                                        {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                         5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setEquityVolIsSurface("", true);
    simMarketData->setEquityVolMoneyness(
        "", {0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.5, 3.0});

    simMarketData->setSimulateBaseCorrelations(true);
    simMarketData->setBaseCorrelationNames({"Tranch1"});
    simMarketData->baseCorrelationDetachmentPoints() = {0.03, 0.06, 0.09, 0.12, 0.22, 1.0};
    simMarketData->baseCorrelationTerms() = {1 * Days};

    simMarketData->setZeroInflationIndices({"UKRPI"});
    simMarketData->setZeroInflationTenors("UKRPI", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years,
                                                    10 * Years, 15 * Years, 20 * Years});

    simMarketData->setYoyInflationIndices({"UKRPI"});
    simMarketData->setYoyInflationTenors(
        "UKRPI", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});

    if (hasYYCapVols) {
        simMarketData->setSimulateYoYInflationCapFloorVols(true);
        simMarketData->setYoYInflationCapFloorVolNames({"UKRPI"});
        simMarketData->setYoYInflationCapFloorVolExpiries(
            "UKRPI", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
        simMarketData->setYoYInflationCapFloorVolStrikes("", {-0.02, -0.01, 0.00, 0.01, 0.02, 0.03});
        simMarketData->yoyInflationCapFloorVolDecayMode() = "ForwardVariance";
    }

    return simMarketData;
}

ore::analytics::SensitivityScenarioData::CurveShiftParData createCurveData() {
    ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData;

    cvsData.shiftTenors = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years, 3 * Years,
                           5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.00001;
    cvsData.parInstruments = {"DEP", "DEP", "DEP", "DEP", "FRA", "IRS", "IRS",
                              "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};

    return cvsData;
}

QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>
TestConfigurationObjects::setupSensitivityScenarioData(bool hasSwapVolCube, bool hasYYCapVols, bool parConversion) {
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData>(parConversion);
    vector<string> oisInstruments = {"OIS", "OIS", "OIS", "OIS", "OIS", "OIS", "OIS",
                                     "OIS", "OIS", "OIS", "OIS", "OIS", "OIS"};
    vector<string> xbsInstruments = {"FXF", "FXF", "FXF", "FXF", "FXF", "XBS", "XBS",
                                     "XBS", "XBS", "XBS", "XBS", "XBS", "XBS"};

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.001;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
    fxvsData.shiftSize = 0.1;
    fxvsData.shiftExpiries = {5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 0.00001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.10};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
    swvsData.shiftSize = 0.001;
    swvsData.shiftExpiries = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years, 3 * Years,
                              5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};
    swvsData.shiftTerms = {1 * Years, 2 * Years,  3 * Years,  4 * Years,  5 * Years,
                           7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years};
    if (hasSwapVolCube)
        swvsData.shiftStrikes = {-0.02, -0.005, 0.0, 0.005, 0.02};

    ore::analytics::SensitivityScenarioData::CdsVolShiftData cdsvsData;
    cdsvsData.shiftType = ShiftType::Relative;
    cdsvsData.shiftSize = 0.01;
    cdsvsData.shiftExpiries = {6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years};

    ore::analytics::SensitivityScenarioData::SpotShiftData eqsData;
    eqsData.shiftType = ShiftType::Relative;
    eqsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData eqvsData;
    eqvsData.shiftType = ShiftType::Relative;
    eqvsData.shiftSize = 0.01;
    eqvsData.shiftExpiries = {2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years, 3 * Years,
                              5 * Years, 10 * Years, 13 * Years, 15 * Years, 20 * Years, 30 * Years};

    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftData> eqdivData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>();
    eqdivData->shiftType = ShiftType::Absolute;
    eqdivData->shiftSize = 0.00001;
    eqdivData->shiftTenors = {6 * Months, 1 * Years, 2 * Years};

    ore::analytics::SensitivityScenarioData::BaseCorrelationShiftData bcorrData;
    bcorrData.shiftType = ShiftType::Absolute;
    bcorrData.shiftSize = 0.01;
    bcorrData.shiftLossLevels = {0.03, 0.06, 0.09, 0.12, 0.22, 1.0};
    bcorrData.shiftTerms = {1 * Days};

    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftParData> zinfData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>();
    zinfData->shiftType = ShiftType::Absolute;
    zinfData->shiftSize = 0.0001;
    zinfData->shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                             7 * Years,  10 * Years, 15 * Years, 20 * Years};
    zinfData->parInstruments = {"ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS", "ZIS"};
    zinfData->parInstrumentConventions["ZIS"] = "UKRPI";

    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftParData> yinfData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>();
    yinfData->shiftType = ShiftType::Absolute;
    yinfData->shiftSize = 0.0001;
    yinfData->shiftTenors = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years};
    yinfData->parInstruments = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
    yinfData->parInstrumentConventions["ZIS"] = "UKRPI";
    yinfData->parInstrumentConventions["YYS"] = "UKRPI";

    auto yinfCfData = QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftParData>();
    yinfCfData->shiftType = ShiftType::Absolute;
    yinfCfData->shiftSize = 0.00001;
    yinfCfData->shiftExpiries = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                                 7 * Years, 10 * Years, 15 * Years, 20 * Years};
    yinfCfData->shiftStrikes = {-0.02, -0.01, 0.00, 0.01, 0.02, 0.03};
    yinfCfData->parInstruments = {"YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS", "YYS"};
    yinfCfData->parInstrumentSingleCurve = false;
    yinfCfData->parInstrumentConventions["ZIS"] = "UKRPI";
    yinfCfData->parInstrumentConventions["YYS"] = "UKRPI";

    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = true;
        cvsData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "EUR-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
        sensiData->discountCurveShiftData()["EUR"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }

    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = true;
        cvsData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "USD-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "USD-3M-SWAP-CONVENTIONS";
        sensiData->discountCurveShiftData()["USD"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }

    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = true;
        cvsData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "GBP-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
        sensiData->discountCurveShiftData()["GBP"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }

    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = true;
        cvsData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "JPY-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["OIS"] = "JPY-OIS-CONVENTIONS";
        cvsData.parInstruments = oisInstruments; // aligned with market setup
        sensiData->discountCurveShiftData()["JPY"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = true;
        cvsData.parInstrumentConventions["DEP"] = "CHF-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "CHF-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "CHF-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["FXF"] = "CHF-FX-CONVENTIONS";
        cvsData.parInstrumentConventions["XBS"] = "CHF-XCCY-BASIS-CONVENTIONS";
        cvsData.parInstruments = xbsInstruments; // aligned with market setup
        sensiData->discountCurveShiftData()["CHF"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "EUR-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["EUR-EURIBOR-2W"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "EUR-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["EUR-EURIBOR-1M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "EUR-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["EUR-EURIBOR-3M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "EUR-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "USD-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "USD-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["USD-LIBOR-2W"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "USD-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "USD-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["USD-LIBOR-1M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "USD-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "USD-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "USD-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "USD-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["USD-LIBOR-6M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "GBP-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["GBP-LIBOR-2W"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "GBP-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["GBP-LIBOR-1M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "GBP-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["GBP-LIBOR-3M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "GBP-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
        sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "JPY-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["OIS"] = "JPY-OIS-CONVENTIONS";
        sensiData->indexCurveShiftData()["JPY-TONAR"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "JPY-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["OIS"] = "JPY-OIS-CONVENTIONS";
        sensiData->indexCurveShiftData()["JPY-LIBOR-2W"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "JPY-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["OIS"] = "JPY-OIS-CONVENTIONS";
        sensiData->indexCurveShiftData()["JPY-LIBOR-1M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "JPY-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["OIS"] = "JPY-OIS-CONVENTIONS";
        sensiData->indexCurveShiftData()["JPY-LIBOR-3M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = false;
        cvsData.parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "JPY-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["OIS"] = "JPY-OIS-CONVENTIONS";
        sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.parInstrumentSingleCurve = true;
        cvsData.parInstrumentConventions["DEP"] = "CHF-DEP-CONVENTIONS";
        cvsData.parInstrumentConventions["FRA"] = "CHF-FRA-CONVENTIONS";
        cvsData.parInstrumentConventions["IRS"] = "CHF-6M-SWAP-CONVENTIONS";
        cvsData.parInstrumentConventions["FXF"] = "CHF-FX-CONVENTIONS";
        cvsData.parInstrumentConventions["XBS"] = "CHF-XCCY-BASIS-CONVENTIONS";
        sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolShiftData()["EUR"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["EUR"]->indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["USD"]->indexName = "USD-LIBOR-6M";

    sensiData->creditCcys()["dc"] = "USD";
    sensiData->creditCcys()["dc2"] = "EUR";
    sensiData->creditCcys()["dc3"] = "GBP";
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.shiftTenors = {3 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,
                               5 * Years,  10 * Years, 13 * Years, 15 * Years, 20 * Years};
        cvsData.parInstruments = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
        cvsData.parInstrumentConventions["CDS"] = "CDS-STANDARD-CONVENTIONS";
        sensiData->creditCurveShiftData()["dc"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.shiftTenors = {3 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,
                               5 * Years,  10 * Years, 13 * Years, 15 * Years, 20 * Years};
        cvsData.parInstruments = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
        cvsData.parInstrumentConventions["CDS"] = "CDS-STANDARD-CONVENTIONS";
        sensiData->creditCurveShiftData()["dc2"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    {
        ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData = createCurveData();
        cvsData.shiftTenors = {3 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,
                               5 * Years,  10 * Years, 13 * Years, 15 * Years, 20 * Years};
        cvsData.parInstruments = {"CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS", "CDS"};
        cvsData.parInstrumentConventions["CDS"] = "CDS-STANDARD-CONVENTIONS";
        sensiData->creditCurveShiftData()["dc3"] =
            QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
    }
    sensiData->cdsVolShiftData()["dc"] = cdsvsData;
    sensiData->cdsVolShiftData()["dc2"] = cdsvsData;
    sensiData->cdsVolShiftData()["dc3"] = cdsvsData;

    sensiData->equityShiftData()["SP5"] = eqsData;
    sensiData->equityShiftData()["Lufthansa"] = eqsData;

    sensiData->equityVolShiftData()["SP5"] = eqvsData;
    sensiData->equityVolShiftData()["Lufthansa"] = eqvsData;
    sensiData->dividendYieldShiftData()["SP5"] = eqdivData;
    sensiData->dividendYieldShiftData()["Lufthansa"] = eqdivData;

    sensiData->baseCorrelationShiftData()["Tranch1"] = bcorrData;

    sensiData->zeroInflationCurveShiftData()["UKRPI"] = zinfData;

    sensiData->yoyInflationCurveShiftData()["UKRPI"] = yinfData;

    if (hasYYCapVols)
        sensiData->yoyInflationCapFloorVolShiftData()["UKRPI"] = yinfCfData;

    return sensiData;
};

    
QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> TestConfigurationObjects::setupSimMarketData2() {
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP"});
    simMarketData->setYieldCurveNames({"BondCurve0"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 6 * Years, 7 * Years, 8 * Years, 9 * Years, 10 * Years,
                                            12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years});
    simMarketData->setIndices({"EUR-EURIBOR-6M", "GBP-LIBOR-6M"});
    simMarketData->setDefaultNames({"BondIssuer0"});
    simMarketData->setDefaultTenors(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setSecurities({"Bond0"});
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

QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> TestConfigurationObjects::setupSimMarketData5() {
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData(
        new ore::analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices(
        {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    simMarketData->swapIndices()["EUR-CMS-2Y"] = "EUR-EURIBOR-6M";
    simMarketData->swapIndices()["EUR-CMS-30Y"] = "EUR-EURIBOR-6M";

    simMarketData->setYieldCurveNames({"BondCurve0"});
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

    simMarketData->setDefaultNames({"BondIssuer0"});
    simMarketData->setDefaultTenors(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setSimulateSurvivalProbabilities(true);
    simMarketData->setSecurities({"Bond0"});
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

QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData2() {
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData>();

    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
                           7 * Years, 10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.05};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

    sensiData->discountCurveShiftData()["EUR"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->yieldCurveShiftData()["BondCurve0"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    sensiData->creditCurveShiftData()["BondIssuer0"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
    // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
    // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

    return sensiData;
}

QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData2b() {
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData>();

    // shift curve has more points than underlying curve, has tenor points where the underlying curve hasn't, skips some
    // tenor points that occur in the underlying curve (e.g. 2Y, 3Y, 4Y)
    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {1 * Years,   15 * Months, 18 * Months, 21 * Months, 27 * Months, 30 * Months, 33 * Months,
                           42 * Months, 54 * Months, 5 * Years,   6 * Years,   7 * Years,   8 * Years,   9 * Years,
                           10 * Years,  11 * Years,  12 * Years,  13 * Years,  14 * Years,  15 * Years,  16 * Years,
                           17 * Years,  18 * Years,  19 * Years,  20 * Years};
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.05};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

    sensiData->discountCurveShiftData()["EUR"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->yieldCurveShiftData()["BondCurve0"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    sensiData->creditCurveShiftData()["BondIssuer0"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
    // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
    // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

    return sensiData;
}

QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> TestConfigurationObjects::setupSensitivityScenarioData5() {
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData>();

    ore::analytics::SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                           7 * Years,  10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 0.0001;

    ore::analytics::SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Relative;
    fxsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Relative;
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = {5 * Years};

    ore::analytics::SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Relative;
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = {2 * Years, 5 * Years, 10 * Years};
    swvsData.shiftTerms = {5 * Years, 10 * Years};

    ore::analytics::SensitivityScenarioData::SpotShiftData eqsData;
    eqsData.shiftType = ShiftType::Relative;
    eqsData.shiftSize = 0.01;

    ore::analytics::SensitivityScenarioData::VolShiftData eqvsData;
    eqvsData.shiftType = ShiftType::Relative;
    eqvsData.shiftSize = 0.01;
    eqvsData.shiftExpiries = {5 * Years};

    ore::analytics::SensitivityScenarioData::CurveShiftData zinfData;
    zinfData.shiftType = ShiftType::Absolute;
    zinfData.shiftSize = 0.0001;
    zinfData.shiftTenors = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years};

    auto commodityShiftData = QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>();
    commodityShiftData->shiftType = ShiftType::Relative;
    commodityShiftData->shiftSize = 0.01;
    commodityShiftData->shiftTenors = {0 * Days, 1 * Years, 2 * Years, 5 * Years};

    ore::analytics::SensitivityScenarioData::VolShiftData commodityVolShiftData;
    commodityVolShiftData.shiftType = ShiftType::Relative;
    commodityVolShiftData.shiftSize = 0.01;
    commodityVolShiftData.shiftExpiries = {1 * Years, 2 * Years, 5 * Years};
    commodityVolShiftData.shiftStrikes = {0.9, 0.95, 1.0, 1.05, 1.1};

    sensiData->discountCurveShiftData()["EUR"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["USD"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["GBP"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["JPY"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->discountCurveShiftData()["CHF"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->yieldCurveShiftData()["BondCurve0"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->creditCurveShiftData()["BondIssuer0"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(cvsData);

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
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["EUR"]->indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["USD"]->indexName = "USD-LIBOR-3M";

    sensiData->equityShiftData()["SP5"] = eqsData;
    sensiData->equityShiftData()["Lufthansa"] = eqsData;

    sensiData->equityVolShiftData()["SP5"] = eqvsData;
    sensiData->equityVolShiftData()["Lufthansa"] = eqvsData;

    sensiData->zeroInflationCurveShiftData()["UKRPI"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(zinfData);

    sensiData->yoyInflationCurveShiftData()["UKRPI"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftData>(zinfData);

    sensiData->commodityCurveShiftData()["COMDTY_GOLD_USD"] = commodityShiftData;
    sensiData->commodityCurrencies()["COMDTY_GOLD_USD"] = "USD";
    sensiData->commodityCurveShiftData()["COMDTY_WTI_USD"] = commodityShiftData;
    sensiData->commodityCurrencies()["COMDTY_WTI_USD"] = "USD";

    sensiData->commodityVolShiftData()["COMDTY_GOLD_USD"] = commodityVolShiftData;
    sensiData->commodityVolShiftData()["COMDTY_WTI_USD"] = commodityVolShiftData;

    return sensiData;
}

} // namespace testsuite
