/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/heston.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

void Heston::performModelCalculations() const {
    if (type_ == Model::Type::MC)
        performCalculationsMc();
    else if (type_ == Model::Type::FD)
        performCalculationsFd();
}

Real Heston::initialValue(const Size indexNo) const {
    // TODO, see localvol.cpp for a template
    //return model_->generalizedBlackScholesProcesses()[indexNo]->x0();
    // FIXME: Check correctness of indexNo, what if we have a mix of BS and Heston proceses?
    return model_->hestonProcesses()[indexNo]->s0()->value();
}

Real Heston::atmForward(const Size indexNo, const Real t) const {
    // TODO, see localvol.cpp for a template
    return ore::data::atmForward(model_->generalizedBlackScholesProcesses()[indexNo]->x0(),
                                 model_->generalizedBlackScholesProcesses()[indexNo]->riskFreeRate(),
                                 model_->generalizedBlackScholesProcesses()[indexNo]->dividendYield(), t);
}

Real Heston::compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const {
    // TODO, see localvol.cpp for a template
    const auto& p = model_->generalizedBlackScholesProcesses().at(indexNo);
    return p->dividendYield()->discount(d1) / p->dividendYield()->discount(d2) /
           (p->riskFreeRate()->discount(d1) / p->riskFreeRate()->discount(d2));
}

void Heston::performCalculationsMc() const {
    LOG("Heston::performCalculationsMc() called");
    initUnderlyingPathsMc();
    setReferenceDateValuesMc();
    if (effectiveSimulationDates_.size() == 1)
        return;
    generatePaths();
}

void Heston::performCalculationsFd() const {
    // TODO, see localvol.cpp for a template
    QL_FAIL("Heston::performCalculationsFd() not implemented");
}

void Heston::populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                const Matrix& correlation, const Matrix& sqrtCorr,
                                const std::vector<Array>& deterministicDrift, const std::vector<Size>& eqComIdx,
                                const std::vector<Real>& t, const std::vector<Real>& dt,
                                const std::vector<Real>& sqrtdt) const {
    // TODO, see localvol.cpp for a template
}

void Heston::generatePaths() const {
    // TODO: multi-asset paths, expand the correlation matrix to include the variance processes
    generateSingleAssetPaths();
}


void Heston::generateSingleAssetPaths() const {
    // TODO: drift adjustment for indices that are not in base ccy

    DLOG("Heston::generateSingleAssetPaths() called");

    QL_REQUIRE(indices_.size() == 1, "only a single index is covered so far");
    QL_REQUIRE(model_->hestonProcesses().size() == 1, "only a single heston process is covered so far");

    auto process = model_->hestonProcesses().front();
    
    std::vector<std::vector<RandomVariable*>> rvs(indices_.size(),
                                                  std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            rvs[j][i] = &underlyingPaths_[*date][j];
            rvs[j][i]->expand();
        }
    }
    DLOG("Heston::generateSingleAssetPaths rvs connected");

    // single asset: dimension = 2
    auto gen = makeMultiPathVariateGenerator(params_.sequenceType, indices_.size() * 2,
					     timeGrid_.size() - 1, params_.seed,
					     params_.sobolOrdering, params_.sobolDirectionIntegers);
    DLOG("Heston::generateSingleAssetPaths generator built");

    // single asset
    Array state(2), state0(2);
    state0[0] = model_->hestonProcesses()[0]->s0()->value();
    state0[1] = model_->hestonProcesses()[0]->v0();
    Array dw(2);

    DLOG("Heston::generateSingleAssetPaths initial state done");

    for (Size path = 0; path < size(); ++path) {
        auto p = gen->next();
        state = state0;
        std::size_t date = 0;
        auto pos = positionInTimeGrid_.begin();
        ++pos;

	// evolve the process on the refined time grid
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            Real t0 = timeGrid_[i];
            Real dt = timeGrid_[i+1] - t0;

	    // 2-d array of independent Wiener increments
            dw = p.value[i];

            // Heston process turns dw into correlated increments
            state = process->evolve(t0, state, dt, dw);

            // on the effective simulation dates populate the underlying paths
            if (i + 1 == *pos) {
                rvs[0][date]->data()[path] = state[0];
                ++date;
                ++pos;
            }
        }
    }

    DLOG("Heston::generateSingleAssetPaths() done");
}

void Heston::setAdditionalResults() const {

    Matrix correlation = getCorrelation();

    for (Size i = 0; i < indices_.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            additionalResults_["Heston.Correlation_" + indices_[i].name() + "_" + indices_[j].name()] =
                correlation(i, j);
        }
    }

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_["Heston.CalibrationStrike_" + indices_[i].name()] =
            (calibrationStrikes[i] == Null<Real>() ? "ATMF" : std::to_string(calibrationStrikes[i]));
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        Size timeStep = 0;
        for (auto const& d : effectiveSimulationDates_) {
            Real t = timeGrid_[positionInTimeGrid_[timeStep]];
            Real forward = atmForward(i, t);
            additionalResults_["Heston.Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] = forward;
            ++timeStep;
        }
    }
}

} // namespace data
} // namespace ore
