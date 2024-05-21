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

#include <qle/models/commodityschwartzmodel.hpp>
#include <qle/processes/commodityschwartzstateprocess.hpp>

namespace QuantExt {

CommoditySchwartzModel::CommoditySchwartzModel(const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>& parametrization, 
                                               const Discretization discretization)
    : parametrization_(parametrization), discretization_(discretization) {
    QL_REQUIRE(parametrization_ != nullptr, "CommoditySchwartzModel: parametrization is null");
    arguments_.resize(2);
    arguments_[0] = parametrization_->parameter(0);
    arguments_[1] = parametrization_->parameter(1);
    stateProcess_ = QuantLib::ext::make_shared<CommoditySchwartzStateProcess>(parametrization_, discretization_);
}

QuantLib::Real CommoditySchwartzModel::forwardPrice(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& state,
                                                    const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve) const {
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in CommoditySchwartzModel::forwardPrice");
    Real f0T = priceCurve.empty() ? parametrization_->priceCurve()->price(T) : priceCurve->price(T);
    Real VtT = parametrization_->VtT(t, T);
    Real V0T = parametrization_->VtT(0, T);
    Real k = parametrization_->kappaParameter();
    if (parametrization_->driftFreeState())
        return f0T * std::exp(-state[0] * std::exp(-k*T) - 0.5 * (V0T - VtT));
    else
        return f0T * std::exp(-state[0] * std::exp(-k*(T-t)) - 0.5 * (V0T - VtT));
}

void CommoditySchwartzModel::update() {
    parametrization_->update();
    notifyObservers();
}

void CommoditySchwartzModel::generateArguments() { update(); }

} // namespace QuantExt
