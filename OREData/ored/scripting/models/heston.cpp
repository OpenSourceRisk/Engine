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
#include <qle/methods/fdmblackscholesmesher.hpp>
#include <qle/methods/fdmblackscholesop.hpp>
#include <qle/methods/fdmhestonop.hpp>
#include <ql/methods/finitedifferences/meshers/fdmblackscholesmesher.hpp>
#include <ql/methods/finitedifferences/meshers/fdmblackscholesmultistrikemesher.hpp>
#include <ql/methods/finitedifferences/meshers/fdmhestonvariancemesher.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/methods/finitedifferences/operators/fdmlinearoplayout.hpp>
#include <ql/methods/finitedifferences/operators/fdmhestonop.hpp>
#include <ql/methods/finitedifferences/solvers/fdmhestonsolver.hpp>
#include <ql/methods/finitedifferences/stepconditions/fdmstepconditioncomposite.hpp>
#include <ql/methods/finitedifferences/utilities/fdminnervaluecalculator.hpp>
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
    if (model_->processType() == AssetModelWrapper::ProcessType::Heston)
        return model_->hestonProcesses()[indexNo]->s0()->value();
    else if (model_->processType() == AssetModelWrapper::ProcessType::PiecewiseHeston)
        return model_->ptdHestonProcesses()[indexNo]->model()->s0();
    else {
        QL_FAIL("process type " << static_cast<int>(model_->processType()) << " not covered");
    }
}

Real Heston::atmForward(const Size indexNo, const Real t) const {
    if (model_->processType() == AssetModelWrapper::ProcessType::Heston)
        return ore::data::atmForward(model_->hestonProcesses()[indexNo]->s0()->value(),
                                     model_->hestonProcesses()[indexNo]->riskFreeRate(),
                                     model_->hestonProcesses()[indexNo]->dividendYield(), t);
    else if (model_->processType() == AssetModelWrapper::ProcessType::PiecewiseHeston)
        return ore::data::atmForward(model_->ptdHestonProcesses()[indexNo]->model()->s0(),
                                     model_->ptdHestonProcesses()[indexNo]->model()->riskFreeRate(),
                                     model_->ptdHestonProcesses()[indexNo]->model()->dividendYield(), t);
    else {
        QL_FAIL("process type " << static_cast<int>(model_->processType()) << " not covered");
    }
}

Real Heston::compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const {
    if (model_->processType() == AssetModelWrapper::ProcessType::Heston) {
        const auto& p = model_->hestonProcesses().at(indexNo);
        return p->dividendYield()->discount(d1) / p->dividendYield()->discount(d2) /
               (p->riskFreeRate()->discount(d1) / p->riskFreeRate()->discount(d2));
    } else if (model_->processType() == AssetModelWrapper::ProcessType::PiecewiseHeston) {
        const auto& p = model_->ptdHestonProcesses().at(indexNo);
        return p->model()->dividendYield()->discount(d1) / p->model()->dividendYield()->discount(d2) /
               (p->model()->riskFreeRate()->discount(d1) / p->model()->riskFreeRate()->discount(d2));
    } else {
        QL_FAIL("process type " << static_cast<int>(model_->processType()) << " not covered");
    }
}

void Heston::performCalculationsMc() const {
    initUnderlyingPathsMc();
    setReferenceDateValuesMc();
    if (effectiveSimulationDates_.size() == 1)
        return;
    generatePaths();
}

void Heston::performCalculationsFd() const {
    DLOG("Heston::performCalculationsFd() called");
    QL_REQUIRE(model_->hestonProcesses().size() == 1, "a single Heston process with constant parameters is expected");
    auto process = model_->hestonProcesses()[0];
    
    // 0c if we only have one effective sim date (today), we set the underlying values = spot

    if (effectiveSimulationDates_.size() == 1) {
        underlyingValues_ = RandomVariable(size(), model_->generalizedBlackScholesProcesses()[0]->x0());
        return;
    }

    // 1a set the calibration strikes

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    // 1b set up the critical points for the mesher, same as for BS with local vol

    std::vector<std::vector<std::tuple<Real, Real, bool>>> cPoints;
    for (Size i = 0; i < indices_.size(); ++i) {
        cPoints.push_back(std::vector<std::tuple<Real, Real, bool>>());
        auto f = calibrationStrikes_.find(indices_[i].name());
        if (f != calibrationStrikes_.end()) {
            for (Size j = 0; j < std::min(f->second.size(), params_.mesherMaxConcentratingPoints); ++j) {
                cPoints.back().push_back(
                    QuantLib::ext::make_tuple(std::log(f->second[j]), params_.mesherConcentration, false));
                TLOG("added critical point at strike " << f->second[j] << " with concentration "
                                                       << params_.mesherConcentration);
            }
        }
    }

    // 2 set up mesher if we do not have one already or if we want to rebuild it every time

    const Time maturity = timeGrid_.back();

    if (mesher_ == nullptr || !params_.staticMesher) {

        // variance mesher
        const Size vGrid = 50; // FIXME: Add a "VarianceStateGridPoints" parameter
        const Size tGridMin = 5;
        const Size tGridAvgSteps = std::max(tGridMin, timeGrid_.size() / 50);
        const ext::shared_ptr<FdmHestonVarianceMesher> vMesher =
            ext::make_shared<FdmHestonVarianceMesher>(vGrid, process, maturity, tGridAvgSteps);

        // equity mesher
        const Size xGrid = 24; // FIXME: use StateGridPoints parameter
        ext::shared_ptr<QuantExt::FdmBlackScholesMesher> equityMesher;
        auto processHelper = QuantExt::FdmBlackScholesMesher::processHelper(
            process->s0(), process->dividendYield(), process->riskFreeRate(), vMesher->volaEstimate());
        QL_REQUIRE(calibrationStrikes.size() > 0, "empty calibration strikes");
        Real strike = calibrationStrikes[0] == Null<Real>() ? atmForward(0, timeGrid_.back()) : calibrationStrikes[0];
        equityMesher = QuantLib::ext::make_shared<QuantExt::FdmBlackScholesMesher>(
            xGrid, processHelper, maturity, strike, Null<Real>(), Null<Real>(), params_.mesherEpsilon,
            params_.mesherScaling, cPoints[0]);

        mesher_ = ext::make_shared<FdmMesherComposite>(equityMesher, vMesher);
    }

    // 3 set up operator using atmf vol and without discounting, floor forward variances at zero

    // FIXME: Keep this in analogy to the BS local vol case
    QuantLib::ext::shared_ptr<QuantExt::FdmQuantoHelper> quantoHelper;
    if (applyQuantoAdjustment_) {
        Real quantoCorr = quantoCorrelationMultiplier_ * getCorrelation()[0][1];
        quantoHelper = QuantLib::ext::make_shared<QuantExt::FdmQuantoHelper>(
            *curves_[quantoTargetCcyIndex_], *curves_[quantoSourceCcyIndex_],
            *model_->generalizedBlackScholesProcesses()[1]->blackVolatility(), quantoCorr, Null<Real>(),
            model_->generalizedBlackScholesProcesses()[1]->x0(), false, true);
    }

    // FIXME
    // Modify QuantExt:FdmHestonOp in analogy to QuantExt::FdmBlackScholesOp
    operator_ = QuantLib::ext::make_shared<QuantExt::FdmHestonOp>(
        mesher_, process, quantoHelper);

    // FIXME
    // Should we try to use FdmHestonSolver (as in fdhestonvanillaengine, i.e with
    // calculator, step conditions, boundaries, solver desc, ...) instead of using the
    // FdmBackwardSolver directly?
    // FdmHestonSolver uses FdmBackwardSolver internally adding bicubic spline interpolation, etc.
    std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>> boundaries;
    ext::shared_ptr<FdmStepConditionComposite> stepCondition = nullptr;
    //const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::Douglas();
    const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::Hundsdorfer();

    solver_ = QuantLib::ext::make_shared<FdmBackwardSolver>(operator_, boundaries, stepCondition, schemeDesc);

    // Size tGrid = timeGrid_.size();
    // Size dampingSteps = 0;
    // ext::shared_ptr<FdmInnerValueCalculator> calculator = nullptr;    
    // FdmSolverDesc solverDesc = { mesher_, boundaries, stepConditions,
    // 				 calculator, maturity,
    // 				 tGrid, dampingSteps };
    // auto solver = ext::make_shared<FdmHestonSolver>(
    //                 Handle<HestonProcess>(process),
    //                 solverDesc, schemeDesc,
    //                 Handle<FdmQuantoHelper>(quantoHelper));
    
    // 5 fill random variable with underlying values, these are valid for all times
    
    auto locations = mesher_->locations(0);
    underlyingValues_ = exp(RandomVariable(locations));

    // 6 set additional results

    setAdditionalResults();

    DLOG("Heston::performCalculationsFd() done");
}

void Heston::generatePaths() const {

    DLOG("Heston::generatePaths() called for " << indices_.size() << " indices:");
    for (auto i : indices_) {
        DLOG(i);
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

    /*
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
    */
    
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

    QuantLib::ext::shared_ptr<MultiAssetQuantoHestonProcess> quantoProcess;
    if (model_->processType() == AssetModelWrapper::ProcessType::Heston)
        quantoProcess =
            QuantLib::ext::make_shared<MultiAssetQuantoHestonProcess>(model_->hestonProcesses(), eqComIdx, C);
    else if (model_->processType() == AssetModelWrapper::ProcessType::PiecewiseHeston)
        quantoProcess =
            QuantLib::ext::make_shared<MultiAssetQuantoHestonProcess>(model_->ptdHestonProcesses(), eqComIdx, C);
    else {
        QL_FAIL("process type " << static_cast<int>(model_->processType()) << " not covered by the Heston class");
    }

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
    
    for (Size i = 0; i < indices_.size(); ++i)
        additionalResults_["Heston.Index[" + to_string(i) + "]"] = indices_[i].name();

    if (model_->calibration().size() > 0)
        additionalResults_["Heston.calibration"] = model_->calibration();

    if (debug_ && type_ == Model::Type::MC) {
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

} // namespace data
} // namespace ore
