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

#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/instruments/currencyswap.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;
using namespace ore::analytics;

namespace ore {
namespace analytics {

// result types, taken from qlep/pricingengines/discountingcurrencyswapenginedeltagamma.hpp
struct CurrencyComparator {
    bool operator()(const Currency& c1, const Currency& c2) const { return c1.code() < c2.code(); }
};
typedef std::map<Currency, Matrix, CurrencyComparator> result_type_matrix;
typedef std::map<Currency, std::vector<Real>, CurrencyComparator> result_type_vector;
typedef std::map<Currency, Real, CurrencyComparator> result_type_scalar;

CamSensitivityStorageManager::CamSensitivityStorageManager(
    const std::vector<std::string>& camCurrencies, const Size nCurveSensitivities, const Size nVegaOptSensitivities,
    const Size nVegaUndSensitivities, const Size nFxVegaSensitivities, const Size firstCubeIndexToUse,
    const bool use2ndOrderSensitivities)
    : camCurrencies_(camCurrencies), nCurveSensitivities_(nCurveSensitivities),
      nVegaOptSensitivities_(nVegaOptSensitivities), nVegaUndSensitivities_(nVegaUndSensitivities),
      nFxVegaSensitivities_(nFxVegaSensitivities), firstCubeIndexToUse_(firstCubeIndexToUse),
      use2ndOrderSensitivities_(use2ndOrderSensitivities) {

    QL_REQUIRE(!camCurrencies_.empty(), "CamSensitivityStorageManager: camCurrencies are empty");

    N_ = nCurveSensitivities_ * camCurrencies_.size() + (camCurrencies_.size() - 1);
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

Size getNettingSetIndex(const std::string& nettingSetId, const boost::shared_ptr<NPVCube>& cube) {
    QL_REQUIRE(cube, "SensitivityCalculator::calculate(): no cube given to store sensitivity");
    auto nid = std::find(cube->ids().begin(), cube->ids().end(), nettingSetId);
    QL_REQUIRE(nid != cube->ids().end(),
               "SensitivityCalculator::calculate(): did not find nettingSetId '" << nettingSetId << "' in cube");
    return std::distance(cube->ids().begin(), nid);
}

} // namespace

void CamSensitivityStorageManager::addSensitivities(boost::shared_ptr<ore::analytics::NPVCube> cube,
                                                    const boost::shared_ptr<ore::data::Trade>& trade,
                                                    const boost::shared_ptr<Market>& market, const Size dateIndex,
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
        } else {
            QL_FAIL("trade type '" << trade->tradeType() << "' not supported");
        }

        // serialise the results into a vector that we can write to the cube

        std::vector<Real> cubeData;

        for (auto const& d : delta) {
            cubeData.push_back(d);
        }

        cubeData.push_back(theta);

        if (use2ndOrderSensitivities_) {
            for (Size i = 0; i < gamma.rows(); ++i) {
                for (Size j = 0; j <= i; ++j) {
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
        WLOG("CamSensitivityStorageManager::addSensitivities(): failed to get sensitivities for trade '"
             << trade->id() << "': " << e.what() << " - not adding sensitivities to cube");
    }
}

boost::any CamSensitivityStorageManager::getSensitivities(const boost::shared_ptr<ore::analytics::NPVCube>& cube,
                                                          const std::string& nettingSetId, const Size dateIndex,
                                                          const Size sampleIndex) const {

    QL_REQUIRE(cube, "CamSensitivityStorageManager::getSensitivites(): no cube to retrieve results from");

    Array delta(N_, 0.0);
    Matrix gamma(N_, N_, 0.0);
    Real theta = 0.0;

    // get data from cube

    std::vector<Real> cubeData;
    Size idx = firstCubeIndexToUse_;
    Size nettingSetIndex = getNettingSetIndex(nettingSetId, cube);
    for (Size i = 0; i < getRequiredSize(); ++i) {
        Real tmp;
        if (dateIndex == Null<Size>()) {
            tmp = cube->getT0(nettingSetIndex, idx);
        } else {
            tmp = cube->get(nettingSetIndex, dateIndex, sampleIndex, idx);
        }
        cubeData.push_back(tmp);
    }

    // deserialise data from cube into delta, gamma theta

    for (Size i = 0; i < N_; ++i) {
        delta[i] = cubeData[i];
    }

    theta = cubeData[N_];

    if (use2ndOrderSensitivities_) {
        Size idx = 0;
        for (Size i = 0; i < N_; ++i) {
            for (Size j = 0; j <= i; ++i) {
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
CamSensitivityStorageManager::processSwapSwaption(boost::shared_ptr<ore::analytics::NPVCube> cube,
                                                  const boost::shared_ptr<ore::data::Trade>& trade,
                                                  const boost::shared_ptr<Market>& market) const {

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
    boost::shared_ptr<Instrument> qlInstr = trade->instrument()->qlInstrument();
    Real tradeMultiplier = trade->instrument()->multiplier();

    // do we have an xccy swap?
    bool isXCCY = false;
    for (Size k = 1; k < currencies.size(); ++k)
        isXCCY = isXCCY || (currencies[k] != currencies[0]);

    if (!isXCCY) {

        // determine relevant ql instrument and trade multiplier in case we have a swaption
        bool hasThetaVega = true;
        if (trade->tradeType() == "Swaption") {
            boost::shared_ptr<OptionWrapper> wrapper = boost::dynamic_pointer_cast<OptionWrapper>(trade->instrument());
            if (wrapper) { // option wrapper (i.e. we have a swaption)
                if (wrapper->isExercised()) {
                    qlInstr = wrapper->activeUnderlyingInstrument();
                    tradeMultiplier = wrapper->underlyingMultiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                    hasThetaVega = false;
                } else {
                    qlInstr = wrapper->qlInstrument();
                    tradeMultiplier = wrapper->multiplier() * (wrapper->isLong() ? 1.0 : -1.0);
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
        Real fx = market->fxSpot(currencies[0] + baseCcyCode)->value();
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
                               fx;
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
        Real fx1 = market->fxSpot(distinctCurrs[0] + baseCcyCode)->value();
        Real fx2 = market->fxSpot(distinctCurrs[1] + baseCcyCode)->value();
        boost::shared_ptr<CurrencySwap> instr =
            boost::dynamic_pointer_cast<CurrencySwap>(trade->instrument()->qlInstrument());
        QL_REQUIRE(instr, "Cross Currency Swap: Expected QL instrument CurrencySwap");
        std::map<string, Real> legNpvs;
        for (Size l = 0; l < currencies.size(); ++l) {
            legNpvs[currencies[l]] += instr->inCcyLegNPV(l);
        }
        if (ccyIndex1 != 0) {
            Real fxDelta1 = distinctCurrs[0] != baseCcyCode ? tradeMultiplier * legNpvs.at(distinctCurrs[0]) : 0.0;
            delta[n * c + ccyIndex1 - 1] += fxDelta1 * fx1;
        }
        if (ccyIndex2 != 0) {
            Real fxDelta2 = distinctCurrs[1] != baseCcyCode ? tradeMultiplier * legNpvs.at(distinctCurrs[1]) : 0.0;
            delta[n * c + ccyIndex2 - 1] += fxDelta2 * fx2;
        }
        std::vector<Real> deltaDiscount1 =
            trade->instrument()->qlInstrument()->result<result_type_vector>("deltaDiscount")[ccy1];
        std::vector<Real> deltaForward1 = instr->result<result_type_vector>("deltaForward")[ccy1];
        std::vector<Real> deltaDiscount2 = instr->result<result_type_vector>("deltaDiscount")[ccy2];
        std::vector<Real> deltaForward2 = instr->result<result_type_vector>("deltaForward")[ccy2];
        for (Size ii = 0; ii < n; ++ii) {
            delta[ccyIndex1 * n + ii] += (deltaDiscount1[ii] + deltaForward1[ii]) * tradeMultiplier * fx1;
            delta[ccyIndex2 * n + ii] += (deltaDiscount1[ii] + deltaForward1[ii]) * tradeMultiplier * fx2;
        }
        if (use2ndOrderSensitivities_) {
            Matrix inputGamma1 = instr->result<result_type_matrix>("gamma")[ccy1];
            Matrix inputGamma2 = instr->result<result_type_matrix>("gamma")[ccy2];
            // IR-IR gamma
            for (Size ii = 0; ii < n; ++ii) {
                for (Size jj = 0; jj <= ii; ++jj) {
                    Real tmp1 = (inputGamma1[ii][jj] + inputGamma1[n + ii][jj] + inputGamma1[ii][n + jj] +
                                 inputGamma1[n + ii][n + jj]) *
                                fx1;
                    Real tmp2 = (inputGamma2[ii][jj] + inputGamma2[n + ii][jj] + inputGamma2[ii][n + jj] +
                                 inputGamma2[n + ii][n + jj]) *
                                fx2;
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
CamSensitivityStorageManager::processFxOption(boost::shared_ptr<ore::analytics::NPVCube> cube,
                                              const boost::shared_ptr<ore::data::Trade>& trade,
                                              const boost::shared_ptr<Market>& market) const {

    // just for convenience
    const Size& n = nCurveSensitivities_;
    const Size& u = nVegaOptSensitivities_;
    const Size& v = nVegaUndSensitivities_;
    const Size& w = nFxVegaSensitivities_;
    const Size c = camCurrencies_.size();

    // results to fill
    Array delta(N_, 0.0);
    Matrix gamma(N_, N_, 0.0);
    Real theta = 0.0;

    // FIXME dom is not necessarily base ccy!
    boost::shared_ptr<FxOption> fxOpt = boost::dynamic_pointer_cast<FxOption>(trade);

    boost::shared_ptr<Instrument> qlInstr = fxOpt->instrument()->qlInstrument();
    if (qlInstr->isExpired())
        return std::make_tuple(delta, gamma, theta);
    Real tradeMultiplier = fxOpt->instrument()->multiplier();

    Currency ccy = parseCurrency(fxOpt->boughtCurrency());
    Currency domCcy = parseCurrency(fxOpt->soldCurrency());
    Size ccyIndex = getCcyIndex(fxOpt->boughtCurrency());
    Size domCcyIndex = getCcyIndex(fxOpt->soldCurrency());
    Real fx = market->fxSpot(fxOpt->boughtCurrency() + fxOpt->soldCurrency())->value();
    QL_REQUIRE(ccyIndex != 0, "FX Option: foreign currency is equal to base simulation currency");
    QL_REQUIRE(domCcyIndex == 0, "FX Option: domestic currency must be equal to base simulation currency");
    // log(fx) delta, i.e. multiply by fx
    delta[n * c + ccyIndex - 1] += qlInstr->result<Real>("deltaSpot") * tradeMultiplier * fx;
    // for ccy delta curve risk
    std::vector<Real> deltaDiv = qlInstr->result<std::vector<Real>>("deltaDividend");
    for (Size ii = 0; ii < n; ++ii) {
        delta[ccyIndex * n + ii] += deltaDiv[ii] * tradeMultiplier * fx;
    }
    // dom ccy delta curve risk
    std::vector<Real> deltaRate = qlInstr->result<std::vector<Real>>("deltaRate");
    for (Size ii = 0; ii < n; ++ii) {
        delta[ii] += deltaRate[ii] * tradeMultiplier;
    }
    if (use2ndOrderSensitivities_) {
        Real spotGamma = qlInstr->result<Real>("gammaSpot");
        Matrix inputGamma = qlInstr->result<Matrix>("gamma");
        std::vector<Real> spotRateGamma = qlInstr->result<std::vector<Real>>("gammaSpotRate");
        std::vector<Real> spotDivGamma = qlInstr->result<std::vector<Real>>("gammaSpotDiv");
        // IR-IR gamma
        for (Size ii = 0; ii < n; ++ii) {
            for (Size jj = 0; jj <= ii; ++jj) {
                gamma[ccyIndex * n + ii][ccyIndex * n + jj] += inputGamma[n + ii][n + jj];
                gamma[ii][jj] += inputGamma[ii][jj];
                gamma[ccyIndex * n + ii][jj] += inputGamma[n + ii][jj];
                gamma[ii][ccyIndex * n + jj] += inputGamma[ii][n + jj];
                if (ii != jj) {
                    gamma[ccyIndex * n + jj][ccyIndex * n + ii] += inputGamma[n + ii][n + jj];
                    gamma[jj][ii] += inputGamma[ii][jj];
                    gamma[jj][ccyIndex * n + ii] += inputGamma[n + ii][jj];
                    gamma[ccyIndex * n + jj][ii] += inputGamma[ii][n + jj];
                }
            }
        }
        // IR-FX gamma
        for (Size ii = 0; ii < n; ++ii) {
            // log(fx) delta
            gamma[n * c + ccyIndex - 1][ccyIndex * n + ii] += spotDivGamma[ii] * fx;
            gamma[n * c + ccyIndex - 1][ii] += spotRateGamma[ii] * fx;
            gamma[ccyIndex * n + ii][n * c + ccyIndex - 1] += spotDivGamma[ii] * fx;
            gamma[ii][n * c + ccyIndex - 1] += spotRateGamma[ii] * fx;
        }
        // FX-FX gamma
        gamma[n * c + ccyIndex - 1][n * c + ccyIndex - 1] = spotGamma * fx * fx;
    }

    return std::make_tuple(delta, gamma, theta);
}

} // namespace analytics
} // namespace ore
