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

#include <orea/engine/simmsensitivitystoragemanager.hpp>

#include <orea/app/structuredanalyticserror.hpp>

#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/currencies/currencycomparator.hpp>
#include <qle/instruments/currencyswap.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace ore {
namespace analytics {

SimmSensitivityStorageManager::SimmSensitivityStorageManager(const std::vector<std::string>& currencies,
                                                             const QuantLib::Size firstCubeIndexToUse)
    : currencies_(currencies), firstCubeIndexToUse_(firstCubeIndexToUse) {

    QL_REQUIRE(!currencies_.empty(), "SimmSensitivityStorageManager: currencies are empty");

    nCurveTenors_ = irDeltaTerms_.size();
    nSwaptionExpiries_ = irVegaTerms_.size();
    nSwaptionTerms_ = irVegaUnderlyingTerms_.size();
    nFxExpiries_ = fxVegaTerms_.size();

    DayCounter dc = ActualActual(ActualActual::ISDA);
    Date asof = Settings::instance().evaluationDate();
    for (auto p: irVegaTerms_)
        swaptionExpiryTimes_.push_back(dc.yearFraction(asof, asof + p));
    for (auto p: irVegaUnderlyingTerms_)
        swaptionTermTimes_.push_back(dc.yearFraction(asof, asof + p));
    for (auto p: fxVegaTerms_)
        fxExpiryTimes_.push_back(dc.yearFraction(asof, asof + p));

    // IR Deltas: curve sensis across all currencies
    nc_ = nCurveTenors_ * currencies_.size();
    // FX Deltas: fxspot sensis
    nx_ = currencies_.size() - 1;
    // IR Vegas: Swaption vega risk vector for all currencies
    nco_ = nSwaptionExpiries_ * currencies_.size();
    // FX Vegas: FX Option vega vector, ignoring strike
    nxo_ = nFxExpiries_ * nx_;

    // Storage vector size for deltas, vegas and theta
    N_ = nc_ + nx_ + nco_ + nxo_ + 1;

    LOG("SimmSensitivityStorageManager created with depth " << N_ << " nc=" << nc_ << " nx=" << nx_ << " nco=" << nco_
                                                            << " nxo=" << nxo_);
}

Size SimmSensitivityStorageManager::getRequiredSize() const {
    return N_;
}

namespace {

Size getNettingSetIndex(const std::string& nettingSetId, const QuantLib::ext::shared_ptr<NPVCube>& cube) {
    QL_REQUIRE(cube, "SensitivityCalculator::calculate(): no cube given to store sensitivity");
    auto nid = cube->idsAndIndexes().find(nettingSetId);
    QL_REQUIRE(nid != cube->idsAndIndexes().end(),
               "SensitivityCalculator::calculate(): did not find nettingSetId '" << nettingSetId << "' in cube");
    return nid->second;
}

} // namespace

void SimmSensitivityStorageManager::addSensitivities(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
						     const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
						     const QuantLib::ext::shared_ptr<Market>& market, const Size dateIndex,
						     const Size sampleIndex) const {

    DLOG("SimmSensitivityStorageManager::addSensitivities called for date " << dateIndex << ", sample " << sampleIndex);
  
    QL_REQUIRE((dateIndex == Null<Size>() && sampleIndex == Null<Size>()) ||
                   (dateIndex != Null<Size>() && sampleIndex != Null<Size>()),
               "SimmSensitivityStorageManager::addSensitivities(): date and sample index must be both null (write to T0 "
               "slice) or both not null");

    QL_REQUIRE(cube, "SimmSensitivityStorageManager::addSensitivites(): no cube to store results given");

    try {

        // get results we want to store in the cube, i.e. delta, vega and theta

        // IR and FX delta in one array
        Array delta(nc_ + nx_, 0.0);
	
	Real theta = 0;

	// We have Swaption Vega matrices for each currency
        vector<Array> swaptionVega(currencies_.size(), Array(nSwaptionExpiries_, 0.0));

	// we have fxVega vectors for each currency pair
	vector<Array> fxVega(nx_, Array(nFxExpiries_, 0.0));

        if (trade->tradeType() == "Swap" || trade->tradeType() == "Swaption") {
            processSwapSwaption(delta, swaptionVega, theta, trade, market);
        } else if (trade->tradeType() == "FxOption") {
            processFxOption(delta, fxVega, theta, trade, market);
        } else if (trade->tradeType() == "FxForward") {
            processFxForward(delta, theta, trade, market);
        } else {
            QL_FAIL("trade type '" << trade->tradeType() << "' not supported");
        }

        // serialise the results into a vector that we can write to the cube

        std::vector<Real> cubeData;
	Size deltaCount = 0;
        for (auto const& d : delta) {
            QL_REQUIRE(std::isfinite(d), "delta not finite: " << d);
            cubeData.push_back(d);
	    deltaCount ++;
        }

	DLOG("SimmSensitivityStorageManager::addSensitivities: delta " << deltaCount << " " << nc_ + nx_);
	
        QL_REQUIRE(std::isfinite(theta), "theta not finite: " << theta);
        cubeData.push_back(theta);

	Size irVegaCount = 0;
        for (Size k = 0; k < currencies_.size(); ++k) {
            for (auto const& d : swaptionVega[k]) {
                QL_REQUIRE(std::isfinite(d), "swaption vega risk not finite: " << d);
                cubeData.push_back(d);
		irVegaCount++;
	    }
        }

	DLOG("SimmSensitivityStorageManager::addSensitivities: irVega " << irVegaCount << " " << nco_);

	Size fxVegaCount = 0;
        for (Size k = 0; k < currencies_.size() - 1; ++k) {
            for (auto const& d : fxVega[k]) {
                QL_REQUIRE(std::isfinite(d), "fxVega not finite: " << d);
                cubeData.push_back(d);
		fxVegaCount++;
            }
        }

	DLOG("SimmSensitivityStorageManager::addSensitivities: fxVega " << fxVegaCount << " " << nxo_);

        // write the serialised data to the cube

        Size idx = firstCubeIndexToUse_;
        Size nettingSetIndex = getNettingSetIndex(trade->envelope().nettingSetId(), cube);

        for (auto const& d : cubeData) {
            if (idx < cube->depth()) {
                if (dateIndex == Null<Size>()) {
                    Real tmp = cube->getT0(nettingSetIndex, idx);
                    cube->setT0(tmp + d, nettingSetIndex, idx);
                } else {
                    Real tmp = cube->get(nettingSetIndex, dateIndex, sampleIndex, idx);
                    cube->set(tmp + d, nettingSetIndex, dateIndex, sampleIndex, idx);
                }
            } else {
	        ALOG("Skip writing sensitivity for index " << idx << " >= cube depth, N = " << N_);
            }
            ++idx;
        }

        DLOG("SimmSensitivityStorageManager: cubeData size = " <<  cubeData.size() << ", " << N_);
	
    } catch (std::exception& e) {
        auto subFields = map<string, string>({{"tradeId", trade->id()}, {"tradeType", trade->tradeType()}});
        StructuredAnalyticsErrorMessage(
            "Dynamic Sensitivity Calculation", "SimmSensitivityStorageManager::addSensitivities()",
            "Failed to get sensitivities for trade: " + to_string(e.what()) + " - not adding sensitivities to cube.",
            subFields)
            .log();
    }

    TLOG("SimmSensitivityStorageManager: Added sensitivities to cube for trade="
	<< trade->id() << " sample=" << sampleIndex << " date=" << dateIndex);
}

boost::any SimmSensitivityStorageManager::getSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube,
							   const std::string& nettingSetId, const Size dateIndex,
							   const Size sampleIndex) const {

    DLOG("SimmSensitivityStorageManager::getSensitivities called for date " << dateIndex << ", sample " << sampleIndex);

    QL_REQUIRE((dateIndex == Null<Size>() && sampleIndex == Null<Size>()) ||
                   (dateIndex != Null<Size>() && sampleIndex != Null<Size>()),
               "SimmSensitivityStorageManager::getSensitivities(): date and sample index must be both null (write to T0 "
               "slice) or both not null");

    QL_REQUIRE(cube, "SimmSensitivityStorageManager::getSensitivites(): no cube to retrieve results from");

    DLOG("SimmSensitivityStorageManager::getSensitivities create delta and vega structures");

    Array delta(nc_ + nx_, 0.0);
    Real theta;
    vector<Array> swaptionVega(currencies_.size(), Array(nSwaptionExpiries_, 0.0));
    vector<Array> fxVega(nx_, Array(nFxExpiries_, 0.0));
    
    // get data from cube

    DLOG("SimmSensitivityStorageManager::getSensitivities get cubeData");

    std::vector<Real> cubeData;
    Size nettingSetIndex = getNettingSetIndex(nettingSetId, cube);
    Size idx = firstCubeIndexToUse_;
    for (Size i = 0; i < getRequiredSize(); ++i) {
        Real tmp;
        if (dateIndex == Null<Size>()) {
            tmp = cube->getT0(nettingSetIndex, idx);
        } else {
            tmp = cube->get(nettingSetIndex, dateIndex, sampleIndex, idx);
        }
        ++idx;
        cubeData.push_back(tmp);
    }

    // deserialise data from cube into deltas, vegas and theta

    DLOG("SimmSensitivityStorageManager::getSensitivities deserialise delta");

    for (Size i = 0; i < nc_ + nx_; ++i) {
        delta[i] = cubeData[i];
    }

    DLOG("SimmSensitivityStorageManager::getSensitivities deserialise theta");

    theta = cubeData[nc_ + nx_];

    DLOG("SimmSensitivityStorageManager::getSensitivities deserialise irVega");

    idx = nc_ + nx_ + 1;
    for (Size k = 0; k < currencies_.size(); ++k) {
        for (Size i = 0; i < swaptionVega[k].size(); ++i) {
            swaptionVega[k][i] = cubeData[idx];
            ++idx;
        }
    }

    DLOG("SimmSensitivityStorageManager::getSensitivities deserialise fxVega");

    for (Size k = 0; k < currencies_.size() - 1; ++k) {
        for (Size i = 0; i < fxVega[k].size(); ++i) {
            fxVega[k][i] = cubeData[idx];
            ++idx;
        }
    }

    TLOG("SimmSensitivityStorageManager: Got sensitivities from cube for nettingSet="
	<< nettingSetId << " sample=" << sampleIndex << " date=" << dateIndex);

    return std::make_tuple(delta, swaptionVega, fxVega, theta);
}

Size SimmSensitivityStorageManager::getCcyIndex(const std::string& ccy) const {
    auto c = std::find(currencies_.begin(), currencies_.end(), ccy);
    QL_REQUIRE(c != currencies_.end(),
               "SimmSensitivityStorageManager::getCcyIndex(): ccy '" << ccy << "' not found");
    return std::distance(currencies_.begin(), c);
}

void SimmSensitivityStorageManager::processSwapSwaption(Array& delta, vector<Array>& vega, QuantLib::Real& theta,
							const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
							const QuantLib::ext::shared_ptr<Market>& market) const {

    Date asof = Settings::instance().evaluationDate();
    DayCounter dc = Actual365Fixed();
    
    // just for convenience
    const Size& n = nCurveTenors_;
    const Size c = currencies_.size();
    const std::string& baseCcyCode = currencies_[0];

    // if the trade has no legs, we are done
    std::vector<std::string> currencies = trade->legCurrencies();
    if (currencies.empty())
        return;
    
    // get ql instrument and trade multiplier (might be overwritten below for trade wrappers)
    QuantLib::ext::shared_ptr<Instrument> qlInstr = trade->instrument()->qlInstrument();
    Real tradeMultiplier = trade->instrument()->multiplier();

    // do we have an xccy swap?
    bool isXCCY = false;
    for (Size k = 1; k < currencies.size(); ++k)
        isXCCY = isXCCY || (currencies[k] != currencies[0]);

    if (!isXCCY) {

        // determine relevant ql instrument and trade multiplier in case we have a swaption
        bool hasThetaVega = false;
        if (trade->tradeType() == "Swaption") {
            QuantLib::ext::shared_ptr<OptionWrapper> wrapper = QuantLib::ext::dynamic_pointer_cast<OptionWrapper>(trade->instrument());
            if (wrapper) { // option wrapper (i.e. we have a swaption)
                if (wrapper->isExercised()) {
                    if (wrapper->isPhysicalDelivery()) {
                        qlInstr = wrapper->activeUnderlyingInstrument();
                        tradeMultiplier = wrapper->underlyingMultiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                        hasThetaVega = false;
                    } else {
                        qlInstr = wrapper->qlInstrument();
                        tradeMultiplier = wrapper->multiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                        hasThetaVega = false;
                    }
                } else {
                    qlInstr = wrapper->qlInstrument();
                    tradeMultiplier = wrapper->multiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                    hasThetaVega = true;
                }
            } else { // not an option wrapper
                qlInstr = trade->instrument()->qlInstrument();
                tradeMultiplier = trade->instrument()->multiplier();
                hasThetaVega = true;
            }
        }

        // handle expired instruments
        if (qlInstr->isExpired())
	    return;

        // single currency swap or european swaption
        Size ccyIndex = getCcyIndex(currencies[0]);
        Real fx = market->fxRate(currencies[0] + baseCcyCode)->value();
        if (ccyIndex != 0) {
            Real fxDelta = currencies[0] != baseCcyCode ? tradeMultiplier * qlInstr->NPV() : 0.0;
            // log(fx) delta, i.e. multiply by fx
            delta[n * c + ccyIndex - 1] += fxDelta * fx;
        }
        for (Size ii = 0; ii < n; ++ii) {
            // aggregate discount and forward curve deltas
            std::vector<Real> deltaDiscount = qlInstr->result<std::vector<Real>>("deltaDiscount");
            std::vector<Real> deltaForward = qlInstr->result<std::vector<Real>>("deltaForward");
            delta[ccyIndex * n + ii] += (deltaDiscount[ii] + deltaForward[ii]) * tradeMultiplier * fx;
        }
        // Theta and Vega
        if (hasThetaVega) {
	  
	    theta = qlInstr->result<Real>("theta") * tradeMultiplier * fx;

	    Real singleVega = qlInstr->result<Real>("singleVega") * tradeMultiplier * fx;
	    Real atmVol = qlInstr->result<Real>("atmVol");
	    Real singleVegaRisk = singleVega * atmVol;
	    Date exerciseDate = qlInstr->result<Date>("exerciseDate");
	    // Date maturityDate = trade->maturity();
	    map<Size,Real> sv = bucketMapping(singleVegaRisk, exerciseDate, swaptionExpiryTimes_, asof, dc);	    
	    // Add the contributions
            Size idx = getCcyIndex(currencies.front());
            QL_REQUIRE(vega.size() > idx, "currency " << currencies.front() << " not found in vega matrtices");
            for (auto it : sv) {
                LOG("map swaption single vega " << singleVegaRisk << " " << currencies.front() << " for trade "
                                                << trade->id() << " to vega array for ccy " << idx << ", row " << it.first
                                                << ": " << it.second);
                vega[idx][it.first] += it.second;
            }
        }
    } else {
        // cross currency swap

        // handle expired instrument
        if (qlInstr->isExpired())
	    return;

        std::vector<string> distinctCurrs(currencies);
        std::sort(distinctCurrs.begin(), distinctCurrs.end());
        auto end = std::unique(distinctCurrs.begin(), distinctCurrs.end());
        distinctCurrs.resize(end - distinctCurrs.begin());
        QL_REQUIRE(distinctCurrs.size() == 2,
                   "expected 2 currencies for cross currency swap, got " << distinctCurrs.size());
        Currency ccy1 = parseCurrency(distinctCurrs[0]);
        Currency ccy2 = parseCurrency(distinctCurrs[1]);
        Size ccyIndex1 = getCcyIndex(distinctCurrs[0]);
        Size ccyIndex2 = getCcyIndex(distinctCurrs[1]);
        Real fx1 = market->fxRate(distinctCurrs[0] + baseCcyCode)->value();
        Real fx2 = market->fxRate(distinctCurrs[1] + baseCcyCode)->value();
        QuantLib::ext::shared_ptr<CurrencySwap> instr =
            QuantLib::ext::dynamic_pointer_cast<CurrencySwap>(trade->instrument()->qlInstrument());
        QL_REQUIRE(instr, "Cross Currency Swap: Expected QL instrument CurrencySwap");
        auto deltaFxSpot = instr->result<result_type_scalar>("deltaFxSpot");
        for (auto const& fxd : deltaFxSpot) {
            if (fxd.first == ccy1)
                delta[n * c + ccyIndex1 - 1] += fxd.second * fx1;
            else if (fxd.first == ccy2)
                delta[n * c + ccyIndex2 - 1] += fxd.second * fx2;
            else {
                QL_FAIL("unexpected ccy " << fxd.first);
            }
        }
        std::vector<Real> deltaDiscount1 =
            trade->instrument()->qlInstrument()->result<result_type_vector>("deltaDiscount")[ccy1];
        std::vector<Real> deltaForward1 = instr->result<result_type_vector>("deltaForward")[ccy1];
        std::vector<Real> deltaDiscount2 = instr->result<result_type_vector>("deltaDiscount")[ccy2];
        std::vector<Real> deltaForward2 = instr->result<result_type_vector>("deltaForward")[ccy2];
        for (Size ii = 0; ii < n; ++ii) {
            delta[ccyIndex1 * n + ii] += (deltaDiscount1[ii] + deltaForward1[ii]) * tradeMultiplier * fx1;
            delta[ccyIndex2 * n + ii] += (deltaDiscount2[ii] + deltaForward2[ii]) * tradeMultiplier * fx2;
        }
    }
}

void SimmSensitivityStorageManager::processFxOption(Array& delta, vector<Array>& vega, QuantLib::Real& theta,
						    const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
						    const QuantLib::ext::shared_ptr<Market>& market) const {
    DLOG("SimmSensitivityStorageManager::processFxOption called");
    Date asof = Settings::instance().evaluationDate();
    DayCounter dc = Actual365Fixed(); // FIXME

    // just for convenience
    const Size& n = nCurveTenors_;
    const Size c = currencies_.size();
    const std::string& baseCcyCode = currencies_[0];

    QuantLib::ext::shared_ptr<FxOption> fxOpt = QuantLib::ext::dynamic_pointer_cast<FxOption>(trade);
    QuantLib::ext::shared_ptr<Instrument> qlInstr = fxOpt->instrument()->qlInstrument();

    // handle expired trade
    if (qlInstr->isExpired())
        return;

    Real tradeMultiplier = fxOpt->instrument()->multiplier();

    Currency forCcy = parseCurrency(fxOpt->boughtCurrency());
    Currency domCcy = parseCurrency(fxOpt->soldCurrency());
    Size forCcyIndex = getCcyIndex(fxOpt->boughtCurrency());
    Size domCcyIndex = getCcyIndex(fxOpt->soldCurrency());
    Real forFx = market->fxRate(fxOpt->boughtCurrency() + baseCcyCode)->value();
    Real domFx = market->fxRate(fxOpt->soldCurrency() + baseCcyCode)->value();

    Real npv = qlInstr->NPV();
    Real spotDelta = qlInstr->result<Real>("deltaSpot");
    // log(fx) delta
    if (forCcyIndex != 0 && domCcyIndex != 0) {
        delta[n * c + forCcyIndex - 1] += spotDelta * forFx * tradeMultiplier;
        delta[n * c + domCcyIndex - 1] += (-spotDelta * forFx + npv * domFx) * tradeMultiplier;
    } else if (forCcyIndex != 0) {
        delta[n * c + forCcyIndex - 1] += spotDelta * forFx * tradeMultiplier;
    } else {
        delta[n * c + domCcyIndex - 1] += (-spotDelta + npv * domFx) * tradeMultiplier;
    }
    // for ccy delta curve risk
    std::vector<Real> deltaDiv = qlInstr->result<std::vector<Real>>("deltaDividend");
    for (Size ii = 0; ii < n; ++ii) {
        delta[forCcyIndex * n + ii] += deltaDiv[ii] * tradeMultiplier * domFx;
    }
    // dom ccy delta curve risk
    std::vector<Real> deltaRate = qlInstr->result<std::vector<Real>>("deltaRate");
    for (Size ii = 0; ii < n; ++ii) {
        delta[domCcyIndex * n + ii] += deltaRate[ii] * tradeMultiplier * domFx;
    }

    // FIXME: Vega
    DLOG("SimmSensitivityStorageManager::processFxOption, process Vega ");
    Real singleVega = qlInstr->result<Real>("singleVega");
    Date exerciseDate = qlInstr->result<Date>("exerciseDate");
    Size idx = 0;
    Real v = singleVega;
    if (domCcyIndex == 0)
        idx = forCcyIndex - 1;
    else if (forCcyIndex == 0)
        idx = domCcyIndex - 1;
    else {
        QL_FAIL("case not covered");
    }
    // rebucket
    map<Size,Real> vegaContributions = bucketMapping(v, exerciseDate, fxExpiryTimes_, asof, dc);	    
    // and add the contributions
    for (auto it : vegaContributions) {
        // LOG("map fx single vega " << singleVega << " to vega vector " << idx << ", " << it.first << ": " << it.second);
	// LOG("map fx single vega, vega size " << vega.size());
	// LOG("map fx single vega, vega size " << vega.size());
	// LOG("map fx single vega, vega[" << idx << "]  size " << vega[idx].size());	
        vega[idx][it.first] += it.second * tradeMultiplier;
    }
}

void SimmSensitivityStorageManager::processFxForward(Array& delta, QuantLib::Real& theta,
						     const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
						     const QuantLib::ext::shared_ptr<Market>& market) const {

    // just for convenience
    const Size& n = nCurveTenors_;
    const Size c = currencies_.size();
    const std::string& baseCcyCode = currencies_[0];

    // get ql isntrument and trade multiplier
    QuantLib::ext::shared_ptr<Instrument> qlInstr = trade->instrument()->qlInstrument();
    Real tradeMultiplier = trade->instrument()->multiplier();

    // cast to FxForward trade
    auto fxFwdTrade = QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
    QL_REQUIRE(fxFwdTrade, "expected FxForward trade class, could not cast");

    // handle expired instrument
    if (qlInstr->isExpired())
        return;

    Currency ccy1 = parseCurrency(fxFwdTrade->boughtCurrency()); // foreign
    Currency ccy2 = parseCurrency(fxFwdTrade->soldCurrency());   // domestic
    Size ccyIndex1 = getCcyIndex(fxFwdTrade->boughtCurrency());
    Size ccyIndex2 = getCcyIndex(fxFwdTrade->soldCurrency());
    Real fx1 = market->fxRate(fxFwdTrade->boughtCurrency() + baseCcyCode)->value();
    Real fx2 = market->fxRate(fxFwdTrade->soldCurrency() + baseCcyCode)->value();

    Real npv1 = qlInstr->result<Real>("npvFor");
    Real npv2 = qlInstr->result<Real>("npvDom");

    if (ccyIndex1 != 0) {
        Real fxDelta1 = fxFwdTrade->boughtCurrency() != baseCcyCode ? tradeMultiplier * npv1 : 0.0;
        delta[n * c + ccyIndex1 - 1] += fxDelta1 * fx1;
    }
    if (ccyIndex2 != 0) {
        Real fxDelta2 = fxFwdTrade->soldCurrency() != baseCcyCode ? tradeMultiplier * npv2 : 0.0;
        delta[n * c + ccyIndex2 - 1] += fxDelta2 * fx2;
    }

    std::vector<Real> deltaDiscount1 = qlInstr->result<result_type_vector>("deltaDiscount")[ccy1];
    std::vector<Real> deltaDiscount2 = qlInstr->result<result_type_vector>("deltaDiscount")[ccy2];
    for (Size ii = 0; ii < n; ++ii) {
        delta[ccyIndex1 * n + ii] += deltaDiscount1[ii] * tradeMultiplier * fx1;
        delta[ccyIndex2 * n + ii] += deltaDiscount2[ii] * tradeMultiplier * fx2;
    }

    return;
}

std::map<QuantLib::Size, QuantLib::Real>
SimmSensitivityStorageManager::bucketMapping(QuantLib::Real value, const QuantLib::Date& date, const std::vector<Real>& timeGrid, const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dc) const {
    Real t = dc.yearFraction(referenceDate, date);
    int n = timeGrid.size();
    int b = (int)(std::upper_bound(timeGrid.begin(), timeGrid.end(), t) - timeGrid.begin());

    std::map<QuantLib::Size, QuantLib::Real> res;

    if (b == 0) {
        res[0] = value;
    }
    else if (b == n) {
        res[n - 1] = value;
    }
    else {
        Real tmp = (timeGrid[b] - t) / (timeGrid[b] - timeGrid[b - 1]);
	res[b - 1] = value * tmp;
        res[b] = value * (1.0 - tmp);
    }

    // check
    Real tmp = 0.0;
    for (auto it : res)
        tmp += it.second;
    QL_REQUIRE(close_enough(tmp, value), "mapping to time grid not close enough");

    return res;

}

/*
std::map<std::pair<QuantLib::Size, QuantLib::Size>, QuantLib::Real>
SimmSensitivityStorageManager::swaptionVega(Real vega, const Date& expiry, const Date& maturity,
                                            const Date& referenceDate, const DayCounter& dc) const {
    Real te = dc.yearFraction(referenceDate, expiry);
    Real tt = dc.yearFraction(expiry, maturity);
    int ne = swaptionExpiryTimes_.size();
    int be = (int)(std::upper_bound(swaptionExpiryTimes_.begin(), swaptionExpiryTimes_.end(), te) -
                   swaptionExpiryTimes_.begin());
    int nt = swaptionTermTimes_.size();
    int bt = (int)(std::upper_bound(swaptionTermTimes_.begin(), swaptionTermTimes_.end(), tt) -
                   swaptionTermTimes_.begin());

    std::map<std::pair<QuantLib::Size, QuantLib::Size>, QuantLib::Real> res;

    // corners
    if (be == 0 && bt == 0) {
        res[pair(0, 0)] = vega;
    }
    else if (be == 0 && bt == nt) {
        res[pair(0, nt - 1)] = vega;
    }
    else if (be == ne && bt == 0) {
        res[pair(ne - 1, 0)] = vega;
    }
    else if (be == ne && bt == nt) {
        res[pair(ne - 1, nt - 1)] = vega;
    }
    // edges
    else if (be == 0) {
        Real tmp = (swaptionTermTimes_[bt] - tt) / (swaptionTermTimes_[bt] - swaptionTermTimes_[bt - 1]);
        res[pair(0, bt - 1)] = vega * tmp;
        res[pair(0, bt)] = vega * (1.0 - tmp);
    }
    else if (be == ne) {
        Real tmp = (swaptionTermTimes_[bt] - tt) / (swaptionTermTimes_[bt] - swaptionTermTimes_[bt - 1]);
        res[pair(ne - 1, bt - 1)] = vega * tmp;
        res[pair(ne - 1, bt)] = vega * (1.0 - tmp);
    }
    else if (bt == 0) {
        Real tmp = (swaptionExpiryTimes_[be] - te) / (swaptionExpiryTimes_[be] - swaptionExpiryTimes_[be - 1]);
        res[pair(be - 1, 0)] = vega * tmp;
        res[pair(be, 0)] = vega * (1.0 - tmp);
    }
    else if (bt == nt) {
        Real tmp = (swaptionExpiryTimes_[be] - te) / (swaptionExpiryTimes_[be] - swaptionExpiryTimes_[be - 1]);
        res[pair(be - 1, nt-1)] = vega * tmp;
        res[pair(be, nt-1)] = vega * (1.0 - tmp);
    }
    // bulk
    else {
        Real tmpe = (swaptionExpiryTimes_[be] - te) / (swaptionExpiryTimes_[be] - swaptionExpiryTimes_[be - 1]);
        Real tmpt = (swaptionTermTimes_[bt] - tt) / (swaptionTermTimes_[bt] - swaptionTermTimes_[bt - 1]);
        res[pair(be - 1, bt - 1)] = vega * tmpe * tmpt;
        res[pair(be - 1, bt)] = vega * tmpe * (1.0 - tmpt);
        res[pair(be, bt - 1)] = vega * (1.0 - tmpe) * tmpt;
        res[pair(be, bt)] = vega * (1.0 - tmpe) * (1.0 - tmpt);
    }

    Real tmp = 0.0;
    for (auto it : res)
        tmp += it.second;
    QL_REQUIRE(close_enough(tmp, vega), "vega mapping not close enough");

    return res;
}
*/

  
} // namespace analytics
} // namespace ore
