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
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace ore::data;
using std::vector;
using std::pair;

namespace testsuite {

//! Simple flat market setup to be used in the test suite
/*!
  \ingroup tests
*/
class TestMarket : public ore::data::MarketImpl {
public:
    TestMarket(Date asof) {
        asof_ = asof;

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

        conventions_.add(swapIndexEURConv);
        conventions_.add(swapIndexEURLongConv);
        conventions_.add(swapIndexUSDConv);
        conventions_.add(swapIndexUSDLongConv);
        conventions_.add(swapIndexGBPConv);
        conventions_.add(swapIndexGBPLongConv);
        conventions_.add(swapIndexCHFConv);
        conventions_.add(swapIndexCHFLongConv);
        conventions_.add(swapIndexJPYConv);
        conventions_.add(swapIndexJPYLongConv);

        boost::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
            "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
        boost::shared_ptr<ore::data::Convention> swapUSDConv(new ore::data::IRSwapConvention(
            "USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360", "USD-LIBOR-3M"));
        boost::shared_ptr<ore::data::Convention> swapGBPConv(new ore::data::IRSwapConvention(
            "GBP-3M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-3M"));
        boost::shared_ptr<ore::data::Convention> swapGBPLongConv(new ore::data::IRSwapConvention(
            "GBP-6M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-6M"));
        boost::shared_ptr<ore::data::Convention> swapCHFConv(new ore::data::IRSwapConvention(
            "CHF-3M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-3M"));
        boost::shared_ptr<ore::data::Convention> swapCHFLongConv(new ore::data::IRSwapConvention(
            "CHF-6M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-6M"));
        boost::shared_ptr<ore::data::Convention> swapJPYConv(new ore::data::IRSwapConvention(
            "JPY-LIBOR-6M-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M"));

        conventions_.add(swapEURConv);
        conventions_.add(swapUSDConv);
        conventions_.add(swapGBPConv);
        conventions_.add(swapGBPLongConv);
        conventions_.add(swapCHFConv);
        conventions_.add(swapCHFLongConv);
        conventions_.add(swapJPYConv);

        // build discount
        discountCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateYts(0.02);
        discountCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateYts(0.03);
        discountCurves_[make_pair(Market::defaultConfiguration, "GBP")] = flatRateYts(0.04);
        discountCurves_[make_pair(Market::defaultConfiguration, "CHF")] = flatRateYts(0.01);
        discountCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateYts(0.005);

        // build ibor indices
        vector<pair<string, Real>> indexData = {
            {"EUR-EONIA", 0.01},    {"EUR-EURIBOR-6M", 0.02}, {"USD-FedFunds", 0.01}, {"USD-LIBOR-3M", 0.03},
            {"USD-LIBOR-6M", 0.05}, {"GBP-SONIA", 0.01},      {"GBP-LIBOR-3M", 0.03}, {"GBP-LIBOR-6M", 0.04},
            {"CHF-LIBOR-3M", 0.01}, {"CHF-LIBOR-6M", 0.02},   {"JPY-LIBOR-6M", 0.01}};
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
        fxSpots_[Market::defaultConfiguration].addQuote("EURUSD", Handle<Quote>(boost::make_shared<SimpleQuote>(1.2)));
        fxSpots_[Market::defaultConfiguration].addQuote("EURGBP", Handle<Quote>(boost::make_shared<SimpleQuote>(0.8)));
        fxSpots_[Market::defaultConfiguration].addQuote("EURCHF", Handle<Quote>(boost::make_shared<SimpleQuote>(1.0)));
        fxSpots_[Market::defaultConfiguration].addQuote("EURJPY",
                                                        Handle<Quote>(boost::make_shared<SimpleQuote>(128.0)));

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

        equityDividendCurves_[make_pair(Market::defaultConfiguration, "SP5")] = flatRateDiv(0.01);
        equityDividendCurves_[make_pair(Market::defaultConfiguration, "Lufthansa")] = flatRateDiv(0.0);

        // build swaption vols
        swaptionCurves_[make_pair(Market::defaultConfiguration, "EUR")] = flatRateSvs(0.20);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "USD")] = flatRateSvs(0.30);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "GBP")] = flatRateSvs(0.25);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "CHF")] = flatRateSvs(0.25);
        swaptionCurves_[make_pair(Market::defaultConfiguration, "JPY")] = flatRateSvs(0.25);
        swaptionIndexBases_[make_pair(Market::defaultConfiguration, "EUR")] =
            std::make_pair("EUR-CMS-2Y", "EUR-CMS-30Y");
        swaptionIndexBases_[make_pair(Market::defaultConfiguration, "USD")] =
            std::make_pair("USD-CMS-2Y", "USD-CMS-30Y");
        swaptionIndexBases_[make_pair(Market::defaultConfiguration, "GBP")] =
            std::make_pair("GBP-CMS-2Y", "GBP-CMS-30Y");
        swaptionIndexBases_[make_pair(Market::defaultConfiguration, "CHF")] =
            std::make_pair("CHF-CMS-2Y", "CHF-CMS-30Y");
        swaptionIndexBases_[make_pair(Market::defaultConfiguration, "JPY")] =
            std::make_pair("JPY-CMS-2Y", "JPY-CMS-30Y");

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

        recoveryRates_[make_pair(Market::defaultConfiguration, "dc")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.4));
        recoveryRates_[make_pair(Market::defaultConfiguration, "dc2")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.4));
        recoveryRates_[make_pair(Market::defaultConfiguration, "BondIssuer1")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));

        yieldCurves_[make_pair(Market::defaultConfiguration, "BondCurve1")] = flatRateYts(0.05);

        securitySpreads_[make_pair(Market::defaultConfiguration, "Bond1")] =
            Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), forward, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        boost::shared_ptr<BlackVolTermStructure> fxv(
            new BlackConstantVol(Settings::instance().evaluationDate(), NullCalendar(), forward, ActualActual()));
        return Handle<BlackVolTermStructure>(fxv);
    }
    Handle<YieldTermStructure> flatRateDiv(Real dividend) {
        boost::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), dividend, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantLib::SwaptionVolatilityStructure>
    flatRateSvs(Volatility forward, VolatilityType type = ShiftedLognormal, Real shift = 0.0) {
        boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                     ModifiedFollowing, forward, ActualActual(), type, shift));
        return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
    }
    Handle<DefaultProbabilityTermStructure> flatRateDcs(Volatility forward) {
        boost::shared_ptr<DefaultProbabilityTermStructure> dcs(new FlatHazardRate(asof_, forward, ActualActual()));
        return Handle<DefaultProbabilityTermStructure>(dcs);
    }
    Handle<OptionletVolatilityStructure> flatRateCvs(Volatility vol, VolatilityType type = Normal, Real shift = 0.0) {
        boost::shared_ptr<OptionletVolatilityStructure> ts(
            new QuantLib::ConstantOptionletVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                      ModifiedFollowing, vol, ActualActual(), type, shift));
        return Handle<OptionletVolatilityStructure>(ts);
    }
};

//! Static class to allow for easy construction of configuration objects for use within tests
class TestConfigurationObjects {
public:
    //! ScenarioSimMarketParameters instance, 2 currencies
    static boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> setupSimMarketData2();
    //! ScenarioSimMarketParameters instance, 5 currencies
    static boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> setupSimMarketData5();
    //! SensitivityScenarioData instance, 2 currencies
    static boost::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData2();
    //! SensitivityScenarioData instance, 5 currencies
    static boost::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData5();
    //! SensitivityScenarioData instance, 2 currencies, shifts more granular than base curve
    static boost::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData2b();
    //! Conventions instance
    static boost::shared_ptr<ore::data::Conventions> conv();
};
} // namespace testsuite
