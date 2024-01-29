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

/*! \file fdmlgmop.hpp
    \brief finite difference operator LGM model

*/

#pragma once

#include <ql/methods/finitedifferences/operators/fdmlinearopcomposite.hpp>
#include <ql/methods/finitedifferences/operators/firstderivativeop.hpp>
#include <ql/methods/finitedifferences/operators/triplebandlinearop.hpp>
#include <ql/stochasticprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

class FdmLgmOp : public FdmLinearOpComposite {
public:
    FdmLgmOp(const ext::shared_ptr<FdmMesher>& mesher, const ext::shared_ptr<StochasticProcess1D>& process);

    Size size() const override;
    void setTime(Time t1, Time t2) override;

    Array apply(const Array& r) const override;
    Array apply_mixed(const Array& r) const override;
    Array apply_direction(Size direction, const Array& r) const override;
    Array solve_splitting(Size direction, const Array& r, Real s) const override;
    Array preconditioner(const Array& r, Real s) const override;

#if !defined(QL_NO_UBLAS_SUPPORT)
    std::vector<QuantLib::SparseMatrix> toMatrixDecomp() const override;
#endif
private:
    ext::shared_ptr<FdmMesher> mesher_;
    ext::shared_ptr<StochasticProcess1D> process_;
    FirstDerivativeOp dxMap_;
    TripleBandLinearOp dxxMap_;
    TripleBandLinearOp mapT_;
};
} // namespace QuantExt
