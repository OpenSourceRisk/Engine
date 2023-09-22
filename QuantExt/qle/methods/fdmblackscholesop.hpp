/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file fdmblackscholesop.hpp
    \brief extended version of the QuantLib class, see the documentation for details

*/

#pragma once

#include <qle/methods/fdmquantohelper.hpp>

#include <ql/methods/finitedifferences/operators/fdmlinearopcomposite.hpp>
#include <ql/methods/finitedifferences/operators/firstderivativeop.hpp>
#include <ql/methods/finitedifferences/operators/triplebandlinearop.hpp>
#include <ql/payoff.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

/* Black Scholes linear operator as in QuantLib. In addition, if strike = null the volatility
   at the atmf level is used, more precisely, a forward variance is calculated as

           V(t1, t2, atmf) = V(t2, k2) - V(t1, k1)

   with k1 and k2 the respective atmf levels at t1 and t2.

   The discounting term -r dt can be suppressed setting discounting = false.

   If ensureNonNegativeForwardVariance is true, the forward variances from the input vol ts are floored at zero. */
class FdmBlackScholesOp : public FdmLinearOpComposite {
public:
    FdmBlackScholesOp(const ext::shared_ptr<FdmMesher>& mesher,
                      const ext::shared_ptr<GeneralizedBlackScholesProcess>& process, Real strike = Null<Real>(),
                      bool localVol = false, Real illegalLocalVolOverwrite = -Null<Real>(), Size direction = 0,
                      const ext::shared_ptr<FdmQuantoHelper>& quantoHelper = ext::shared_ptr<FdmQuantoHelper>(),
                      const bool discounting = true, const bool ensureNonNegativeForwardVariance = false);

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
    const ext::shared_ptr<FdmMesher> mesher_;
    const ext::shared_ptr<YieldTermStructure> rTS_, qTS_;
    const ext::shared_ptr<BlackVolTermStructure> volTS_;
    const ext::shared_ptr<LocalVolTermStructure> localVol_;
    const Array x_;
    const FirstDerivativeOp dxMap_;
    const TripleBandLinearOp dxxMap_;
    TripleBandLinearOp mapT_;
    const Real strike_;
    const Real illegalLocalVolOverwrite_;
    const Size direction_;
    const ext::shared_ptr<FdmQuantoHelper> quantoHelper_;
    const Real initialValue_;
    const bool discounting_;
    const bool ensureNonNegativeForwardVariance_;
};
} // namespace QuantExt
