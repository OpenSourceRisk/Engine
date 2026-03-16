/*
 Copyright (C) 2021 Quaternion Risk Management Ltd

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

/*! \file qle/methods/fdmdefaultableequityjumpdiffusionop.hpp */

#pragma once

#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>
#include <ql/methods/finitedifferences/operators/fdmlinearopcomposite.hpp>
#include <ql/methods/finitedifferences/operators/firstderivativeop.hpp>
#include <ql/methods/finitedifferences/operators/triplebandlinearop.hpp>

#include <functional>

namespace QuantExt {

using QuantLib::Array;
using QuantLib::Size;

class FdmDefaultableEquityJumpDiffusionOp : public QuantLib::FdmLinearOpComposite {
public:
    /*! - The recovery is given as a function of (t, S, conversionRatio)
        - The model rate r can be overwritten with a discountingCurve, this is used in the discounting
          term of the operator then, but not the drift term. This can e.g. be bond discounting curve in
          convertible bond pricings.
        - An additional credit curve and associated recovery rate function can be specified, which will constitute
          an additional discounting term and recovery term. This can e.g. be the bond credit curve for exchangeable
          convertible bonds (in this context, the model credit curve will be the equity credit curve then).
     */
    FdmDefaultableEquityJumpDiffusionOp(
        const QuantLib::ext::shared_ptr<QuantLib::FdmMesher>& mesher,
        const QuantLib::ext::shared_ptr<DefaultableEquityJumpDiffusionModel>& model, const Size direction = 0,
        const std::function<Real(Real, Real, Real)>& recovery = {},
        const Handle<QuantLib::YieldTermStructure>& discountingCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& addCreditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const std::function<Real(Real, Real, Real)>& addRecovery = {});

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

    // conversion ratio as a function of S, used to evaluate the recovery term
    void setConversionRatio(const std::function<Real(Real)>& conversionRatio);

private:
    QuantLib::ext::shared_ptr<QuantLib::FdmMesher> mesher_;
    QuantLib::ext::shared_ptr<DefaultableEquityJumpDiffusionModel> model_;
    Size direction_;
    std::function<Real(Real, Real, Real)> recovery_;
    Handle<QuantLib::YieldTermStructure> discountingCurve_;
    Handle<QuantLib::DefaultProbabilityTermStructure> addCreditCurve_;
    std::function<Real(Real, Real, Real)> addRecovery_;
    Handle<QuantLib::Quote> discountingSpread_;

    QuantLib::FirstDerivativeOp dxMap_;
    QuantLib::TripleBandLinearOp dxxMap_;
    QuantLib::TripleBandLinearOp mapT_;
    Array recoveryTerm_;

    std::function<Real(Real)> conversionRatio_;
};

} // namespace QuantExt
