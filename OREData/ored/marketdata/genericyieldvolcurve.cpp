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

#include <ored/configuration/genericyieldvolcurveconfig.hpp>
#include <ored/configuration/reportconfig.hpp>
#include <ored/marketdata/genericyieldvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/models/carrmadanarbitragecheck.hpp>
#include <qle/termstructures/proxyswaptionvolatility.hpp>
#include <qle/termstructures/swaptionsabrcube.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

namespace {
Rate atmStrike(const Date& optionD, const Period& swapTenor, const QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase,
               const QuantLib::ext::shared_ptr<SwapIndex> shortSwapIndexBase) {
    if (swapTenor > shortSwapIndexBase->tenor()) {
        return swapIndexBase->clone(swapTenor)->fixing(swapIndexBase->fixingCalendar().adjust(optionD));
    } else {
        return shortSwapIndexBase->clone(swapTenor)->fixing(shortSwapIndexBase->fixingCalendar().adjust(optionD));
    }
}
} // namespace

GenericYieldVolCurve::GenericYieldVolCurve(
    const Date& asof, const Loader& loader, const CurveConfigurations& curveConfigs,
    const QuantLib::ext::shared_ptr<GenericYieldVolatilityCurveConfig>& config,
    const map<string, QuantLib::ext::shared_ptr<SwapIndex>>& requiredSwapIndices,
    const map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>>& requiredVolCurves,
    const std::function<bool(const QuantLib::ext::shared_ptr<MarketDatum>& md, Period& expiry, Period& term)>& matchAtmQuote,
    const std::function<bool(const QuantLib::ext::shared_ptr<MarketDatum>& md, Period& expiry, Period& term, Real& strike)>&
        matchSmileQuote,
    const std::function<bool(const QuantLib::ext::shared_ptr<MarketDatum>& md, Period& term)>& matchShiftQuote,
    const bool buildCalibrationInfo) {

    try {
        QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase;
        QuantLib::ext::shared_ptr<SwapIndex> shortSwapIndexBase;

        if (!config->proxySourceCurveId().empty()) {

            // Build proxy surface

            QL_REQUIRE(!config->proxySourceShortSwapIndexBase().empty(),
                       "GenericYieldVolCurve: proxy curve requires Source / ShortSwapIndexBase in the curve config.");
            QL_REQUIRE(!config->proxySourceSwapIndexBase().empty(),
                       "GenericYieldVolCurve: proxy curve requires Source / SwapIndexBase in the curve config.");
            QL_REQUIRE(!config->proxyTargetShortSwapIndexBase().empty(),
                       "GenericYieldVolCurve: proxy curve requires Target / ShortSwapIndexBase in the curve config.");
            QL_REQUIRE(!config->proxyTargetSwapIndexBase().empty(),
                       "GenericYieldVolCurve: proxy curve requires Target / SwapIndexBase in the curve config.");

            QuantLib::ext::shared_ptr<SwapIndex> sourceSwapIndexBase;
            QuantLib::ext::shared_ptr<SwapIndex> sourceShortSwapIndexBase;

            auto it = requiredSwapIndices.find(config->proxySourceShortSwapIndexBase());
            QL_REQUIRE(it != requiredSwapIndices.end(), "GenericYieldVolCurve: did not find swap index '"
                                                            << config->proxySourceShortSwapIndexBase()
                                                            << "' required for curve id '" << config->curveID() << "'");
            sourceShortSwapIndexBase = it->second;

            it = requiredSwapIndices.find(config->proxySourceSwapIndexBase());
            QL_REQUIRE(it != requiredSwapIndices.end(), "GenericYieldVolCurve: did not find swap index '"
                                                            << config->proxySourceSwapIndexBase()
                                                            << "' required for curve id '" << config->curveID() << "'");
            sourceSwapIndexBase = it->second;

            it = requiredSwapIndices.find(config->proxyTargetShortSwapIndexBase());
            QL_REQUIRE(it != requiredSwapIndices.end(), "GenericYieldVolCurve: did not find swap index '"
                                                            << config->proxyTargetShortSwapIndexBase()
                                                            << "' required for curve id '" << config->curveID() << "'");
            shortSwapIndexBase = it->second;

            it = requiredSwapIndices.find(config->proxyTargetSwapIndexBase());
            QL_REQUIRE(it != requiredSwapIndices.end(), "GenericYieldVolCurve: did not find swap index '"
                                                            << config->proxyTargetSwapIndexBase()
                                                            << "' required for curve id '" << config->curveID() << "'");
            swapIndexBase = it->second;

            auto it2 = requiredVolCurves.find(config->proxySourceCurveId());
            QL_REQUIRE(it2 != requiredVolCurves.end(), "GenericYieldVolCurve: did not find swaption vol curve '"
                                                           << config->proxySourceCurveId()
                                                           << "' required for curve id '" << config->curveID() << "'");

            vol_ = QuantLib::ext::make_shared<QuantExt::ProxySwaptionVolatility>(
                Handle<SwaptionVolatilityStructure>(it2->second->volTermStructure()), sourceSwapIndexBase,
                sourceShortSwapIndexBase, swapIndexBase, shortSwapIndexBase);

        } else {

            // Build quote based surface

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
                QuantLib::ext::shared_ptr<MarketDatum> md = loader.get(std::make_pair(p, true), asof);
                if (md == nullptr)
                    continue;
                Period expiry, term;
                if (md->quoteType() == volatilityType && matchAtmQuote(md, expiry, term)) {
                    quotesRead++;
                    Size i = std::find(optionTenors.begin(), optionTenors.end(), expiry) - optionTenors.begin();
                    Size j =
                        std::find(underlyingTenors.begin(), underlyingTenors.end(), term) - underlyingTenors.begin();
                    QL_REQUIRE(i < config->optionTenors().size(),
                               "expiry " << expiry << " not in configuration, this is unexpected");
                    QL_REQUIRE(j < config->underlyingTenors().size(),
                               "term " << term << " not in configuration, this is unexpected");
                    vols[i][j] = md->quote()->value();
                }
                if (isSln && md->quoteType() == MarketDatum::QuoteType::SHIFT && matchShiftQuote(md, term)) {
                    shiftQuotesRead++;
                    Size j =
                        std::find(underlyingTenors.begin(), underlyingTenors.end(), term) - underlyingTenors.begin();
                    QL_REQUIRE(j < config->underlyingTenors().size(),
                               "term " << term << " not in configuration, this is unexpected");
                    for (Size i = 0; i < shifts.rows(); ++i)
                        shifts[i][j] = md->quote()->value();
                }
            }

            LOG("GenericYieldVolCurve: read " << quotesRead << " vols and " << shiftQuotesRead << " shift quotes");

            // check we have found all requires values
            bool haveAllAtmValues = true;
            for (Size i = 0; i < config->optionTenors().size(); ++i) {
                for (Size j = 0; j < config->underlyingTenors().size(); ++j) {
                    if (vols[i][j] == Null<Real>()) {
                        ALOG("missing ATM vol for " << config->optionTenors()[i] << " / "
                                                    << config->underlyingTenors()[j]);
                        haveAllAtmValues = false;
                    }
                    if (isSln && shifts[i][j] == Null<Real>()) {
                        ALOG("missing shift for " << config->optionTenors()[i] << " / "
                                                  << config->underlyingTenors()[j]);
                        haveAllAtmValues = false;
                    }
                }
            }
            QL_REQUIRE(haveAllAtmValues, "Did not find all required quotes to build ATM surface");

            if (!config->swapIndexBase().empty()) {
                auto it = requiredSwapIndices.find(config->swapIndexBase());
                if (it != requiredSwapIndices.end())
                    swapIndexBase = it->second;
            }
            if (!config->shortSwapIndexBase().empty()) {
                auto it = requiredSwapIndices.find(config->shortSwapIndexBase());
                if (it != requiredSwapIndices.end())
                    shortSwapIndexBase = it->second;
            }

            QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atm;

            QL_REQUIRE(quotesRead > 0,
                       "GenericYieldVolCurve: did not read any quotes, are option and swap tenors defined?");
            if (quotesRead > 1) {
                atm = QuantLib::ext::shared_ptr<SwaptionVolatilityStructure>(new SwaptionVolatilityMatrix(
                    asof, config->calendar(), config->businessDayConvention(), optionTenors, underlyingTenors, vols,
                    config->dayCounter(),
                    config->extrapolation() == GenericYieldVolatilityCurveConfig::Extrapolation::Flat,
                    config->volatilityType() == GenericYieldVolatilityCurveConfig::VolatilityType::Normal
                        ? QuantLib::Normal
                        : QuantLib::ShiftedLognormal,
                    isSln ? shifts : Matrix(vols.rows(), vols.columns(), 0.0)));

                atm->enableExtrapolation(config->extrapolation() ==
                                         GenericYieldVolatilityCurveConfig::Extrapolation::Flat);
                TLOG("built atm surface with vols:");
                TLOGGERSTREAM(vols);
                if (isSln) {
                    TLOG("built atm surface with shifts:");
                    TLOGGERSTREAM(shifts);
                }
            } else {
                // Constant volatility
                atm = QuantLib::ext::shared_ptr<SwaptionVolatilityStructure>(new ConstantSwaptionVolatility(
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
                vector<Period> smileOptionTenors =
                    parseVectorOfValues<Period>(config->smileOptionTenors(), &parsePeriod);
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
                        j = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));

                LOG("vol cube smile option tenors " << smileOptionTenors.size());
                LOG("vol cube smile swap tenors " << smileUnderlyingTenors.size());
                LOG("vol cube strike spreads " << spreads.size());

                Size spreadQuotesRead = 0;
                for (auto& p : config->quotes()) {
                    // optional because we do not require all spreads
                    // we default them to zero instead and post process them below
                    QuantLib::ext::shared_ptr<MarketDatum> md = loader.get(std::make_pair(p, true), asof);
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
                        QL_REQUIRE(k < spreads.size(),
                                   "strike " << strike << " not in configuration, this is unexpected");

                        spreadQuotesRead++;
                        // Assume quotes are absolute vols by strike so construct the vol spreads here
                        Volatility atmVol = atm->volatility(smileOptionTenors[i], smileUnderlyingTenors[j], 0.0);
                        volSpreadHandles[i * smileUnderlyingTenors.size() + j][k] =
                            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(md->quote()->value() - atmVol));
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
                            QuantLib::ext::shared_ptr<SimpleQuote> q = QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(
                                *volSpreadHandles[i * smileUnderlyingTenors.size() + j][spreads.size() - 1 - k]);
                            QL_REQUIRE(q, "internal error: expected simple quote");
                            // do not overwrite vol spread for zero strike spread (ATM point)
                            if (zero[i * smileUnderlyingTenors.size() + j][spreads.size() - 1 - k] &&
                                !close_enough(spreads[spreads.size() - 1 - k], 0.0)) {
                                q->setValue(lastNonZeroValue);
                                DLOG("Overwrite vol spread for " << config->curveID() << "/" << smileOptionTenors[i]
                                                                 << "/" << smileUnderlyingTenors[j] << "/"
                                                                 << spreads[spreads.size() - 1 - k] << " with "
                                                                 << lastNonZeroValue << " since market quote is zero");
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

                // check we have swap indices
                QL_REQUIRE(swapIndexBase, "Unable to find SwapIndex " << config->swapIndexBase());
                QL_REQUIRE(shortSwapIndexBase, "Unable to find ShortSwapIndex " << config->shortSwapIndexBase());

                Handle<SwaptionVolatilityStructure> hATM(atm);
                QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityCube> cube;
                if (config->interpolation() == GenericYieldVolatilityCurveConfig::Interpolation::Linear) {
                    cube = QuantLib::ext::make_shared<QuantExt::SwaptionVolCube2>(
                        hATM, smileOptionTenors, smileUnderlyingTenors, spreads, volSpreadHandles, swapIndexBase,
                        shortSwapIndexBase, false,
                        config->extrapolation() == GenericYieldVolatilityCurveConfig::Extrapolation::Flat);
                    cube->enableExtrapolation();
                } else {
                    std::map<std::pair<QuantLib::Period, QuantLib::Period>, std::vector<std::pair<Real, bool>>>
                        initialModelParameters;
                    Size maxCalibrationAttempts = 10;
                    Real exitEarlyErrorThreshold = 0.005;
                    Real maxAcceptableError = 0.05;
                    if (config->parametricSmileConfiguration()) {
                        auto alpha = config->parametricSmileConfiguration()->parameter("alpha");
                        auto beta = config->parametricSmileConfiguration()->parameter("beta");
                        auto nu = config->parametricSmileConfiguration()->parameter("nu");
                        auto rho = config->parametricSmileConfiguration()->parameter("rho");
                        QL_REQUIRE(alpha.initialValue.size() == beta.initialValue.size() &&
                                       alpha.initialValue.size() == nu.initialValue.size() &&
                                       alpha.initialValue.size() == rho.initialValue.size(),
                                   "GenericYieldVolCurve: parametric smile config: alpha size ("
                                       << alpha.initialValue.size() << ") beta size (" << beta.initialValue.size()
                                       << ") nu size (" << nu.initialValue.size() << ") rho size ("
                                       << rho.initialValue.size() << ") must match");
                        QL_REQUIRE(alpha.initialValue.size() == 1 ||
                                       alpha.initialValue.size() == optionTenors.size() * underlyingTenors.size(),
                                   "GenericYieldVolCurve: parametric smile config: alpha, beta, nu, rho size ("
                                       << alpha.initialValue.size() << ") must match product of option tenors ("
                                       << optionTenors.size() << ") and swap tenors (" << underlyingTenors.size()
                                       << ") = " << optionTenors.size() * underlyingTenors.size() << ")");
                        for (Size i = 0; i < optionTenors.size(); ++i) {
                            for (Size j = 0; j < underlyingTenors.size(); ++j) {
                                std::vector<std::pair<Real, bool>> tmp;
                                Size idx = alpha.initialValue.size() == 1 ? 0 : i * underlyingTenors.size() + j;
                                tmp.push_back(std::make_pair(alpha.initialValue[idx], alpha.isFixed));
                                tmp.push_back(std::make_pair(beta.initialValue[idx], beta.isFixed));
                                tmp.push_back(std::make_pair(nu.initialValue[idx], nu.isFixed));
                                tmp.push_back(std::make_pair(rho.initialValue[idx], rho.isFixed));
                                initialModelParameters[std::make_pair(optionTenors[i], underlyingTenors[j])] = tmp;
                            }
                        }
                        maxCalibrationAttempts =
                            config->parametricSmileConfiguration()->calibration().maxCalibrationAttempts;
                        exitEarlyErrorThreshold =
                            config->parametricSmileConfiguration()->calibration().exitEarlyErrorThreshold;
                        maxAcceptableError = config->parametricSmileConfiguration()->calibration().maxAcceptableError;
                    }
                    cube = QuantLib::ext::make_shared<QuantExt::SwaptionSabrCube>(
                        hATM, smileOptionTenors, smileUnderlyingTenors, optionTenors, underlyingTenors, spreads,
                        volSpreadHandles, swapIndexBase, shortSwapIndexBase,
                        QuantExt::SabrParametricVolatility::ModelVariant(config->interpolation()),
                        config->outputVolatilityType() == GenericYieldVolatilityCurveConfig::VolatilityType::Normal
                            ? QuantLib::Normal
                            : QuantLib::ShiftedLognormal,
                        initialModelParameters, maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
                }

                // Wrap it in a SwaptionVolCubeWithATM
                vol_ = QuantLib::ext::make_shared<QuantExt::SwaptionVolCubeWithATM>(cube);
            }
        }

        // build calibration info

        if (buildCalibrationInfo) {

            DLOG("Building calibration info for generic yield vols");

            if (swapIndexBase == nullptr || shortSwapIndexBase == nullptr) {
                WLOG("no swap indexes given for " << config->curveID() << ", skip building calibraiton info");
                return;
            }

            ReportConfig rc = effectiveReportConfig(curveConfigs.reportConfigIrSwaptionVols(), config->reportConfig());

            bool reportOnStrikeGrid = *rc.reportOnStrikeGrid();
            bool reportOnStrikeSpreadGrid = *rc.reportOnStrikeSpreadGrid();
            std::vector<Real> strikes = *rc.strikes();
            std::vector<Real> strikeSpreads = *rc.strikeSpreads();
            std::vector<Period> expiries = *rc.expiries();
            std::vector<Period> underlyingTenorsReport = *rc.underlyingTenors();

            calibrationInfo_ = QuantLib::ext::make_shared<IrVolCalibrationInfo>();

            calibrationInfo_->dayCounter = config->dayCounter().empty() ? "na" : config->dayCounter().name();
            calibrationInfo_->calendar = config->calendar().empty() ? "na" : config->calendar().name();
            calibrationInfo_->volatilityType = ore::data::to_string(vol_->volatilityType());
            calibrationInfo_->underlyingTenors = underlyingTenorsReport;

            std::vector<Real> times;
            std::vector<std::vector<Real>> forwards;
            for (auto const& p : expiries) {
                Date d = vol_->optionDateFromTenor(p);
                calibrationInfo_->expiryDates.push_back(d);
                times.push_back(vol_->dayCounter().empty() ? Actual365Fixed().yearFraction(asof, d)
                                                           : vol_->timeFromReference(d));
                forwards.push_back(std::vector<Real>());
                for (auto const& u : underlyingTenorsReport) {
                    forwards.back().push_back(atmStrike(d, u, swapIndexBase, shortSwapIndexBase));
                }
            }

            calibrationInfo_->times = times;
            calibrationInfo_->forwards = forwards;

            std::vector<std::vector<std::vector<Real>>> callPricesStrike(
                times.size(),
                std::vector<std::vector<Real>>(underlyingTenorsReport.size(), std::vector<Real>(strikes.size(), 0.0)));
            std::vector<std::vector<std::vector<Real>>> callPricesStrikeSpread(
                times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                             std::vector<Real>(strikeSpreads.size(), 0.0)));

            calibrationInfo_->isArbitrageFree = true;

            if (reportOnStrikeGrid) {
                calibrationInfo_->strikes = strikes;
                calibrationInfo_->strikeGridStrikes = std::vector<std::vector<std::vector<Real>>>(
                    times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                                 std::vector<Real>(strikes.size(), 0.0)));
                calibrationInfo_->strikeGridProb = std::vector<std::vector<std::vector<Real>>>(
                    times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                                 std::vector<Real>(strikes.size(), 0.0)));
                calibrationInfo_->strikeGridImpliedVolatility = std::vector<std::vector<std::vector<Real>>>(
                    times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                                 std::vector<Real>(strikes.size(), 0.0)));
                calibrationInfo_->strikeGridCallSpreadArbitrage = std::vector<std::vector<std::vector<bool>>>(
                    times.size(), std::vector<std::vector<bool>>(underlyingTenorsReport.size(),
                                                                 std::vector<bool>(strikes.size(), true)));
                calibrationInfo_->strikeGridButterflyArbitrage = std::vector<std::vector<std::vector<bool>>>(
                    times.size(), std::vector<std::vector<bool>>(underlyingTenorsReport.size(),
                                                                 std::vector<bool>(strikes.size(), true)));
                TLOG("Strike cube arbitrage analysis result:");
                for (Size u = 0; u < underlyingTenorsReport.size(); ++u) {
                    TLOG("Underlying tenor " << underlyingTenorsReport[u]);
                    for (Size i = 0; i < times.size(); ++i) {
                        Real t = times[i];
                        Real shift = vol_->volatilityType() == Normal
                                         ? 0.0
                                         : vol_->shift(expiries[i], underlyingTenorsReport[u]);
                        bool validSlice = true;
                        for (Size j = 0; j < strikes.size(); ++j) {
                            try {
                                Real stddev = 0.0;
                                if (vol_->volatilityType() == ShiftedLognormal) {
                                    if ((strikes[j] > -shift || close_enough(strikes[j], -shift)) &&
                                        (forwards[i][u] > -shift || close_enough(forwards[i][u], -shift))) {
                                        stddev = std::sqrt(
                                            vol_->blackVariance(expiries[i], underlyingTenorsReport[u], strikes[j]));
                                        callPricesStrike[i][u][j] =
                                            blackFormula(Option::Type::Call, strikes[j], forwards[i][u], stddev);
                                    }
                                } else {
                                    stddev = std::sqrt(
                                        vol_->blackVariance(expiries[i], underlyingTenorsReport[u], strikes[j]));
                                    callPricesStrike[i][u][j] =
                                        bachelierBlackFormula(Option::Type::Call, strikes[j], forwards[i][u], stddev);
                                }
                                calibrationInfo_->strikeGridStrikes[i][u][j] = strikes[j];
                                calibrationInfo_->strikeGridImpliedVolatility[i][u][j] = stddev / std::sqrt(t);
                            } catch (const std::exception& e) {
                                validSlice = false;
                                TLOG("error for time " << t << " strike " << strikes[j] << ": " << e.what());
                            }
                        }
                        if (validSlice) {
                            try {
                                QuantExt::CarrMadanMarginalProbabilitySafeStrikes cm(
                                    calibrationInfo_->strikeGridStrikes[i][u], forwards[i][u], callPricesStrike[i][u],
                                    vol_->volatilityType(), shift);
                                calibrationInfo_->strikeGridCallSpreadArbitrage[i][u] = cm.callSpreadArbitrage();
                                calibrationInfo_->strikeGridButterflyArbitrage[i][u] = cm.butterflyArbitrage();
                                if (!cm.arbitrageFree())
                                    calibrationInfo_->isArbitrageFree = false;
                                calibrationInfo_->strikeGridProb[i][u] = cm.density();
                                TLOGGERSTREAM(arbitrageAsString(cm));
                            } catch (const std::exception& e) {
                                TLOG("error for time " << t << ": " << e.what());
                                calibrationInfo_->isArbitrageFree = false;
                                TLOGGERSTREAM("..(invalid slice)..");
                            }
                        } else {
                            TLOGGERSTREAM("..(invalid slice)..");
                        }
                    }
                }
                TLOG("Strike cube arbitrage analysis completed.");
            }

            if (reportOnStrikeSpreadGrid) {
                calibrationInfo_->strikeSpreads = strikeSpreads;
                calibrationInfo_->strikeSpreadGridStrikes = std::vector<std::vector<std::vector<Real>>>(
                    times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                                 std::vector<Real>(strikeSpreads.size(), 0.0)));
                calibrationInfo_->strikeSpreadGridProb = std::vector<std::vector<std::vector<Real>>>(
                    times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                                 std::vector<Real>(strikeSpreads.size(), 0.0)));
                calibrationInfo_->strikeSpreadGridImpliedVolatility = std::vector<std::vector<std::vector<Real>>>(
                    times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                                 std::vector<Real>(strikeSpreads.size(), 0.0)));
                calibrationInfo_->strikeSpreadGridCallSpreadArbitrage = std::vector<std::vector<std::vector<bool>>>(
                    times.size(), std::vector<std::vector<bool>>(underlyingTenorsReport.size(),
                                                                 std::vector<bool>(strikeSpreads.size(), true)));
                calibrationInfo_->strikeSpreadGridButterflyArbitrage = std::vector<std::vector<std::vector<bool>>>(
                    times.size(), std::vector<std::vector<bool>>(underlyingTenorsReport.size(),
                                                                 std::vector<bool>(strikeSpreads.size(), true)));
                TLOG("Strike Spread cube arbitrage analysis result:");
                for (Size u = 0; u < underlyingTenorsReport.size(); ++u) {
                    TLOG("Underlying tenor " << underlyingTenorsReport[u]);
                    for (Size i = 0; i < times.size(); ++i) {
                        Real t = times[i];
                        Real shift = vol_->volatilityType() == Normal
                                         ? 0.0
                                         : vol_->shift(expiries[i], underlyingTenorsReport[u]);
                        bool validSlice = true;
                        for (Size j = 0; j < strikeSpreads.size(); ++j) {
                            Real strike = forwards[i][u] + strikeSpreads[j];
                            try {
                                Real stddev = 0.0;
                                if (vol_->volatilityType() == ShiftedLognormal) {
                                    if ((strike > -shift || close_enough(strike, -shift)) &&
                                        (forwards[i][u] > -shift || close_enough(forwards[i][u], -shift))) {
                                        stddev = std::sqrt(
                                            vol_->blackVariance(expiries[i], underlyingTenorsReport[u], strike));
                                        callPricesStrikeSpread[i][u][j] =
                                            blackFormula(Option::Type::Call, strike, forwards[i][u], stddev);
                                    }
                                } else {
                                    stddev =
                                        std::sqrt(vol_->blackVariance(expiries[i], underlyingTenorsReport[u], strike));
                                    callPricesStrikeSpread[i][u][j] =
                                        bachelierBlackFormula(Option::Type::Call, strike, forwards[i][u], stddev);
                                }
                                calibrationInfo_->strikeSpreadGridStrikes[i][u][j] = strike;
                                calibrationInfo_->strikeSpreadGridImpliedVolatility[i][u][j] = stddev / std::sqrt(t);
                            } catch (const std::exception& e) {
                                validSlice = false;
                                TLOG("error for time " << t << " strike spread " << strikeSpreads[j] << " strike "
                                                       << strike << ": " << e.what());
                            }
                        }
                        if (validSlice) {
                            try {
                                QuantExt::CarrMadanMarginalProbabilitySafeStrikes cm(
                                    calibrationInfo_->strikeSpreadGridStrikes[i][u], forwards[i][u],
                                    callPricesStrikeSpread[i][u], vol_->volatilityType(), shift);
                                calibrationInfo_->strikeSpreadGridCallSpreadArbitrage[i][u] = cm.callSpreadArbitrage();
                                calibrationInfo_->strikeSpreadGridButterflyArbitrage[i][u] = cm.butterflyArbitrage();
                                if (!cm.arbitrageFree())
                                    calibrationInfo_->isArbitrageFree = false;
                                calibrationInfo_->strikeSpreadGridProb[i][u] = cm.density();
                                TLOGGERSTREAM(arbitrageAsString(cm));
                            } catch (const std::exception& e) {
                                TLOG("error for time " << t << ": " << e.what());
                                calibrationInfo_->isArbitrageFree = false;
                                TLOGGERSTREAM("..(invalid slice)..");
                            }
                        } else {
                            TLOGGERSTREAM("..(invalid slice)..");
                        }
                    }
                }
                TLOG("Strike Spread cube arbitrage analysis completed.");
            }

            DLOG("Building calibration info generic yield vols completed.");

            // output SABR calibration to log, if SABR was used

            if (auto sw = QuantLib::ext::dynamic_pointer_cast<QuantExt::SwaptionVolCubeWithATM>(vol_)) {
                if (auto sabr = QuantLib::ext::dynamic_pointer_cast<QuantExt::SwaptionSabrCube>(sw->cube())) {
                    if (auto p = QuantLib::ext::dynamic_pointer_cast<QuantExt::SabrParametricVolatility>(
                            sabr->parametricVolatility())) {
                        DLOG("SABR parameters:");
                        DLOG("alpha (rows = option tenors, cols = underlying lengths):");
                        DLOGGERSTREAM(transpose(p->alpha()));
                        DLOG("beta (rows = option tenors, cols = underlying lengths):");
                        DLOGGERSTREAM(transpose(p->beta()));
                        DLOG("nu (rows = option tenors, cols = underlying lengths):");
                        DLOGGERSTREAM(transpose(p->nu()));
                        DLOG("rho (rows = option tenors, cols = underlying lengths):");
                        DLOGGERSTREAM(transpose(p->rho()));
                        DLOG("lognormal shift (rows = option tenors, cols = underlying lengths):");
                        DLOGGERSTREAM(transpose(p->lognormalShift()));
                        DLOG("calibration attempts (rows = option tenors, cols = underlying lengths):");
                        DLOGGERSTREAM(transpose(p->numberOfCalibrationAttempts()));
                        DLOG("calibration error (rows = option tenors, cols = underlying lengths, rmse of relative "
                             "errors w.r.t. max of sabr variant's preferred quotation type, i.e. nvol, slnvol, "
                             "premium:");
                        DLOGGERSTREAM(transpose(p->calibrationError()));
                        DLOG("isInterpolated (rows = option tenors, cols = underlying lengths, 1 means calibration "
                             "failed and point is interpolated):");
                        DLOGGERSTREAM(transpose(p->isInterpolated()));
                    }
                }
            }
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
