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

#include <orea/engine/sensitivitystoragemanager.hpp>

#include <orea/app/structuredanalyticserror.hpp>

#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/instruments/currencyswap.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace ore {
namespace analytics {

CamSensitivityStorageManager::CamSensitivityStorageManager(
    const std::vector<std::string>& camCurrencies, const Size nCurveSensitivities, const Size nVegaOptSensitivities,
    const Size nVegaUndSensitivities, const Size nFxVegaSensitivities, const Size firstCubeIndexToUse,
    const bool use2ndOrderSensitivities)
    : camCurrencies_(camCurrencies), nCurveSensitivities_(nCurveSensitivities),
      firstCubeIndexToUse_(firstCubeIndexToUse), use2ndOrderSensitivities_(use2ndOrderSensitivities) {

    QL_REQUIRE(!camCurrencies_.empty(), "CamSensitivityStorageManager: camCurrencies are empty");

    N_ = nCurveSensitivities_ * camCurrencies_.size() + (camCurrencies_.size() - 1);

    LOG("CamSensitivityStorageManager created");
}

Size CamSensitivityStorageManager::getRequiredSize() const {
    if (use2ndOrderSensitivities_) {
        // delta vector length plus lower triangle of gamma matrix including the diagonal plus theta
        return N_ + N_ * (N_ + 1) / 2 + 1;
    } else {
        // just the delta vector length plus theta
        return N_ + 1;
    }
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

void CamSensitivityStorageManager::addSensitivities(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                                                    const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                                                    const QuantLib::ext::shared_ptr<Market>& market, const Size dateIndex,
                                                    const Size sampleIndex) const {

    QL_REQUIRE((dateIndex == Null<Size>() && sampleIndex == Null<Size>()) ||
                   (dateIndex != Null<Size>() && sampleIndex != Null<Size>()),
               "CamSensitivityStorageManager::addSensitivities(): date and sample index must be both null (write to T0 "
               "slice) or both not null");

    QL_REQUIRE(cube, "CamSensitivityStorageManager::addSensitivites(): no cube to store results given");

    try {

        // get results we want to store in the cube, i.e. delta, gamma and theta

        Array delta;
        Matrix gamma;
        Real theta;

        if (trade->tradeType() == "Swap" || trade->tradeType() == "Swaption") {
            std::tie(delta, gamma, theta) = processSwapSwaption(cube, trade, market);
        } else if (trade->tradeType() == "FxOption") {
            std::tie(delta, gamma, theta) = processFxOption(cube, trade, market);
        } else if (trade->tradeType() == "FxForward") {
            std::tie(delta, gamma, theta) = processFxForward(cube, trade, market);
        } else {
            QL_FAIL("trade type '" << trade->tradeType() << "' not supported");
        }

        // serialise the results into a vector that we can write to the cube

        std::vector<Real> cubeData;

        for (auto const& d : delta) {
            QL_REQUIRE(std::isfinite(d), "delta not finite: " << d);
            cubeData.push_back(d);
        }

        QL_REQUIRE(std::isfinite(theta), "theta not finite: " << theta);
        cubeData.push_back(theta);

        if (use2ndOrderSensitivities_) {
            for (Size i = 0; i < gamma.rows(); ++i) {
                for (Size j = 0; j <= i; ++j) {
                    QL_REQUIRE(std::isfinite(gamma(i, j)), "gamma not finite: " << gamma(i, j));
                    cubeData.push_back(gamma(i, j));
                }
            }
        }

        // write the serialised data to the cube

        Size idx = firstCubeIndexToUse_;
        Size nettingSetIndex = getNettingSetIndex(trade->envelope().nettingSetId(), cube);

        for (auto const& d : cubeData) {
            if (dateIndex == Null<Size>()) {
                Real tmp = cube->getT0(nettingSetIndex, idx);
                cube->setT0(tmp + d, nettingSetIndex, idx);
            } else {
                Real tmp = cube->get(nettingSetIndex, dateIndex, sampleIndex, idx);
                cube->set(tmp + d, nettingSetIndex, dateIndex, sampleIndex, idx);
            }
            ++idx;
        }
    } catch (std::exception& e) {
        auto subFields = map<string, string>({{"tradeId", trade->id()}, {"tradeType", trade->tradeType()}});
        StructuredAnalyticsErrorMessage(
            "Dynamic Sensitivity Calculation", "CamSensitivityStorageManager::addSensitivities()",
            "Failed to get sensitivities for trade: " + to_string(e.what()) + " - not adding sensitivities to cube.",
            subFields)
            .log();
    }

    TLOG("CamSensitivityStorageManager: Added sensitivities to cube for trade="
	<< trade->id() << " sample=" << sampleIndex << " date=" << dateIndex);
}

boost::any CamSensitivityStorageManager::getSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                                          const std::string& nettingSetId, const Size dateIndex,
                                                          const Size sampleIndex) const {

    QL_REQUIRE((dateIndex == Null<Size>() && sampleIndex == Null<Size>()) ||
                   (dateIndex != Null<Size>() && sampleIndex != Null<Size>()),
               "CamSensitivityStorageManager::getSensitivities(): date and sample index must be both null (write to T0 "
               "slice) or both not null");

    QL_REQUIRE(cube, "CamSensitivityStorageManager::getSensitivites(): no cube to retrieve results from");

    Array delta(N_, 0.0);
    Matrix gamma(N_, N_, 0.0);
    Real theta = 0.0;

    // get data from cube

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

    // deserialise data from cube into delta, gamma theta

    for (Size i = 0; i < N_; ++i) {
        delta[i] = cubeData[i];
    }

    theta = cubeData[N_];

    if (use2ndOrderSensitivities_) {
        idx = 0;
        for (Size i = 0; i < N_; ++i) {
            for (Size j = 0; j <= i; ++j) {
                gamma(i, j) = gamma(j, i) = cubeData[N_ + 1 + idx];
                ++idx;
            }
        }
    }

    return std::make_tuple(delta, gamma, theta);
}

Size CamSensitivityStorageManager::getCcyIndex(const std::string& ccy) const {
    auto c = std::find(camCurrencies_.begin(), camCurrencies_.end(), ccy);
    QL_REQUIRE(c != camCurrencies_.end(),
               "CamSensitivityStorageManager::getCcyIndex(): ccy '" << ccy << "' not found in CAM");
    return std::distance(camCurrencies_.begin(), c);
}

std::tuple<Array, Matrix, Real>
CamSensitivityStorageManager::processSwapSwaption(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                                                  const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                                                  const QuantLib::ext::shared_ptr<Market>& market) const {

    // just for convenience
    const Size& n = nCurveSensitivities_;
    const Size c = camCurrencies_.size();
    const std::string& baseCcyCode = camCurrencies_[0];

    // results to fill
    Array delta(N_, 0.0);
    Matrix gamma(N_, N_, 0.0);
    Real theta = 0.0;

    // if the trade has no legs, we are done
    std::vector<std::string> currencies = trade->legCurrencies();
    if (currencies.empty())
        return std::make_tuple(delta, gamma, theta);

    // get ql isntrument and trade multiplier (might be overwritten below for trade wrappers)
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
                    qlInstr = wrapper->activeUnderlyingInstrument();
                    tradeMultiplier = wrapper->underlyingMultiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                    hasThetaVega = false;
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
            return std::make_tuple(delta, gamma, theta);

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
        if (use2ndOrderSensitivities_) {
            Matrix inputGamma = trade->instrument()->qlInstrument()->result<Matrix>("gamma");
            // IR-IR gamma
            for (Size ii = 0; ii < n; ++ii) {
                for (Size jj = 0; jj <= ii; ++jj) {
                    Real tmp = (inputGamma[ii][jj] + inputGamma[n + ii][jj] + inputGamma[ii][n + jj] +
                                inputGamma[n + ii][n + jj]) *
                               fx * tradeMultiplier;
                    gamma[ccyIndex * n + ii][ccyIndex * n + jj] += tmp;
                    if (ii != jj)
                        gamma[ccyIndex * n + jj][ccyIndex * n + ii] += tmp;
                }
            }
            // IR-FX gamma
            if (ccyIndex != 0) {
                for (Size ii = 0; ii < n; ++ii) {
                    Real tmp = delta[ccyIndex * n + ii] * fx;
                    gamma[n * c + ccyIndex - 1][ccyIndex * n + ii] += tmp;
                    gamma[ccyIndex * n + ii][n * c + ccyIndex - 1] += tmp;
                }
            }
        }
        // Theta
        if (hasThetaVega) {
            theta = qlInstr->result<Real>("theta") * tradeMultiplier * fx;
        }
    } else {
        // cross currency swap

        // handle expired instrument
        if (qlInstr->isExpired())
            return std::make_tuple(delta, gamma, theta);

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
        if (use2ndOrderSensitivities_) {
            Matrix inputGamma1 = instr->result<result_type_matrix>("gamma")[ccy1];
            Matrix inputGamma2 = instr->result<result_type_matrix>("gamma")[ccy2];
            // IR-IR gamma
            for (Size ii = 0; ii < n; ++ii) {
                for (Size jj = 0; jj <= ii; ++jj) {
                    Real tmp1 = (inputGamma1[ii][jj] + inputGamma1[n + ii][jj] + inputGamma1[ii][n + jj] +
                                 inputGamma1[n + ii][n + jj]) *
                                fx1 * tradeMultiplier;
                    Real tmp2 = (inputGamma2[ii][jj] + inputGamma2[n + ii][jj] + inputGamma2[ii][n + jj] +
                                 inputGamma2[n + ii][n + jj]) *
                                fx2 * tradeMultiplier;
                    gamma[ccyIndex1 * n + ii][ccyIndex1 * n + jj] += tmp1;
                    gamma[ccyIndex2 * n + ii][ccyIndex2 * n + jj] += tmp2;
                    if (ii != jj) {
                        gamma[ccyIndex1 * n + jj][ccyIndex1 * n + ii] += tmp1;
                        gamma[ccyIndex2 * n + jj][ccyIndex2 * n + ii] += tmp2;
                    }
                }
            }
            // IR-FX gamma
            if (ccyIndex1 != 0) {
                for (Size ii = 0; ii < n; ++ii) {
                    // log(fx) delta
                    Real tmp1 = delta[ccyIndex1 * n + ii] * fx1;
                    gamma[n * c + ccyIndex1 - 1][ccyIndex1 * n + ii] += tmp1;
                    gamma[ccyIndex1 * n + ii][n * c + ccyIndex1 - 1] += tmp1;
                }
            }
            if (ccyIndex2 != 0) {
                for (Size ii = 0; ii < n; ++ii) {
                    // log(fx) delta
                    Real tmp2 = delta[ccyIndex2 * n + ii] * fx2;
                    gamma[n * c + ccyIndex2 - 1][ccyIndex2 * n + ii] += tmp2;
                    gamma[ccyIndex2 * n + ii][n * c + ccyIndex2 - 1] += tmp2;
                }
            }
        }
    }
    return std::make_tuple(delta, gamma, theta);
}

std::tuple<Array, Matrix, Real>
CamSensitivityStorageManager::processFxOption(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                                              const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                                              const QuantLib::ext::shared_ptr<Market>& market) const {

    // just for convenience
    const Size& n = nCurveSensitivities_;
    const Size c = camCurrencies_.size();
    const std::string& baseCcyCode = camCurrencies_[0];

    // results to fill
    Array delta(N_, 0.0);
    Matrix gamma(N_, N_, 0.0);
    Real theta = 0.0;

    QuantLib::ext::shared_ptr<FxOption> fxOpt = QuantLib::ext::dynamic_pointer_cast<FxOption>(trade);
    QuantLib::ext::shared_ptr<Instrument> qlInstr = fxOpt->instrument()->qlInstrument();

    // handle expired trade
    if (qlInstr->isExpired())
        return std::make_tuple(delta, gamma, theta);

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
    if (use2ndOrderSensitivities_) {
        Matrix irGamma = qlInstr->result<Matrix>("gamma");
        // IR-IR gamma
        Real mult = domFx * tradeMultiplier;
        for (Size ii = 0; ii < n; ++ii) {
            for (Size jj = 0; jj < n; ++jj) {
                gamma[domCcyIndex * n + ii][domCcyIndex * n + jj] += irGamma[ii][jj] * mult;
                gamma[domCcyIndex * n + ii][forCcyIndex * n + jj] += irGamma[ii][n + jj] * mult;
                gamma[forCcyIndex * n + ii][domCcyIndex * n + jj] += irGamma[n + ii][jj] * mult;
                gamma[forCcyIndex * n + ii][forCcyIndex * n + jj] += irGamma[n + ii][n + jj] * mult;
            }
        }
        // IR-FX gamma
        std::vector<Real> spotRateGamma = qlInstr->result<std::vector<Real>>("gammaSpotRate");
        std::vector<Real> spotDivGamma = qlInstr->result<std::vector<Real>>("gammaSpotDiv");
        for (Size ii = 0; ii < n; ++ii) {
            // log(fx) delta
            if (forCcyIndex != 0 && domCcyIndex != 0) {
                // forCcyIndex
                Real tmp1 = spotDivGamma[ii] * forFx * tradeMultiplier;
                Real tmp2 = spotRateGamma[ii] * forFx * tradeMultiplier;
                gamma[n * c + forCcyIndex - 1][forCcyIndex * n + ii] += tmp1;
                gamma[forCcyIndex * n + ii][n * c + forCcyIndex - 1] += tmp1;
                gamma[n * c + forCcyIndex - 1][domCcyIndex * n + ii] += tmp2;
                gamma[domCcyIndex * n + ii][n * c + forCcyIndex - 1] += tmp2;
                // domCcyIndex
                Real tmp3 = (-spotDivGamma[ii] * forFx + deltaDiv[ii] * domFx) * tradeMultiplier;
                Real tmp4 = (-spotRateGamma[ii] * forFx + deltaRate[ii] * domFx) * tradeMultiplier;
                gamma[n * c + domCcyIndex - 1][forCcyIndex * n + ii] += tmp3;
                gamma[forCcyIndex * n + ii][n * c + domCcyIndex - 1] += tmp3;
                gamma[n * c + domCcyIndex - 1][domCcyIndex * n + ii] += tmp4;
                gamma[domCcyIndex * n + ii][n * c + domCcyIndex - 1] += tmp4;
            } else if (forCcyIndex != 0) {
                // forCcyIndex
                Real tmp1 = spotDivGamma[ii] * tradeMultiplier;
                Real tmp2 = spotRateGamma[ii] * tradeMultiplier;
                gamma[n * c + forCcyIndex - 1][forCcyIndex * n + ii] += tmp1;
                gamma[forCcyIndex * n + ii][n * c + forCcyIndex - 1] += tmp1;
                gamma[n * c + forCcyIndex - 1][domCcyIndex * n + ii] += tmp2;
                gamma[domCcyIndex * n + ii][n * c + forCcyIndex - 1] += tmp2;
            } else {
                // domCcyIndex
                Real tmp1 = -spotDivGamma[ii] + deltaDiv[ii] * domFx;
                Real tmp2 = -spotRateGamma[ii] + deltaRate[ii] * domFx;
                gamma[n * c + domCcyIndex - 1][forCcyIndex * n + ii] += tmp1;
                gamma[forCcyIndex * n + ii][n * c + domCcyIndex - 1] += tmp1;
                gamma[n * c + domCcyIndex - 1][domCcyIndex * n + ii] += tmp2;
                gamma[domCcyIndex * n + ii][n * c + domCcyIndex - 1] += tmp2;
            }
        }
        // FX-FX gamma
        Real spotGamma = qlInstr->result<Real>("gammaSpot");
        if (forCcyIndex != 0 && domCcyIndex != 0) {
            gamma[n * c + forCcyIndex - 1][n * c + forCcyIndex - 1] +=
                (spotGamma * (forFx * forFx) / domFx + spotDelta * forFx) * tradeMultiplier;
            gamma[n * c + domCcyIndex - 1][n * c + domCcyIndex - 1] +=
                (spotGamma * forFx / domFx - spotDelta * forFx + npv * domFx) * tradeMultiplier;
            Real tmp = (-spotGamma * (forFx * forFx) / domFx) * tradeMultiplier;
            gamma[n * c + domCcyIndex - 1][n * c + forCcyIndex - 1] += tmp;
            gamma[n * c + forCcyIndex - 1][n * c + domCcyIndex - 1] += tmp;
        } else if (forCcyIndex != 0) {
            gamma[n * c + forCcyIndex - 1][n * c + forCcyIndex - 1] +=
                (spotGamma * forFx * forFx + spotDelta * forFx) * tradeMultiplier;
        } else {
            gamma[n * c + domCcyIndex - 1][n * c + domCcyIndex - 1] +=
                (spotGamma / domFx - spotDelta + npv * domFx) * tradeMultiplier;
        }
    }
    return std::make_tuple(delta, gamma, theta);
}

std::tuple<Array, Matrix, Real>
CamSensitivityStorageManager::processFxForward(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                                               const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                                               const QuantLib::ext::shared_ptr<Market>& market) const {

    // just for convenience
    const Size& n = nCurveSensitivities_;
    const Size c = camCurrencies_.size();
    const std::string& baseCcyCode = camCurrencies_[0];

    // results to fill
    Array delta(N_, 0.0);
    Matrix gamma(N_, N_, 0.0);
    Real theta = 0.0;

    // get ql isntrument and trade multiplier
    QuantLib::ext::shared_ptr<Instrument> qlInstr = trade->instrument()->qlInstrument();
    Real tradeMultiplier = trade->instrument()->multiplier();

    // cast to FxForward trade
    auto fxFwdTrade = QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
    QL_REQUIRE(fxFwdTrade, "expected FxForward trade class, could not cast");

    // handle expired instrument
    if (qlInstr->isExpired())
        return std::make_tuple(delta, gamma, theta);

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

    if (use2ndOrderSensitivities_) {
        Matrix inputGamma1 = qlInstr->result<result_type_matrix>("gamma")[ccy1];
        Matrix inputGamma2 = qlInstr->result<result_type_matrix>("gamma")[ccy2];
        // IR-IR gamma
        for (Size ii = 0; ii < n; ++ii) {
            for (Size jj = 0; jj <= ii; ++jj) {
                Real tmp1 = inputGamma1[ii][jj] * fx1 * tradeMultiplier;
                Real tmp2 = inputGamma2[ii][jj] * fx2 * tradeMultiplier;
                gamma[ccyIndex1 * n + ii][ccyIndex1 * n + jj] += tmp1;
                gamma[ccyIndex2 * n + ii][ccyIndex2 * n + jj] += tmp2;
                if (ii != jj) {
                    gamma[ccyIndex1 * n + jj][ccyIndex1 * n + ii] += tmp1;
                    gamma[ccyIndex2 * n + jj][ccyIndex2 * n + ii] += tmp2;
                }
            }
        }
        // IR-FX gamma
        if (ccyIndex1 != 0) {
            for (Size ii = 0; ii < n; ++ii) {
                // log(fx) delta
                Real tmp1 = delta[ccyIndex1 * n + ii] * fx1;
                gamma[n * c + ccyIndex1 - 1][ccyIndex1 * n + ii] += tmp1;
                gamma[ccyIndex1 * n + ii][n * c + ccyIndex1 - 1] += tmp1;
            }
        }
        if (ccyIndex2 != 0) {
            for (Size ii = 0; ii < n; ++ii) {
                // log(fx) delta
                Real tmp2 = delta[ccyIndex2 * n + ii] * fx2;
                gamma[n * c + ccyIndex2 - 1][ccyIndex2 * n + ii] += tmp2;
                gamma[ccyIndex2 * n + ii][n * c + ccyIndex2 - 1] += tmp2;
            }
        }
    }
    return std::make_tuple(delta, gamma, theta);
}

} // namespace analytics
} // namespace ore
