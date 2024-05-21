/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/methods/fdmdefaultableequityjumpdiffusionfokkerplanckop.hpp>

#include <ql/methods/finitedifferences/meshers/fdmmesher.hpp>
#include <ql/methods/finitedifferences/operators/secondderivativeop.hpp>

namespace QuantExt {

using namespace QuantLib;

FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::FdmDefaultableEquityJumpDiffusionFokkerPlanckOp(
    const Real T, const QuantLib::ext::shared_ptr<QuantLib::FdmMesher>& mesher,
    const QuantLib::ext::shared_ptr<const DefaultableEquityJumpDiffusionModel>& model, const Size direction)
    : T_(T), mesher_(mesher), model_(model), direction_(direction), dxMap_(FirstDerivativeOp(direction, mesher)),
      dxxMap_(SecondDerivativeOp(direction, mesher)), mapT_(direction, mesher), y_(mesher_->locations(direction_)) {}

Size FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::size() const { return 1ul; }

void FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::setTime(Time t1, Time t2) {

    Size n = mesher_->locations(direction_).size();

    Real r = model_->r(T_ - t1);
    Real q = model_->q(T_ - t1);
    Real v = model_->sigma(T_ - t1) * model_->sigma(T_ - t1);

    Array h = Array(n);
    for (Size i = 0; i < h.size(); ++i) {
        Real S = std::exp(y_[i]);
        h[i] = model_->h(T_ - t1, S);
    }

    mapT_.axpyb(-(Array(n, r - q - 0.5 * v) + model_->eta() * h), dxMap_, dxxMap_.mult(Array(n, 0.5 * v)),
                -(Array(n, r) + h * (1.0 - model_->p())));
}

Array FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::apply(const Array& r) const {
    return mapT_.apply(r);
}

Array FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::apply_mixed(const Array& r) const {
    Array retVal(r.size(), 0.0);
    return retVal;
}

Array FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::apply_direction(Size direction,
                                                                                   const Array& r) const {
    if (direction == direction_)
        return mapT_.apply(r);
    else {
        Array retVal(r.size(), 0.0);
        return retVal;
    }
}

Array FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::solve_splitting(Size direction, const Array& r,
                                                                                   Real s) const {
    if (direction == direction_)
        return mapT_.solve_splitting(r, s, 1.0);
    else {
        Array retVal(r);
        return retVal;
    }
}

Array FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::preconditioner(const Array& r, Real s) const {
    return solve_splitting(direction_, r, s);
}

#if !defined(QL_NO_UBLAS_SUPPORT)
std::vector<QuantLib::SparseMatrix>
FdmDefaultableEquityJumpDiffusionFokkerPlanckOp::toMatrixDecomp() const {
    std::vector<SparseMatrix> retVal(1, mapT_.toMatrix());
    return retVal;
}
#endif

} // namespace QuantExt
