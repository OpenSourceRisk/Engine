/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/models/hwmodel.hpp>
#include <qle/processes/irhwstateprocess.hpp>

namespace QuantExt {

HwModel::HwModel(const QuantLib::ext::shared_ptr<IrHwParametrization>& parametrization, const Measure measure,
                 const Discretization discretization, const bool evaluateBankAccount)
    : parametrization_(parametrization), measure_(measure), discretization_(discretization),
      evaluateBankAccount_(evaluateBankAccount) {
    QL_REQUIRE(parametrization_ != nullptr, "HwModel: parametrization is null");
    stateProcess_ =
        QuantLib::ext::make_shared<IrHwStateProcess>(parametrization_, measure_, discretization_, evaluateBankAccount_);
}

QuantLib::Real HwModel::discountBond(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
                                     const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return 1.0;
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in HwModel::discountBond");
    QuantLib::Array gt = parametrization_->g(t, T);
    QuantLib::Matrix yt = parametrization_->y(t);

    return (discountCurve.empty()
                ? parametrization_->termStructure()->discount(T) / parametrization_->termStructure()->discount(t)
                : discountCurve->discount(T) / discountCurve->discount(t)) *
           std::exp(-QuantLib::DotProduct(gt, x) - 0.5 * QuantLib::DotProduct(gt, yt * gt));
}

QuantLib::Real HwModel::numeraire(const QuantLib::Time t, const QuantLib::Array& x,
                                  const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                  const QuantLib::Array& aux) const {
    QL_REQUIRE(measure_ == IrModel::Measure::BA, "HwModel::numeraire() supports BA measure only currently.");
    return std::exp(std::accumulate(aux.begin(), aux.end(), 0.0)) /
           (discountCurve.empty() ? parametrization_->termStructure()->discount(t) : discountCurve->discount(t));
}

QuantLib::Real HwModel::shortRate(const QuantLib::Time t, const QuantLib::Array& x,
                                  const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve) const {
    return std::accumulate(x.begin(), x.end(), 0.0) +
           (discountCurve.empty() ? parametrization_->termStructure()->forwardRate(t, t, Compounding::Continuous)
                                  : discountCurve->forwardRate(t, t, Compounding::Continuous));
}

void HwModel::update() {
    parametrization_->update();
    notifyObservers();
}

void HwModel::generateArguments() { update(); }

Size HwModel::n() const { return parametrization_->n(); }
Size HwModel::m() const { return parametrization_->m(); }
Size HwModel::n_aux() const { return evaluateBankAccount_ && measure_ == Measure::BA ? n() : 0; }
Size HwModel::m_aux() const {
    return evaluateBankAccount_ && measure_ == Measure::BA && discretization_ == Discretization::Exact ? m() : 0;
}

} // namespace QuantExt
