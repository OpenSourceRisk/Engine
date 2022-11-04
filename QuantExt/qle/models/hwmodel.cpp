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

HwModel::HwModel(const boost::shared_ptr<IrHwParametrization>& parametrization, const Measure measure,
                 const Discretization discretization)
    : parametrization_(parametrization), measure_(measure), discretization_(discretization) {
    stateProcess_ = boost::make_shared<IrHwStateProcess>(parametrization_, measure_, discretization_);
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
           std::exp(-QuantLib::DotProduct(gt, x) - 0.5 * QuantLib::DotProduct(gt, yt * x));
}

QuantLib::Real HwModel::numeraire(const QuantLib::Time t, const QuantLib::Array& x,
                                  const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                  const QuantLib::Array& aux) const {
    // TDDO
    QL_FAIL("HwModel::numeraire(): not impl.");
    return 1.0;
}

void HwModel::update() {
    parametrization_->update();
    notifyObservers();
}

void HwModel::generateArguments() { update(); }

} // namespace QuantExt
