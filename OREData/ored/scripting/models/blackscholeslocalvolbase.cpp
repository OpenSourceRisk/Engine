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

#include <ored/scripting/models/blackscholeslocalvolbase.hpp>

#include <qle/methods/fdmblackscholesmesher.hpp>
#include <qle/methods/fdmblackscholesop.hpp>

#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

Real BlackScholesLocalVolBase::initialValue(const Size indexNo) const {
    return model_->generalizedBlackScholesProcesses()[indexNo]->x0();
}

Real BlackScholesLocalVolBase::atmForward(const Size indexNo, const Real t) const {
    return ore::data::atmForward(model_->generalizedBlackScholesProcesses()[indexNo]->x0(),
                                 model_->generalizedBlackScholesProcesses()[indexNo]->riskFreeRate(),
                                 model_->generalizedBlackScholesProcesses()[indexNo]->dividendYield(), t);
}

Real BlackScholesLocalVolBase::compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const {
    const auto& p = model_->generalizedBlackScholesProcesses().at(indexNo);
    return p->dividendYield()->discount(d1) / p->dividendYield()->discount(d2) /
           (p->riskFreeRate()->discount(d1) / p->riskFreeRate()->discount(d2));
}

void BlackScholesLocalVolBase::performCalculationsFd(const bool localVol) const {

    // 0c if we only have one effective sim date (today), we set the underlying values = spot

    if (effectiveSimulationDates_.size() == 1) {
        underlyingValues_ = RandomVariable(size(), model_->generalizedBlackScholesProcesses()[0]->x0());
        return;
    }

    // 1 set the calibration strikes

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    // 1b set up the critical points for the mesher

    std::vector<std::vector<std::tuple<Real, Real, bool>>> cPoints;
    for (Size i = 0; i < indices_.size(); ++i) {
        cPoints.push_back(std::vector<std::tuple<Real, Real, bool>>());
        auto f = calibrationStrikes_.find(indices_[i].name());
        if (f != calibrationStrikes_.end()) {
            for (Size j = 0; j < std::min(f->second.size(), params_.mesherMaxConcentratingPoints); ++j) {
                cPoints.back().push_back(
                    std::make_tuple(std::log(f->second[j]), params_.mesherConcentration, false));
                TLOG("added critical point at strike " << f->second[j] << " with concentration "
                                                       << params_.mesherConcentration);
            }
        }
    }

    // 2 set up mesher if we do not have one already or if we want to rebuild it every time

    if (mesher_ == nullptr || !params_.staticMesher) {
        mesher_ =
            QuantLib::ext::make_shared<FdmMesherComposite>(QuantLib::ext::make_shared<QuantExt::FdmBlackScholesMesher>(
                size(), model_->generalizedBlackScholesProcesses()[0], timeGrid_.back(),
                calibrationStrikes[0] == Null<Real>() ? atmForward(0, timeGrid_.back()) : calibrationStrikes[0],
                Null<Real>(), Null<Real>(), params_.mesherEpsilon, params_.mesherScaling, cPoints[0]));
    }

    // 3 set up operator using atmf vol and without discounting, floor forward variances at zero

    QuantLib::ext::shared_ptr<QuantExt::FdmQuantoHelper> quantoHelper;

    if (applyQuantoAdjustment_) {
        Real quantoCorr = quantoCorrelationMultiplier_ * getCorrelation()[0][1];
        quantoHelper = QuantLib::ext::make_shared<QuantExt::FdmQuantoHelper>(
            *curves_[quantoTargetCcyIndex_], *curves_[quantoSourceCcyIndex_],
            *model_->generalizedBlackScholesProcesses()[1]->blackVolatility(), quantoCorr, Null<Real>(),
            model_->generalizedBlackScholesProcesses()[1]->x0(), false, true);
    }

    operator_ = QuantLib::ext::make_shared<QuantExt::FdmBlackScholesOp>(
        mesher_, model_->generalizedBlackScholesProcesses()[0], calibrationStrikes[0], localVol, 1E-10, 0, quantoHelper,
        false, true);

    // 4 set up bwd solver, hardcoded Douglas scheme (= CrankNicholson)

    solver_ = QuantLib::ext::make_shared<FdmBackwardSolver>(
        operator_, std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>>(), nullptr,
        FdmSchemeDesc::Douglas());

    // 5 fill random variable with underlying values, these are valid for all times

    auto locations = mesher_->locations(0);
    underlyingValues_ = exp(RandomVariable(locations));

    // 6 set additional results

    setAdditionalResults(localVol);
}

void BlackScholesLocalVolBase::setAdditionalResults(const bool localVol) const {

    std::string label = localVol ? "LocalVol" : "BlackScholes";

    Matrix correlation = getCorrelation();

    for (Size i = 0; i < indices_.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            additionalResults_[label + ".Correlation_" + indices_[i].name() + "_" + indices_[j].name()] =
                correlation(i, j);
        }
    }

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_[label + ".CalibrationStrike_" + indices_[i].name()] =
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
                additionalResults_[label + ".Volatility_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                    volatility;
            }
            additionalResults_[label + ".Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] = forward;
            ++timeStep;
        }
    }
}

} // namespace data
} // namespace ore
