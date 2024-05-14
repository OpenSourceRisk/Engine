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

#include <qle/methods/multipathgeneratorbase.hpp>

#include <ql/exercise.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

LocalVol::LocalVol(const Size paths, const std::string& currency, const Handle<YieldTermStructure>& curve,
                   const std::string& index, const std::string& indexCurrency,
                   const Handle<BlackScholesModelWrapper>& model, const McParams& mcParams,
                   const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig)
    : LocalVol(paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, model, {}, mcParams, simulationDates,
               iborFallbackConfig) {}

LocalVol::LocalVol(
    const Size paths, const std::vector<std::string>& currencies, const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const McParams& mcParams, const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig)
    : BlackScholesBase(paths, currencies, curves, fxSpots, irIndices, infIndices, indices, indexCurrencies, model,
                       correlations, mcParams, simulationDates, iborFallbackConfig) {}

void LocalVol::performCalculations() const {

    BlackScholesBase::performCalculations();

    // nothing to do if we do not have any indices

    if (indices_.empty())
        return;

    // init underlying path where we map a date to a randomvariable representing the path values

    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<RandomVariable>(model_->processes().size(), RandomVariable(size(), 0.0));
        if (trainingSamples() != Null<Size>())
            underlyingPathsTraining_[d] =
                std::vector<RandomVariable>(model_->processes().size(), RandomVariable(trainingSamples(), 0.0));
    }

    // compile the correlation matrix

    Matrix correlation = getCorrelation();

    // set reference date values, if there are no future simulation dates we are done

    for (Size l = 0; l < indices_.size(); ++l) {
        underlyingPaths_[*effectiveSimulationDates_.begin()][l].setAll(model_->processes()[l]->x0());
        if (trainingSamples() != Null<Size>()) {
            underlyingPathsTraining_[*effectiveSimulationDates_.begin()][l].setAll(model_->processes()[l]->x0());
        }
    }

    if (effectiveSimulationDates_.size() == 1)
        return;

    // compute the sqrt correlation

    Matrix sqrtCorr = pseudoSqrt(correlation, SalvagingAlgorithm::Spectral);

    // precompute the deterministic part of the drift on each time step

    std::vector<Array> deterministicDrift(timeGrid_.size() - 1, Array(indices_.size(), 0.0));

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        for (Size j = 0; j < indices_.size(); ++j) {
            Real t0 = timeGrid_[i];
            Real t1 = timeGrid_[i + 1];
            deterministicDrift[i][j] = -std::log(model_->processes()[j]->riskFreeRate()->discount(t1) /
                                                 model_->processes()[j]->dividendYield()->discount(t1) /
                                                 (model_->processes()[j]->riskFreeRate()->discount(t0) /
                                                  model_->processes()[j]->dividendYield()->discount(t0)));
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

    populatePathValues(size(), underlyingPaths_,
                       makeMultiPathVariateGenerator(mcParams_.sequenceType, indices_.size(), timeGrid_.size() - 1,
                                                     mcParams_.seed, mcParams_.sobolOrdering,
                                                     mcParams_.sobolDirectionIntegers),
                       correlation, sqrtCorr, deterministicDrift, eqComIdx, t, dt, sqrtdt);

    if (trainingSamples() != Null<Size>()) {
        populatePathValues(trainingSamples(), underlyingPathsTraining_,
                           makeMultiPathVariateGenerator(mcParams_.trainingSequenceType, indices_.size(),
                                                         timeGrid_.size() - 1, mcParams_.trainingSeed,
                                                         mcParams_.sobolOrdering, mcParams_.sobolDirectionIntegers),
                           correlation, sqrtCorr, deterministicDrift, eqComIdx, t, dt, sqrtdt);
    }

} // initPaths()

void LocalVol::populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                  const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                  const Matrix& correlation, const Matrix& sqrtCorr,
                                  const std::vector<Array>& deterministicDrift, const std::vector<Size>& eqComIdx,
                                  const std::vector<Real>& t, const std::vector<Real>& dt,
                                  const std::vector<Real>& sqrtdt) const {

    Array stateDiff(indices_.size()), logState(indices_.size()), logState0(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        logState0[j] = std::log(model_->processes()[j]->x0());
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
                    volj = model_->processes()[j]->localVolatility()->localVol(t[i], std::exp(logState[j]));
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
                    Real volIdx = model_->processes()[eqComIdx[j]]->localVolatility()->localVol(
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
} // populatePathValues()

RandomVariable LocalVol::getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                              const RandomVariable& barrier, const bool above) const {
    QL_FAIL("getFutureBarrierProb not implemented by LocalVol");
}

} // namespace data
} // namespace ore
