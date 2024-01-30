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

#include <ql/methods/finitedifferences/meshers/fdmmesher.hpp>
#include <ql/methods/finitedifferences/operators/fdmlinearoplayout.hpp>
#include <ql/methods/finitedifferences/operators/secondderivativeop.hpp>

namespace QuantExt {

FdmLgmOp::FdmLgmOp(const ext::shared_ptr<FdmMesher>& mesher, const ext::shared_ptr<StochasticProcess1D>& process)
    : mesher_(mesher), process_(process), dxMap_(FirstDerivativeOp(0, mesher)), dxxMap_(SecondDerivativeOp(0, mesher)),
      mapT_(0, mesher) {}

void FdmLgmOp::setTime(Time t1, Time t2) {
    Real v = process_->variance(t1, 0.0, t2 - t1) / (t2 - t1);
    mapT_.axpyb(Array(), dxMap_, dxxMap_.mult(0.5 * Array(mesher_->layout()->size(), v)), Array(1, 0));
}

Size FdmLgmOp::size() const { return 1u; }

Array FdmLgmOp::apply(const Array& u) const { return mapT_.apply(u); }

Array FdmLgmOp::apply_direction(Size direction, const Array& r) const {
    if (direction == 0)
        return mapT_.apply(r);
    else {
        Array retVal(r.size(), 0.0);
        return retVal;
    }
}

Array FdmLgmOp::apply_mixed(const Array& r) const {
    Array retVal(r.size(), 0.0);
    return retVal;
}

Array FdmLgmOp::solve_splitting(Size direction, const Array& r, Real dt) const {
    if (direction == 0)
        return mapT_.solve_splitting(r, dt, 1.0);
    else {
        Array retVal(r);
        return retVal;
    }
}

Array FdmLgmOp::preconditioner(const Array& r, Real dt) const { return solve_splitting(0, r, dt); }

#if !defined(QL_NO_UBLAS_SUPPORT)
std::vector<QuantLib::SparseMatrix> FdmLgmOp::toMatrixDecomp() const {
    std::vector<QuantLib::SparseMatrix> retVal(1, mapT_.toMatrix());
    return retVal;
}
#endif
} // namespace QuantExt
