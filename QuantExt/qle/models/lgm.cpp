/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <iostream>
#include <ql/experimental/math/piecewiseintegral.hpp>
#include <qle/models/lgm.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

namespace QuantExt {

LinearGaussMarkovModel::LinearGaussMarkovModel(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization,
                                               const Measure measure, const Discretization discretization,
                                               const bool evaluateBankAccount,
                                               const QuantLib::ext::shared_ptr<Integrator>& integrator)
    : parametrization_(parametrization), measure_(measure), discretization_(discretization),
      evaluateBankAccount_(evaluateBankAccount) {
    QL_REQUIRE(parametrization_ != nullptr, "HwModel: parametrization is null");
    stateProcess_ = QuantLib::ext::make_shared<IrLgm1fStateProcess>(parametrization_);
    arguments_.resize(2);
    arguments_[0] = parametrization_->parameter(0);
    arguments_[1] = parametrization_->parameter(1);
    registerWith(parametrization_->termStructure());

    std::vector<Time> allTimes;
    for (Size i = 0; i < 2; ++i)
        allTimes.insert(allTimes.end(), parametrization_->parameterTimes(i).begin(),
                        parametrization_->parameterTimes(i).end());

    integrator_ = QuantLib::ext::make_shared<PiecewiseIntegral>(integrator, allTimes, true);
}

Real LinearGaussMarkovModel::bankAccountNumeraire(const Time t, const Real x, const Real y,
                                                  const Handle<YieldTermStructure> discountCurve) const {
    QL_REQUIRE(t >= 0.0, "t (" << t << ") >= 0 required in LGM::bankAccountNumeraire");
    Real Ht = parametrization_->H(t);
    Real zeta0 = parametrization_->zeta(t);
    Real zeta2 = parametrization_->zetan(2, t, integrator_);
    Real Vt = 0.5 * (Ht * Ht * zeta0 + zeta2);
    return std::exp(Ht * x - y + Vt) /
           (discountCurve.empty() ? parametrization_->termStructure()->discount(t) : discountCurve->discount(t));
}

Size LinearGaussMarkovModel::n() const { return 1; }
Size LinearGaussMarkovModel::m() const { return 1; }
Size LinearGaussMarkovModel::n_aux() const { return evaluateBankAccount_ && measure_ == Measure::BA ? 1 : 0; }
Size LinearGaussMarkovModel::m_aux() const {
    return evaluateBankAccount_ && measure_ == Measure::BA && discretization_ == Discretization::Exact ? 1 : 0;
}

} // namespace QuantExt
