/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/methods/fdmlgmop.hpp>
#include <qle/models/lgmfdsolver.hpp>

#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/methods/finitedifferences/meshers/fdmsimpleprocess1dmesher.hpp>

namespace QuantExt {

LgmFdSolver::LgmFdSolver(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real maxTime,
                         const QuantLib::FdmSchemeDesc scheme, const Size stateGridPoints, const Size timeStepsPerYear,
                         const Real mesherEpsilon)
    : model_(model), maxTime_(maxTime), scheme_(scheme), stateGridPoints_(stateGridPoints),
      timeStepsPerYear_(timeStepsPerYear), mesherEpsilon_(mesherEpsilon) {
    mesher_ = QuantLib::ext::make_shared<FdmMesherComposite>(QuantLib::ext::make_shared<FdmSimpleProcess1dMesher>(
        stateGridPoints, QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(model->stateProcess()), maxTime_,
        timeStepsPerYear_, mesherEpsilon_));
    mesherLocations_ = RandomVariable(mesher_->locations(0));
    operator_ = QuantLib::ext::make_shared<QuantExt::FdmLgmOp>(
        mesher_, QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(model->stateProcess()));
    solver_ = QuantLib::ext::make_shared<FdmBackwardSolver>(
        operator_, std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>>(), nullptr, scheme_);
}

Size LgmFdSolver::gridSize() const { return stateGridPoints_; }

RandomVariable LgmFdSolver::stateGrid(Real) const { return mesherLocations_; }

const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& LgmFdSolver::model() const { return model_; }

RandomVariable LgmFdSolver::rollback(const RandomVariable& v, const Real t1, const Real t0, Size steps) const {
    if (QuantLib::close_enough(t0, t1) || v.deterministic())
        return v;
    QL_REQUIRE(t0 < t1, "LgmCFdSolver::rollback(): t0 (" << t0 << ") < t1 (" << t1 << ") required.");
    if (steps == Null<Size>())
        steps = std::max<Size>(1, static_cast<Size>(static_cast<double>(timeStepsPerYear_) * (t1 - t0) + 0.5));
    Array workingArray(v.size());
    v.copyToArray(workingArray);
    solver_->rollback(workingArray, t1, t0, steps, 0);
    if (QuantLib::close_enough(t0, 0.0)) {
        Array x = mesher_->locations(0);
        MonotonicCubicNaturalSpline interpolation(x.begin(), x.end(), workingArray.begin());
        interpolation.enableExtrapolation();
        return RandomVariable(gridSize(), interpolation(0.0));
    } else {
        return RandomVariable(workingArray);
    }
}

} // namespace QuantExt
