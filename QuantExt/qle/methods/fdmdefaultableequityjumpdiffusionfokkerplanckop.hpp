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

/*! \file qle/methods/fdmdefaultableequityjumpdiffusionfokkerplanckop.hpp */

#pragma once

#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>
#include <ql/methods/finitedifferences/operators/fdmlinearopcomposite.hpp>
#include <ql/methods/finitedifferences/operators/firstderivativeop.hpp>
#include <ql/methods/finitedifferences/operators/triplebandlinearop.hpp>

#include <functional>

namespace QuantExt {

using QuantLib::Array;
using QuantLib::Size;

class FdmDefaultableEquityJumpDiffusionFokkerPlanckOp : public QuantLib::FdmLinearOpComposite {
public:
    //! this op is implemented in terms of time to maturity T, so that we can use a backward solver to evolve
    FdmDefaultableEquityJumpDiffusionFokkerPlanckOp(
        const Real T, const QuantLib::ext::shared_ptr<QuantLib::FdmMesher>& mesher,
        const QuantLib::ext::shared_ptr<const DefaultableEquityJumpDiffusionModel>& model, const Size direction = 0);

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
    Real T_;
    QuantLib::ext::shared_ptr<QuantLib::FdmMesher> mesher_;
    QuantLib::ext::shared_ptr<const DefaultableEquityJumpDiffusionModel> model_;
    Size direction_;

    QuantLib::FirstDerivativeOp dxMap_;
    QuantLib::TripleBandLinearOp dxxMap_;
    QuantLib::TripleBandLinearOp mapT_;

    Array y_;
};

} // namespace QuantExt
