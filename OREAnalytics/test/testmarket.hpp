/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace openriskengine::data;
using std::vector;
using std::pair;

namespace testsuite {

//! Simple flat market setup to be used in the test suite
/*!
  \ingroup tests
*/
class TestMarket : public openriskengine::data::MarketImpl {
public:
    TestMarket(Date asof) {
        asof_ = asof;

        // add conventions
        boost::shared_ptr<openriskengine::data::Convention> swapIndexEURConv(
            new openriskengine::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexEURLongConv(
            new openriskengine::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexUSDConv(
            new openriskengine::data::SwapIndexConvention("USD-CMS-2Y", "USD-3M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexUSDLongConv(
            new openriskengine::data::SwapIndexConvention("USD-CMS-30Y", "USD-3M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexGBPConv(
            new openriskengine::data::SwapIndexConvention("GBP-CMS-2Y", "GBP-3M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexGBPLongConv(
            new openriskengine::data::SwapIndexConvention("GBP-CMS-30Y", "GBP-6M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexCHFConv(
            new openriskengine::data::SwapIndexConvention("CHF-CMS-2Y", "CHF-3M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexCHFLongConv(
            new openriskengine::data::SwapIndexConvention("CHF-CMS-30Y", "CHF-6M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexJPYConv(
            new openriskengine::data::SwapIndexConvention("JPY-CMS-2Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));
        boost::shared_ptr<openriskengine::data::Convention> swapIndexJPYLongConv(
            new openriskengine::data::SwapIndexConvention("JPY-CMS-30Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));

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

        boost::shared_ptr<openriskengine::data::Convention> swapEURConv(new openriskengine::data::IRSwapConvention(
            "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
        boost::shared_ptr<openriskengine::data::Convention> swapUSDConv(new openriskengine::data::IRSwapConvention(
            "USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360", "USD-LIBOR-3M"));
        boost::shared_ptr<openriskengine::data::Convention> swapGBPConv(new openriskengine::data::IRSwapConvention(
            "GBP-3M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-3M"));
        boost::shared_ptr<openriskengine::data::Convention> swapGBPLongConv(new openriskengine::data::IRSwapConvention(
            "GBP-6M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-6M"));
        boost::shared_ptr<openriskengine::data::Convention> swapCHFConv(new openriskengine::data::IRSwapConvention(
            "CHF-3M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-3M"));
        boost::shared_ptr<openriskengine::data::Convention> swapCHFLongConv(new openriskengine::data::IRSwapConvention(
            "CHF-6M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-6M"));
        boost::shared_ptr<openriskengine::data::Convention> swapJPYConv(new openriskengine::data::IRSwapConvention(
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
        vector<pair<string, Real>> indexData = {{"EUR-EONIA", 0.01},
                                                {"EUR-EURIBOR-6M", 0.02},
                                                {"USD-FedFunds", 0.01},
                                                {"USD-LIBOR-3M", 0.03},
                                                {"USD-LIBOR-6M", 0.05},
                                                {"GBP-SONIA", 0.01},
                                                {"GBP-LIBOR-3M", 0.03},
                                                {"GBP-LIBOR-6M", 0.04},
                                                {"CHF-LIBOR-3M", 0.01},
                                                {"CHF-LIBOR-6M", 0.02},
                                                {"JPY-LIBOR-6M", 0.01}};
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

        // build default curves
        defaultCurves_[make_pair(Market::defaultConfiguration, "dc")] = flatRateDcs(0.1);
        defaultCurves_[make_pair(Market::defaultConfiguration, "dc2")] = flatRateDcs(0.2);
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
};
}
