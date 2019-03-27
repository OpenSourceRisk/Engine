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

#include <algorithm>
#include <ored/configuration/genericyieldvolcurveconfig.hpp>
#include <ored/marketdata/genericyieldvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

GenericYieldVolCurve::GenericYieldVolCurve(
    const Date& asof, const Loader& loader, const boost::shared_ptr<GenericYieldVolatilityCurveConfig>& config,
    const map<string, boost::shared_ptr<SwapIndex>>& requiredSwapIndices,
    const std::function<bool(const boost::shared_ptr<MarketDatum>& md, Period& expiry, Period& term)>& matchAtmQuote,
    const std::function<bool(const boost::shared_ptr<MarketDatum>& md, Period& expiry, Period& term, Real& strike)>&
        matchSmileQuote,
    const std::function<bool(const boost::shared_ptr<MarketDatum>& md, Period& term)>& matchShiftQuote) {

    try {

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole matrix or do not have more quotes in the market data

        MarketDatum::QuoteType volatilityType;
        switch (config->volatilityType()) {
        case GenericYieldVolatilityCurveConfig::VolatilityType::Lognormal:
            volatilityType = MarketDatum::QuoteType::RATE_LNVOL;
            break;
        case GenericYieldVolatilityCurveConfig::VolatilityType::Normal:
            volatilityType = MarketDatum::QuoteType::RATE_NVOL;
            break;
        case GenericYieldVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
            volatilityType = MarketDatum::QuoteType::RATE_SLNVOL;
            break;
        default:
            QL_FAIL("unexpected volatility type");
        }
        bool isSln = volatilityType == MarketDatum::QuoteType::RATE_SLNVOL;
        Matrix vols(config->optionTenors().size(), config->underlyingTenors().size());
        Matrix shifts(isSln ? vols.rows() : 0, isSln ? vols.columns() : 0);
        Size quotesRead = 0, shiftQuotesRead = 0;

        for (auto& p : config->quotes()) {
            boost::shared_ptr<MarketDatum> md = loader.get(p, asof);
            Period expiry, term;
            if (md->quoteType() == volatilityType && matchAtmQuote(md, expiry, term)) {
                quotesRead++;
                Size i = std::find(config->optionTenors().begin(), config->optionTenors().end(), expiry) -
                         config->optionTenors().begin();
                Size j = std::find(config->underlyingTenors().begin(), config->underlyingTenors().end(), term) -
                         config->underlyingTenors().begin();
                if (i < config->optionTenors().size() && j < config->underlyingTenors().size()) {
                    vols[i][j] = md->quote()->value();
                }
            }
            if (isSln && md->quoteType() == MarketDatum::QuoteType::SHIFT && matchShiftQuote(md, term)) {
                shiftQuotesRead++;
                Size j = std::find(config->underlyingTenors().begin(), config->underlyingTenors().end(), term) -
                         config->underlyingTenors().begin();
                if (j < config->underlyingTenors().size()) {
                    for (Size i = 0; i < shifts.rows(); ++i)
                        shifts[i][j] = md->quote()->value();
                }
            }
        }

        LOG("GenericYieldVolCurve: read " << quotesRead << " vols");

        boost::shared_ptr<SwaptionVolatilityStructure> atm;

        if (quotesRead != 1) {
            atm = boost::shared_ptr<SwaptionVolatilityStructure>(new SwaptionVolatilityMatrix(
                asof, config->calendar(), config->businessDayConvention(), config->optionTenors(),
                config->underlyingTenors(), vols, config->dayCounter(), config->flatExtrapolation(),
                config->volatilityType() == GenericYieldVolatilityCurveConfig::VolatilityType::Normal
                    ? QuantLib::Normal
                    : QuantLib::ShiftedLognormal,
                shifts));

            atm->enableExtrapolation(config->extrapolate());
        } else {
            // Constant volatility
            atm = boost::shared_ptr<SwaptionVolatilityStructure>(new ConstantSwaptionVolatility(
                asof, config->calendar(), config->businessDayConvention(), vols[0][0], config->dayCounter(),
                config->volatilityType() == GenericYieldVolatilityCurveConfig::VolatilityType::Normal
                    ? QuantLib::Normal
                    : QuantLib::ShiftedLognormal,
                !shifts.empty() ? shifts[0][0] : 0.0));
        }

        if (config->dimension() == GenericYieldVolatilityCurveConfig::Dimension::ATM) {
            // Nothing more to do
            LOG("Returning ATM surface for config " << config->curveID());
            vol_ = atm;
        } else {
            LOG("Building Cube for config " << config->curveID());
            vector<Period> smileOptionTenors = config->smileOptionTenors();
            vector<Period> smileSwapTenors = config->smileUnderlyingTenors();
            vector<Spread> spreads = config->smileSpreads();
            vector<vector<bool>> zero(smileOptionTenors.size() * smileSwapTenors.size(),
                                      std::vector<bool>(spreads.size(), false));

            // add smile spread 0, if not already existent
            auto spr = std::upper_bound(spreads.begin(), spreads.end(), 0.0);
            if (!close_enough(*spr, 0.0)) {
                spreads.insert(spr, 0.0);
            }

            if (smileOptionTenors.size() == 0)
                smileOptionTenors = config->optionTenors();
            if (smileSwapTenors.size() == 0)
                smileSwapTenors = config->underlyingTenors();
            QL_REQUIRE(spreads.size() > 0, "Need at least 1 strike spread for a SwaptionVolCube");

            Size n = smileOptionTenors.size() * smileSwapTenors.size();
            vector<vector<Handle<Quote>>> volSpreadHandles(
                n, vector<Handle<Quote>>(spreads.size(), Handle<Quote>(boost::make_shared<SimpleQuote>(0.0))));

            LOG("vol cube smile option tenors " << smileOptionTenors.size());
            LOG("vol cube smile swap tenors " << smileSwapTenors.size());
            LOG("vol cube strike spreads " << spreads.size());

            Size spreadQuotesRead = 0;
            for (auto& p : config->quotes()) {
                boost::shared_ptr<MarketDatum> md = loader.get(p, asof);
                Period expiry, term;
                Real strike;
                if (md->quoteType() == volatilityType && matchSmileQuote(md, expiry, term, strike)) {

                    Size i = std::find(smileOptionTenors.begin(), smileOptionTenors.end(), expiry) -
                             smileOptionTenors.begin();
                    Size j = std::find(smileSwapTenors.begin(), smileSwapTenors.end(), term) - smileSwapTenors.begin();
                    // In the MarketDatum we call it a strike, but it's really a spread
                    Size k = std::find(spreads.begin(), spreads.end(), strike) - spreads.begin();

                    if (i < smileOptionTenors.size() && j < smileSwapTenors.size() && k < spreads.size()) {
                        spreadQuotesRead++;
                        // Assume quotes are absolute vols by strike so construct the vol spreads here
                        Volatility atmVol = atm->volatility(smileOptionTenors[i], smileSwapTenors[j], 0.0);
                        volSpreadHandles[i * smileSwapTenors.size() + j][k] =
                            Handle<Quote>(boost::make_shared<SimpleQuote>(md->quote()->value() - atmVol));
                        zero[i * smileSwapTenors.size() + j][k] = close_enough(md->quote()->value(), 0.0);
                    }
                }
            }
            LOG("Read " << spreadQuotesRead << " quotes for VolCube.");

            // post processing: extrapolate leftmost non-zero value flat to the left and overwrite
            // zero values
            for (Size i = 0; i < smileOptionTenors.size(); ++i) {
                for (Size j = 0; j < smileSwapTenors.size(); ++j) {
                    Real lastNonZeroValue = 0.0;
                    for (Size k = 0; k < spreads.size(); ++k) {
                        boost::shared_ptr<SimpleQuote> q = boost::static_pointer_cast<SimpleQuote>(
                            *volSpreadHandles[i * smileSwapTenors.size() + j][spreads.size() - 1 - k]);
                        if (zero[i * smileSwapTenors.size() + j][spreads.size() - 1 - k]) {
                            q->setValue(lastNonZeroValue);
                            WLOG("Overwrite vol spread for "
                                 << config->curveID() << "/" << smileOptionTenors[i] << "/" << smileSwapTenors[j] << "/"
                                 << spreads[spreads.size() - 1 - k] << " with " << lastNonZeroValue
                                 << " since market quote is zero");
                        } else {
                            lastNonZeroValue = q->value();
                        }
                    }
                }
            }

            // log vols
            for (Size i = 0; i < smileOptionTenors.size(); ++i) {
                for (Size j = 0; j < smileSwapTenors.size(); ++j) {
                    ostringstream o;
                    for (Size k = 0; k < spreads.size(); ++k) {
                        o << volSpreadHandles[i * smileSwapTenors.size() + j][k]->value() +
                                 atm->volatility(smileOptionTenors[i], smileSwapTenors[j], 0.0)
                          << " ";
                    }
                    DLOG("Vols for " << smileOptionTenors[i] << "/" << smileSwapTenors[j] << ": " << o.str());
                }
            }

            // build swap indices
            auto it = requiredSwapIndices.find(config->swapIndexBase());
            QL_REQUIRE(it != requiredSwapIndices.end(), "Unable to find SwapIndex " << config->swapIndexBase());
            boost::shared_ptr<SwapIndex> swapIndexBase = it->second;

            it = requiredSwapIndices.find(config->shortSwapIndexBase());
            QL_REQUIRE(it != requiredSwapIndices.end(), "Unable to find SwapIndex " << config->shortSwapIndexBase());
            boost::shared_ptr<SwapIndex> shortSwapIndexBase = it->second;

            bool vegaWeighedSmileFit = false; // TODO

            Handle<SwaptionVolatilityStructure> hATM(atm);
            boost::shared_ptr<QuantExt::SwaptionVolCube2> cube = boost::make_shared<QuantExt::SwaptionVolCube2>(
                hATM, smileOptionTenors, smileSwapTenors, spreads, volSpreadHandles, swapIndexBase, shortSwapIndexBase,
                vegaWeighedSmileFit, config->flatExtrapolation());
            cube->enableExtrapolation();

            // Wrap it in a SwaptionVolCubeWithATM
            vol_ = boost::make_shared<QuantExt::SwaptionVolCubeWithATM>(cube);
        }

    } catch (std::exception& e) {
        QL_FAIL("generic yield volatility curve building failed for curve " << config->curveID() << " on date "
                                                                            << io::iso_date(asof) << ": " << e.what());
    } catch (...) {
        QL_FAIL("generic yield vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
