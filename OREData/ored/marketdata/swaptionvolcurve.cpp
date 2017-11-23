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
#include <ored/marketdata/swaptionvolcurve.hpp>
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

SwaptionVolCurve::SwaptionVolCurve(Date asof, SwaptionVolatilityCurveSpec spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs,
                                   const map<string, boost::shared_ptr<SwapIndex>>& requiredSwapIndices) {

    try {

        const boost::shared_ptr<SwaptionVolatilityCurveConfig>& config =
            curveConfigs.swaptionVolCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config->dimension() == SwaptionVolatilityCurveConfig::Dimension::ATM ||
                       config->dimension() == SwaptionVolatilityCurveConfig::Dimension::Smile,
                   "Unsupported Swaption vol curve building dimension");

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole matrix or do not have more quotes in the market data

        MarketDatum::QuoteType volatilityType;
        switch (config->volatilityType()) {
        case SwaptionVolatilityCurveConfig::VolatilityType::Lognormal:
            volatilityType = MarketDatum::QuoteType::RATE_LNVOL;
            break;
        case SwaptionVolatilityCurveConfig::VolatilityType::Normal:
            volatilityType = MarketDatum::QuoteType::RATE_NVOL;
            break;
        case SwaptionVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
            volatilityType = MarketDatum::QuoteType::RATE_SLNVOL;
            break;
        default:
            QL_FAIL("unexpected volatility type");
        }
        bool isSln = volatilityType == MarketDatum::QuoteType::RATE_SLNVOL;
        vector<Period> optionTenors = config->optionTenors();
        vector<Period> swapTenors = config->swapTenors();
        Matrix vols(optionTenors.size(), swapTenors.size());
        Matrix shifts(isSln ? vols.rows() : 0, isSln ? vols.columns() : 0);
        vector<vector<bool>> found(optionTenors.size(), vector<bool>(swapTenors.size(), false));
        vector<bool> shiftFound(swapTenors.size());
        Size remainingQuotes = optionTenors.size() * swapTenors.size();
        Size remainingShiftQuotes = isSln ? swapTenors.size() : 0;
        Size quotesRead = 0, shiftQuotesRead = 0;

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::SWAPTION) {

                boost::shared_ptr<SwaptionQuote> q = boost::dynamic_pointer_cast<SwaptionQuote>(md);
                boost::shared_ptr<SwaptionShiftQuote> qs = boost::dynamic_pointer_cast<SwaptionShiftQuote>(md);

                if (remainingQuotes > 0 && q != NULL && q->ccy() == spec.ccy() && q->quoteType() == volatilityType &&
                    q->dimension() == "ATM") {

                    quotesRead++;

                    Size i = std::find(optionTenors.begin(), optionTenors.end(), q->expiry()) - optionTenors.begin();
                    Size j = std::find(swapTenors.begin(), swapTenors.end(), q->term()) - swapTenors.begin();

                    if (i < optionTenors.size() && j < swapTenors.size()) {
                        vols[i][j] = q->quote()->value();

                        if (!found[i][j]) {
                            found[i][j] = true;
                            if (--remainingQuotes == 0 && remainingShiftQuotes == 0)
                                break;
                        }
                    }
                }

                if (isSln && remainingShiftQuotes > 0 && qs != NULL && qs->ccy() == spec.ccy() &&
                    qs->quoteType() == MarketDatum::QuoteType::SHIFT) {

                    shiftQuotesRead++;

                    Size j = std::find(swapTenors.begin(), swapTenors.end(), qs->term()) - swapTenors.begin();

                    if (j < swapTenors.size()) {
                        for (Size i = 0; i < shifts.rows(); ++i)
                            shifts[i][j] = qs->quote()->value();

                        if (!shiftFound[j]) {
                            shiftFound[j] = true;
                            if (--remainingShiftQuotes == 0 && remainingQuotes == 0)
                                break;
                        }
                    }
                }
            }
        }

        LOG("SwaptionVolCurve: read " << quotesRead << " vols");

        // Check that we have all the data we need
        if (remainingQuotes != 0 || remainingShiftQuotes != 0) {
            ostringstream m, missingQuotes;
            m << "Not all quotes found for spec " << spec << endl;
            if (remainingQuotes != 0) {
                m << "Found vol data (*) and missing data (.):" << std::endl;
                for (Size i = 0; i < optionTenors.size(); ++i) {
                    for (Size j = 0; j < swapTenors.size(); ++j) {
                        if (found[i][j]) {
                            m << "*";
                        } else {
                            m << ".";
                            missingQuotes << "Missing vol: (" << optionTenors[i] << ", " << swapTenors[j] << ")"
                                          << endl;
                        }
                    }
                    if (i < optionTenors.size() - 1)
                        m << endl;
                }
            }
            if (remainingShiftQuotes != 0) {
                if (remainingQuotes != 0)
                    m << endl;
                m << "Found shift data (*) and missing data (.):" << std::endl;
                for (Size j = 0; j < swapTenors.size(); ++j) {
                    if (shiftFound[j]) {
                        m << "*";
                    } else {
                        m << ".";
                        missingQuotes << "Missing shifted vol: (" << swapTenors[j] << ")" << endl;
                    }
                }
                m << endl;
            }
            DLOGGERSTREAM << m.str() << endl << missingQuotes.str();
            QL_FAIL("could not build swaption vol curve");
        }

        boost::shared_ptr<SwaptionVolatilityStructure> atm;

        if (quotesRead != 1) {
            atm = boost::shared_ptr<SwaptionVolatilityStructure>(new SwaptionVolatilityMatrix(
                asof, config->calendar(), config->businessDayConvention(), optionTenors, swapTenors, vols,
                config->dayCounter(), config->flatExtrapolation(),
                config->volatilityType() == SwaptionVolatilityCurveConfig::VolatilityType::Normal
                    ? QuantLib::Normal
                    : QuantLib::ShiftedLognormal,
                shifts));

            atm->enableExtrapolation(config->extrapolate());
        } else {
            // Constant volatility
            atm = boost::shared_ptr<SwaptionVolatilityStructure>(new ConstantSwaptionVolatility(
                asof, config->calendar(), config->businessDayConvention(), vols[0][0], config->dayCounter(),
                config->volatilityType() == SwaptionVolatilityCurveConfig::VolatilityType::Normal
                    ? QuantLib::Normal
                    : QuantLib::ShiftedLognormal,
                !shifts.empty() ? shifts[0][0] : 0.0));
        }

        if (config->dimension() == SwaptionVolatilityCurveConfig::Dimension::ATM) {
            // Nothing more to do
            LOG("Returning ATM surface for config " << spec);
            vol_ = atm;
        } else {
            LOG("Building Cube for config " << spec);
            vector<Period> smileOptionTenors = config->smileOptionTenors();
            vector<Period> smileSwapTenors = config->smileSwapTenors();
            vector<Spread> spreads = config->smileSpreads();

            // add smile spread 0, if not already existent
            auto spr = std::upper_bound(spreads.begin(), spreads.end(), 0.0);
            if (!close_enough(*spr, 0.0)) {
                spreads.insert(spr, 0.0);
            }

            if (smileOptionTenors.size() == 0)
                smileOptionTenors = optionTenors;
            if (smileSwapTenors.size() == 0)
                smileSwapTenors = swapTenors;
            QL_REQUIRE(spreads.size() > 0, "Need at least 1 strike spread for a SwaptionVolCube");

            Size n = smileOptionTenors.size() * smileSwapTenors.size();
            vector<vector<Handle<Quote>>> volSpreadHandles(
                n, vector<Handle<Quote>>(spreads.size(), Handle<Quote>(boost::make_shared<SimpleQuote>(0.0))));

            LOG("vol cube smile option tenors " << smileOptionTenors.size());
            LOG("vol cube smile swap tenors " << smileSwapTenors.size());
            LOG("vol cube strike spreads " << spreads.size());

            Size spreadQuotesRead = 0;
            Size expectedSpreadQuotes = n * (spreads.size() - 1); // no quote for zero strike spread
            vector<vector<bool>> found(smileOptionTenors.size() * smileSwapTenors.size(),
                                       std::vector<bool>(spreads.size(), false)),
                zero(smileOptionTenors.size() * smileSwapTenors.size(), std::vector<bool>(spreads.size(), false));
            for (auto& md : loader.loadQuotes(asof)) {
                if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::SWAPTION) {

                    boost::shared_ptr<SwaptionQuote> q = boost::dynamic_pointer_cast<SwaptionQuote>(md);
                    if (q != NULL && q->ccy() == spec.ccy() && q->quoteType() == volatilityType &&
                        q->dimension() == "Smile") {

                        Size i = std::find(smileOptionTenors.begin(), smileOptionTenors.end(), q->expiry()) -
                                 smileOptionTenors.begin();
                        Size j = std::find(smileSwapTenors.begin(), smileSwapTenors.end(), q->term()) -
                                 smileSwapTenors.begin();
                        // In the MarketDatum we call it a strike, but it's really a spread
                        Size k = std::find(spreads.begin(), spreads.end(), q->strike()) - spreads.begin();

                        if (i < smileOptionTenors.size() && j < smileSwapTenors.size() && k < spreads.size()) {
                            QL_REQUIRE(!found[i * smileSwapTenors.size() + j][k],
                                       "VolSpreadQuote already found for " << smileOptionTenors[i] << ", "
                                                                           << smileSwapTenors[j] << ", " << spreads[k]);
                            spreadQuotesRead++;
                            // Assume quotes are absolute vols by strike so construct the vol spreads here
                            Volatility atmVol = atm->volatility(smileOptionTenors[i], smileSwapTenors[j], 0.0);
                            volSpreadHandles[i * smileSwapTenors.size() + j][k] = Handle<Quote>(
                                boost::make_shared<SimpleQuote>(q->quote()->value() - atmVol));
                            found[i * smileSwapTenors.size() + j][k] = true;
                            zero[i * smileSwapTenors.size() + j][k] = close_enough(q->quote()->value(), 0.0);
                        }
                    }
                }
            }
            LOG("Read " << spreadQuotesRead << " quotes for VolCube. Expected " << expectedSpreadQuotes);
            if (spreadQuotesRead < expectedSpreadQuotes) {
                for (Size i = 0; i < smileOptionTenors.size(); ++i) {
                    for (Size j = 0; j < smileSwapTenors.size(); ++j) {
                        for (Size k = 0; k < spreads.size(); ++k) {
                            if (!close_enough(spreads[k], 0.0) && !found[i * smileSwapTenors.size() + j][k]) {
                                WLOG("Missing market quote for " << smileOptionTenors[i] << "/" << smileSwapTenors[j]
                                                                 << "/" << spreads[k]);
                            }
                        }
                    }
                }
                QL_FAIL("Missing quotes for vol cube");
            }

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
                            WLOG("Overwrite vol spread for " << smileOptionTenors[i] << "/" << smileSwapTenors[j] << "/"
                                                             << spreads[spreads.size() - 1 - k] << " with "
                                                             << lastNonZeroValue << " since market quote is zero");
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
        QL_FAIL("swaption vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("swaption vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
