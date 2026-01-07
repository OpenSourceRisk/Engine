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
#include <qle/processes/quantohestonprocess.hpp>
#include <qle/processes/multiassetquantohestonprocess.hpp>
#include <sstream>

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
    return model_->hestonProcesses()[indexNo]->s0()->value();
}

Real Heston::atmForward(const Size indexNo, const Real t) const {
    return ore::data::atmForward(model_->hestonProcesses()[indexNo]->s0()->value(),
                                 model_->hestonProcesses()[indexNo]->riskFreeRate(),
                                 model_->hestonProcesses()[indexNo]->dividendYield(), t);
}

Real Heston::compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const {
    const auto& p = model_->hestonProcesses().at(indexNo);
    return p->dividendYield()->discount(d1) / p->dividendYield()->discount(d2) /
           (p->riskFreeRate()->discount(d1) / p->riskFreeRate()->discount(d2));
}

void Heston::performCalculationsMc() const {
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

void Heston::generatePaths() const {

    if (indices_.size() == 1) {
        DLOG("Heston::generatePaths() called for single index:");
        DLOG(indices_.front());
    } else {
        DLOG("Heston::generatePaths() called for " << indices_.size() << " indices:");
        for (auto i : indices_) {
            DLOG(i);
	}
    }

    // Precompute index for drift adjustment for eq / com indices that are not in base ccy
    Size n = indices_.size();
    std::vector<Size> eqComIdx(n);
    for (Size j = 0; j < n; ++j) {
        Size idx = Null<Size>();
        if (!indices_[j].isFx()) {
            // do we have an fx index with the desired currency?
            for (Size jj = 0; jj < n; ++jj) {
                if (indices_[jj].isFx()) {
                    if (indexCurrencies_[jj] == indexCurrencies_[j])
                        idx = jj;
                }
            }
        }
        eqComIdx[j] = idx;
    }

    for (Size j = 0; j < n; ++j) {
        auto process = model_->hestonProcesses()[j];
        auto d = process->discretization();
        if (eqComIdx[j] != Null<Size>() && d != HestonProcess::FullTruncation &&
            d != HestonProcess::PartialTruncation && d != HestonProcess::Reflection) {
            ALOG("The use of HestonProcess::Discretization "
                 << d << " is inconsistent with the Euler quanto adjustment for process " << j << ", "
                 << " use full/partial truncation or reflection schemes with sufficiently large number of time steps.");
        }
    }

    // correlation matrix business is handled in the MultiAssetHestonProcess class, so we just pass dummies here
    Matrix correlation(1, 1, 1), sqrtCorr(1, 1, 1);

    populatePathValues(size(), underlyingPaths_,
                       makeMultiPathVariateGenerator(params_.sequenceType, 2 * n, timeGrid_.size() - 1, params_.seed,
                                                     params_.sobolOrdering, params_.sobolDirectionIntegers),
                       correlation, sqrtCorr, eqComIdx);

    if (trainingSamples() != Null<Size>()) {
        populatePathValues(trainingSamples(), underlyingPathsTraining_,
                           makeMultiPathVariateGenerator(params_.trainingSequenceType, 2 * n, timeGrid_.size() - 1,
                                                         params_.trainingSeed, params_.sobolOrdering,
                                                         params_.sobolDirectionIntegers),
                           correlation, sqrtCorr, eqComIdx);
        }

        setAdditionalResults();

        DLOG("Heston::generatePaths() done");
    }

void Heston::populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                const Matrix& correlation, const Matrix& sqrtCorr,
                                const std::vector<Size>& eqComIdx) const {
    
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
    
    Matrix C = getCorrelation();
    auto quantoProcess = QuantLib::ext::make_shared<MultiAssetQuantoHestonProcess>(model_->hestonProcesses(), eqComIdx, C);

    Array initialState = quantoProcess->initialValues(); // [ spot0, var0, spot1, var1, ... ], array length 2 * indices_.size()

    for (Size path = 0; path < size(); ++path) {
        auto p = gen->next();
        Array state = initialState;
        std::size_t date = 0;
        auto pos = positionInTimeGrid_.begin();
        ++pos;

	// Evolve the process on the refined time grid
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            Real t0 = timeGrid_[i];
            Real dt = timeGrid_[i+1] - t0;
	    const Array& dW = p.value[i];
	    state = quantoProcess->evolve(t0, state, dt, dW); // [ spot0, var0, spot1, var1, ... ]
	
	    // on the effective simulation dates populate the underlying paths
            if (i + 1 == *pos) {
	        for (Size j = 0; j < indices_.size(); ++j)
		  rvs[j][date]->data()[path] = state[2*j]; // spot at even locations in the state array
                ++date;
                ++pos;
            }
	}
    }
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

    for (Size i = 0; i < currencies_.size(); ++i) {
        if (i > 0) {
	    std::ostringstream o;
            o << "fxSpot." << currencies_[i];
            additionalResults_[o.str()] = fxSpots_[i - 1]->value();
        }
        for (auto const& d : effectiveSimulationDates_) {
	    std::ostringstream o;
            o << "discount." << currencies_[i] << "." << io::iso_date(d);
            additionalResults_[o.str()] = curves_[i]->discount(d);
        }
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

    for (Size i = 0; i < indices_.size(); ++i) {
        additionalResults_["Heston.theta_" + indices_[i].name()] = model_->hestonProcesses()[i]->theta();
        additionalResults_["Heston.kappa_" + indices_[i].name()] = model_->hestonProcesses()[i]->kappa();
        additionalResults_["Heston.sigma_" + indices_[i].name()] = model_->hestonProcesses()[i]->sigma();
        additionalResults_["Heston.rho_" + indices_[i].name()] = model_->hestonProcesses()[i]->rho();
        additionalResults_["Heston.v0_" + indices_[i].name()] = model_->hestonProcesses()[i]->v0();
    }
    
    for (Size i = 0; i < indices_.size(); ++i)
        additionalResults_["Heston.Index[" + to_string(i) + "]"] = indices_[i].name();

    if (model_->calibration().size() > 0)
        additionalResults_["Heston.calibration"] = model_->calibration();

    if (debug_) {
        // copy path data
        MultiAssetHestonPaths paths;
        paths.samples = size();
        for (auto i : indices_)
            paths.indexNames.push_back(i.name());
        for (auto d : effectiveSimulationDates_) {
            paths.dates.push_back(d);
            paths.data[d] = std::vector<std::vector<Real>>(indices_.size(), std::vector<Real>(size(), 0.0));
            for (Size i = 0; i < indices_.size(); ++i) {
                for (Size j = 0; j < size(); ++j) {
                    paths.data[d][i][j] = underlyingPaths_[d][i][j];
                }
            }
        }
        additionalResults_["Heston.paths"] = paths;
    }
}

RandomVariable Heston::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                           const QuantLib::ext::optional<long>& memSlot, const RandomVariable& addRegressor1,
                           const RandomVariable& addRegressor2) const {
    RandomVariable result;

    QL_FAIL("Heston::npv not implemented");

    return result;
}

} // namespace data
} // namespace ore
