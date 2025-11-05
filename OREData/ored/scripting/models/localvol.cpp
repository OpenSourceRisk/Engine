/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/localvol.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

void LocalVol::performModelCalculations() const {
    if (type_ == Model::Type::MC)
        performCalculationsMc();
    else if (type_ == Model::Type::FD)
        performCalculationsFd();
}

Real LocalVol::initialValue(const Size indexNo) const {
    return model_->generalizedBlackScholesProcesses()[indexNo]->x0();
}

Real LocalVol::atmForward(const Size indexNo, const Real t) const {
    return ore::data::atmForward(model_->generalizedBlackScholesProcesses()[indexNo]->x0(),
                                 model_->generalizedBlackScholesProcesses()[indexNo]->riskFreeRate(),
                                 model_->generalizedBlackScholesProcesses()[indexNo]->dividendYield(), t);
}

Real LocalVol::compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const {
    const auto& p = model_->generalizedBlackScholesProcesses().at(indexNo);
    return p->dividendYield()->discount(d1) / p->dividendYield()->discount(d2) /
           (p->riskFreeRate()->discount(d1) / p->riskFreeRate()->discount(d2));
}

void LocalVol::performCalculationsMc() const {
    initUnderlyingPathsMc();
    setReferenceDateValuesMc();
    if (effectiveSimulationDates_.size() == 1)
        return;
    generatePaths();
}

void LocalVol::performCalculationsFd() const { QL_FAIL("LocalVol::performCalculationsFdLv() not implemented."); }

void LocalVol::generatePaths() const {

    // compile the correlation matrix

    Matrix correlation = getCorrelation();

    // compute the sqrt correlation

    Matrix sqrtCorr = pseudoSqrt(correlation, params_.salvagingAlgorithm);

    // precompute the deterministic part of the drift on each time step

    std::vector<Array> deterministicDrift(timeGrid_.size() - 1, Array(indices_.size(), 0.0));

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        for (Size j = 0; j < indices_.size(); ++j) {
            Real t0 = timeGrid_[i];
            Real t1 = timeGrid_[i + 1];
            deterministicDrift[i][j] =
                -std::log(model_->generalizedBlackScholesProcesses()[j]->riskFreeRate()->discount(t1) /
                          model_->generalizedBlackScholesProcesses()[j]->dividendYield()->discount(t1) /
                          (model_->generalizedBlackScholesProcesses()[j]->riskFreeRate()->discount(t0) /
                           model_->generalizedBlackScholesProcesses()[j]->dividendYield()->discount(t0)));
        }
    }

    // precompute index for drift adjustment for eq / com indices that are not in base ccy

    std::vector<Size> eqComIdx(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        Size idx = Null<Size>();
        if (!indices_[j].isFx()) {
            // do we have an fx index with the desired currency?
            for (Size jj = 0; jj < indices_.size(); ++jj) {
                if (indices_[jj].isFx()) {
                    if (indexCurrencies_[jj] == indexCurrencies_[j])
                        idx = jj;
                }
            }
        }
        eqComIdx[j] = idx;
    }

    // precompute some time related quantities

    std::vector<Real> t(timeGrid_.size() - 1), dt(timeGrid_.size() - 1), sqrtdt(timeGrid_.size() - 1);
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        t[i] = timeGrid_[i];
        dt[i] = timeGrid_[i + 1] - timeGrid_[i];
        sqrtdt[i] = std::sqrt(dt[i]);
    }

    // evolve the process using correlated normal variates and set the underlying path values

    populatePathValuesLv(size(), underlyingPaths_,
                         makeMultiPathVariateGenerator(params_.sequenceType, indices_.size(), timeGrid_.size() - 1,
                                                       params_.seed, params_.sobolOrdering,
                                                       params_.sobolDirectionIntegers),
                         correlation, sqrtCorr, deterministicDrift, eqComIdx, t, dt, sqrtdt);

    if (trainingSamples() != Null<Size>()) {
        populatePathValuesLv(trainingSamples(), underlyingPathsTraining_,
                             makeMultiPathVariateGenerator(params_.trainingSequenceType, indices_.size(),
                                                           timeGrid_.size() - 1, params_.trainingSeed,
                                                           params_.sobolOrdering, params_.sobolDirectionIntegers),
                             correlation, sqrtCorr, deterministicDrift, eqComIdx, t, dt, sqrtdt);
    }

    setAdditionalResults();
} // generatePathsLv()

void LocalVol::populatePathValuesLv(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                    const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                    const Matrix& correlation, const Matrix& sqrtCorr,
                                    const std::vector<Array>& deterministicDrift, const std::vector<Size>& eqComIdx,
                                    const std::vector<Real>& t, const std::vector<Real>& dt,
                                    const std::vector<Real>& sqrtdt) const {

    Array stateDiff(indices_.size()), logState(indices_.size()), logState0(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        logState0[j] = std::log(model_->generalizedBlackScholesProcesses()[j]->x0());
    }

    std::vector<std::vector<RandomVariable*>> rvs(indices_.size(),
                                                  std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            rvs[j][i] = &paths[*date][j];
            rvs[j][i]->expand();
        }
    }

    for (Size path = 0; path < size(); ++path) {
        auto p = gen->next();
        logState = logState0;
        std::size_t date = 0;
        auto pos = positionInTimeGrid_.begin();
        ++pos;
        // evolve the process on the refined time grid
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            for (Size j = 0; j < indices_.size(); ++j) {
                // localVol might throw / return nan, inf, handle these cases here
                // by setting the local vol to zero
                Real volj = 0.0;
                try {
                    volj = model_->generalizedBlackScholesProcesses()[j]->localVolatility()->localVol(
                        t[i], std::exp(logState[j]));
                } catch (...) {
                }
                if (!std::isfinite(volj))
                    volj = 0.0;
                Real dw = 0;
                for (Size k = 0; k < indices_.size(); ++k) {
                    dw += sqrtCorr[j][k] * p.value[i][k];
                }
                stateDiff[j] = volj * dw * sqrtdt[i] - 0.5 * volj * volj * dt[i];
                // drift adjustment for eq / com indices that are not in base ccy
                if (eqComIdx[j] != Null<Size>()) {
                    Real volIdx = model_->generalizedBlackScholesProcesses()[eqComIdx[j]]->localVolatility()->localVol(
                        t[i], std::exp(logState[eqComIdx[j]]));
                    stateDiff[j] -= correlation[eqComIdx[j]][j] * volIdx * volj * dt[i];
                }
            }
            // update state with stateDiff from above and deterministic part of the drift
            logState += stateDiff + deterministicDrift[i];
            // on the effective simulation dates populate the underlying paths
            if (i + 1 == *pos) {
                for (Size j = 0; j < indices_.size(); ++j)
                    rvs[j][date]->data()[path] = std::exp(logState[j]);
                ++date;
                ++pos;
            }
        }
    }
} // populatePathValuesLV()

void LocalVol::setAdditionalResults() const {

    Matrix correlation = getCorrelation();

    for (Size i = 0; i < indices_.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            additionalResults_["LocalVol.Correlation_" + indices_[i].name() + "_" + indices_[j].name()] =
                correlation(i, j);
        }
    }

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_["LocalVol.CalibrationStrike_" + indices_[i].name()] =
            (calibrationStrikes[i] == Null<Real>() ? "ATMF" : std::to_string(calibrationStrikes[i]));
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        Size timeStep = 0;
        for (auto const& d : effectiveSimulationDates_) {
            Real t = timeGrid_[positionInTimeGrid_[timeStep]];
            Real forward = atmForward(i, t);
            if (timeStep > 0) {
                Real volatility = model_->generalizedBlackScholesProcesses()[i]->blackVolatility()->blackVol(
                    t, (calibrationStrikes.empty() || calibrationStrikes[i] == Null<Real>()) ? forward
                                                                                             : calibrationStrikes[i]);
                additionalResults_["LocalVol.Volatility_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                    volatility;
            }
            additionalResults_["LocalVol.Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] = forward;
            ++timeStep;
        }
    }
}

} // namespace data
} // namespace ore
