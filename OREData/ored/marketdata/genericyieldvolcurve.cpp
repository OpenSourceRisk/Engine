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
#include <ored/utilities/parsers.hpp>
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
        Matrix vols(config->optionTenors().size(), config->underlyingTenors().size(), Null<Real>());
        Matrix shifts(isSln ? vols.rows() : 0, isSln ? vols.columns() : 0, Null<Real>());
        Size quotesRead = 0, shiftQuotesRead = 0;
        vector<Period> optionTenors = parseVectorOfValues<Period>(config->optionTenors(), &parsePeriod);
        vector<Period> underlyingTenors = parseVectorOfValues<Period>(config->underlyingTenors(), &parsePeriod);

        for (auto& p : config->quotes()) {
            // optional, because we do not require all spread quotes; we check below that we have all atm quotes
            boost::shared_ptr<MarketDatum> md = loader.get(std::make_pair(p, true), asof);
            if (md == nullptr)
                continue;
            Period expiry, term;
            if (md->quoteType() == volatilityType && matchAtmQuote(md, expiry, term)) {
                quotesRead++;
                Size i = std::find(optionTenors.begin(), optionTenors.end(), expiry) - optionTenors.begin();
                Size j = std::find(underlyingTenors.begin(), underlyingTenors.end(), term) - underlyingTenors.begin();
                QL_REQUIRE(i < config->optionTenors().size(),
                           "expiry " << expiry << " not in configuration, this is unexpected");
                QL_REQUIRE(j < config->underlyingTenors().size(),
                           "term " << term << " not in configuration, this is unexpected");
                vols[i][j] = md->quote()->value();
            }
            if (isSln && md->quoteType() == MarketDatum::QuoteType::SHIFT && matchShiftQuote(md, term)) {
                shiftQuotesRead++;
                Size j = std::find(underlyingTenors.begin(), underlyingTenors.end(), term) - underlyingTenors.begin();
                QL_REQUIRE(j < config->underlyingTenors().size(),
                           "term " << term << " not in configuration, this is unexpected");
                for (Size i = 0; i < shifts.rows(); ++i)
                    shifts[i][j] = md->quote()->value();
            }
        }

        LOG("GenericYieldVolCurve: read " << quotesRead << " vols");

        // check we have found all requires values
        bool haveAllAtmValues = true;
        for (Size i = 0; i < config->optionTenors().size(); ++i) {
            for (Size j = 0; j < config->underlyingTenors().size(); ++j) {
                if (vols[i][j] == Null<Real>()) {
                    ALOG("missing ATM vol for " << config->optionTenors()[i] << " / " << config->underlyingTenors()[j]);
                    haveAllAtmValues = false;
                }
                if (isSln && shifts[i][j] == Null<Real>()) {
                    ALOG("missing shift for " << config->optionTenors()[i] << " / " << config->underlyingTenors()[j]);
                    haveAllAtmValues = false;
                }
            }
        }
        QL_REQUIRE(haveAllAtmValues, "Did not find all required quotes to build ATM surface");

        boost::shared_ptr<SwaptionVolatilityStructure> atm;

        QL_REQUIRE(quotesRead > 0,
                   "GenericYieldVolCurve: did not read any quotes, are option and swap tenors defined?");
        if (quotesRead > 1) {
            atm = boost::shared_ptr<SwaptionVolatilityStructure>(new SwaptionVolatilityMatrix(
                asof, config->calendar(), config->businessDayConvention(), optionTenors, underlyingTenors, vols,
                config->dayCounter(), config->flatExtrapolation(),
                config->volatilityType() == GenericYieldVolatilityCurveConfig::VolatilityType::Normal
                    ? QuantLib::Normal
                    : QuantLib::ShiftedLognormal,
                isSln ? shifts : Matrix(vols.rows(), vols.columns(), 0.0)));

            atm->enableExtrapolation(config->extrapolate());
            TLOG("built atm surface with vols:");
            TLOGGERSTREAM << vols;
            if(isSln) {
                TLOG("built atm surface with shifts:");
                TLOGGERSTREAM << shifts;
            }
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
            vector<Period> smileOptionTenors = parseVectorOfValues<Period>(config->smileOptionTenors(), &parsePeriod);
            vector<Period> smileUnderlyingTenors =
                parseVectorOfValues<Period>(config->smileUnderlyingTenors(), &parsePeriod);
            vector<Spread> spreads = parseVectorOfValues<Real>(config->smileSpreads(), &parseReal);

            // add smile spread 0, if not already existent and sort the spreads
            if (std::find_if(spreads.begin(), spreads.end(), [](const Real x) { return close_enough(x, 0.0); }) ==
                spreads.end())
                spreads.push_back(0.0);
            std::sort(spreads.begin(), spreads.end());

            vector<vector<bool>> zero(smileOptionTenors.size() * smileUnderlyingTenors.size(),
                                      std::vector<bool>(spreads.size(), true));

            if (smileOptionTenors.size() == 0)
                smileOptionTenors = parseVectorOfValues<Period>(config->optionTenors(), &parsePeriod);
            if (smileUnderlyingTenors.size() == 0)
                smileUnderlyingTenors = parseVectorOfValues<Period>(config->underlyingTenors(), &parsePeriod);
            QL_REQUIRE(spreads.size() > 0, "Need at least 1 strike spread for a SwaptionVolCube");

            Size n = smileOptionTenors.size() * smileUnderlyingTenors.size();
            vector<vector<Handle<Quote>>> volSpreadHandles(n, vector<Handle<Quote>>(spreads.size()));
            for (auto& i : volSpreadHandles)
                for (auto& j : i)
                    j = Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));

            LOG("vol cube smile option tenors " << smileOptionTenors.size());
            LOG("vol cube smile swap tenors " << smileUnderlyingTenors.size());
            LOG("vol cube strike spreads " << spreads.size());

            Size spreadQuotesRead = 0;
            for (auto& p : config->quotes()) {
                // optional because we do not require all spreads
                // we default them to zero instead and post process them below
                boost::shared_ptr<MarketDatum> md = loader.get(std::make_pair(p, true), asof);
                if (md == nullptr)
                    continue;
                Period expiry, term;
                Real strike;
                if (md->quoteType() == volatilityType && matchSmileQuote(md, expiry, term, strike)) {

                    Size i = std::find(smileOptionTenors.begin(), smileOptionTenors.end(), expiry) -
                             smileOptionTenors.begin();
                    Size j = std::find(smileUnderlyingTenors.begin(), smileUnderlyingTenors.end(), term) -
                             smileUnderlyingTenors.begin();
                    // In the MarketDatum we call it a strike, but it's really a spread
                    Size k = std::find(spreads.begin(), spreads.end(), strike) - spreads.begin();
                    QL_REQUIRE(i < smileOptionTenors.size(),
                               "expiry " << expiry << " not in configuration, this is unexpected");
                    QL_REQUIRE(j < smileUnderlyingTenors.size(),
                               "term " << term << " not in configuration, this is unexpected");
                    QL_REQUIRE(k < spreads.size(), "strike " << strike << " not in configuration, this is unexpected");

                    spreadQuotesRead++;
                    // Assume quotes are absolute vols by strike so construct the vol spreads here
                    Volatility atmVol = atm->volatility(smileOptionTenors[i], smileUnderlyingTenors[j], 0.0);
                    volSpreadHandles[i * smileUnderlyingTenors.size() + j][k] =
                        Handle<Quote>(boost::make_shared<SimpleQuote>(md->quote()->value() - atmVol));
                    zero[i * smileUnderlyingTenors.size() + j][k] = close_enough(md->quote()->value(), 0.0);
                }
            }
            LOG("Read " << spreadQuotesRead << " quotes for VolCube.");

            // post processing: extrapolate leftmost non-zero value flat to the left and overwrite
            // zero values
            for (Size i = 0; i < smileOptionTenors.size(); ++i) {
                for (Size j = 0; j < smileUnderlyingTenors.size(); ++j) {
                    Real lastNonZeroValue = 0.0;
                    for (Size k = 0; k < spreads.size(); ++k) {
                        boost::shared_ptr<SimpleQuote> q = boost::dynamic_pointer_cast<SimpleQuote>(
                            *volSpreadHandles[i * smileUnderlyingTenors.size() + j][spreads.size() - 1 - k]);
                        QL_REQUIRE(q, "internal error: expected simple quote");
                        // do not overwrite vol spread for zero strike spread (ATM point)
                        if (zero[i * smileUnderlyingTenors.size() + j][spreads.size() - 1 - k] &&
                            !close_enough(spreads[spreads.size() - 1 - k], 0.0)) {
                            q->setValue(lastNonZeroValue);
                            WLOG("Overwrite vol spread for "
                                 << config->curveID() << "/" << smileOptionTenors[i] << "/" << smileUnderlyingTenors[j]
                                 << "/" << spreads[spreads.size() - 1 - k] << " with " << lastNonZeroValue
                                 << " since market quote is zero");
                        }
                        // update last non-zero value
                        if (!zero[i * smileUnderlyingTenors.size() + j][spreads.size() - 1 - k]) {
                            lastNonZeroValue = q->value();
                        }
                    }
                }
            }

            // log vols
            for (Size i = 0; i < smileOptionTenors.size(); ++i) {
                for (Size j = 0; j < smileUnderlyingTenors.size(); ++j) {
                    ostringstream o;
                    for (Size k = 0; k < spreads.size(); ++k) {
                        o << volSpreadHandles[i * smileUnderlyingTenors.size() + j][k]->value() +
                                 atm->volatility(smileOptionTenors[i], smileUnderlyingTenors[j], 0.0)
                          << " ";
                    }
                    DLOG("Vols for " << smileOptionTenors[i] << "/" << smileUnderlyingTenors[j] << ": " << o.str());
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
                hATM, smileOptionTenors, smileUnderlyingTenors, spreads, volSpreadHandles, swapIndexBase,
                shortSwapIndexBase, vegaWeighedSmileFit, config->flatExtrapolation());
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
