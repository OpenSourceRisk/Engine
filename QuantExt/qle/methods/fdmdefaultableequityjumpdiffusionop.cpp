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

#include <qle/methods/fdmdefaultableequityjumpdiffusionop.hpp>

#include <ql/methods/finitedifferences/meshers/fdmmesher.hpp>
#include <ql/methods/finitedifferences/operators/secondderivativeop.hpp>

namespace QuantExt {

using namespace QuantLib;

FdmDefaultableEquityJumpDiffusionOp::FdmDefaultableEquityJumpDiffusionOp(
    const QuantLib::ext::shared_ptr<QuantLib::FdmMesher>& mesher,
    const QuantLib::ext::shared_ptr<DefaultableEquityJumpDiffusionModel>& model, const Size direction,
    const std::function<Real(Real, Real, Real)>& recovery, const Handle<QuantLib::YieldTermStructure>& discountingCurve,
    const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& addCreditCurve,
    const std::function<Real(Real, Real, Real)>& addRecovery)
    : mesher_(mesher), model_(model), direction_(direction), recovery_(recovery), discountingCurve_(discountingCurve),
      addCreditCurve_(addCreditCurve), addRecovery_(addRecovery), discountingSpread_(discountingSpread),
      dxMap_(FirstDerivativeOp(direction, mesher)), dxxMap_(SecondDerivativeOp(direction, mesher)),
      mapT_(direction, mesher), recoveryTerm_(mesher_->locations(direction).size()) {}

Size FdmDefaultableEquityJumpDiffusionOp::size() const { return 1ul; }

void FdmDefaultableEquityJumpDiffusionOp::setTime(Time t1, Time t2) {

    Size n = mesher_->locations(direction_).size();

    Real r = model_->r(t1);
    Real q = model_->q(t1);
    Real v = model_->sigma(t1) * model_->sigma(t1);

    Array h(n);
    for (Size i = 0; i < h.size(); ++i) {
        h[i] = model_->h(t1, std::exp(mesher_->locations(direction_)[i]));
    }

    // overwrite discounting term with external curve and / or add external spread

    Real r_dis = r;
    if (!discountingCurve_.empty()) {
        r_dis = discountingCurve_->forwardRate(t1, t2, Continuous);
    }
    if (!discountingSpread_.empty()) {
        r_dis += discountingSpread_->value();
    }

    // additional credit discounting term

    Array h2(n, 0.0);
    if (!addCreditCurve_.empty()) {
        QL_REQUIRE(!close_enough(addCreditCurve_->survivalProbability(t1), 0.0),
                   "FdmDefaultableEquityJumpDiffusionOp: addCreditCurve implies zero survival probability at t = "
                       << t1
                       << ", this can not be handled. Check the credit curve / security spread provided in the market "
                          "data. If this happens during a spread imply, the target price might not be ataainable even "
                          "for high spreads.");
        Real tmp =
            -std::log(addCreditCurve_->survivalProbability(t2) / addCreditCurve_->survivalProbability(t1)) / (t2 - t1);
        std::fill(h2.begin(), h2.end(), tmp);
    }

    Array drift(n, r - q - 0.5 * v);
    if (model_->adjustEquityForward())
        drift += model_->eta() * h;
    mapT_.axpyb(drift, dxMap_, dxxMap_.mult(Array(n, 0.5 * v)), -(Array(n, r_dis) + h + h2));

    for (Size i = 0; i < n; ++i) {
        Real S = std::exp(mesher_->locations(direction_)[i]);
        Real cr = conversionRatio_ ? conversionRatio_(S) : Null<Real>();
        recoveryTerm_[i] = 0.0;
        if (recovery_) {
            recoveryTerm_[i] += recovery_(t1, S, cr) * h[i];
        }
        if (addRecovery_) {
            recoveryTerm_[i] += addRecovery_(t1, S, cr) * h2[i];
        }
    }
}

Array FdmDefaultableEquityJumpDiffusionOp::apply(const Array& r) const {
    return mapT_.apply(r) + recoveryTerm_;
}

Array FdmDefaultableEquityJumpDiffusionOp::apply_mixed(const Array& r) const {
    Array retVal(r.size(), 0.0);
    return retVal;
}

Array FdmDefaultableEquityJumpDiffusionOp::apply_direction(Size direction, const Array& r) const {
    if (direction == direction_)
        return mapT_.apply(r);
    else {
        Array retVal(r.size(), 0.0);
        return retVal;
    }
}

Array FdmDefaultableEquityJumpDiffusionOp::solve_splitting(Size direction, const Array& r, Real s) const {
    if (direction == direction_) {
        return mapT_.solve_splitting(r, s, 1.0);
    } else {
        Array retVal(r);
        return retVal;
    }
}

Array FdmDefaultableEquityJumpDiffusionOp::preconditioner(const Array& r, Real s) const {
    return solve_splitting(direction_, r, s);
}

#if !defined(QL_NO_UBLAS_SUPPORT)
std::vector<QuantLib::SparseMatrix> FdmDefaultableEquityJumpDiffusionOp::toMatrixDecomp() const {
    std::vector<SparseMatrix> retVal(1, mapT_.toMatrix());
    return retVal;
}
#endif

void FdmDefaultableEquityJumpDiffusionOp::setConversionRatio(const std::function<Real(Real)>& conversionRatio) {
    conversionRatio_ = conversionRatio;
}

} // namespace QuantExt
