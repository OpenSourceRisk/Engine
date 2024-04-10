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

#include <ql/math/comparison.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>

namespace QuantExt {

CommoditySchwartzParametrization::CommoditySchwartzParametrization(const Currency& currency, const std::string& name,
                                                                   const Handle<QuantExt::PriceTermStructure>& priceCurve,
                                                                   const Handle<Quote>& fxSpotToday,
                                                                   const Real sigma, const Real kappa,
                                                                   bool driftFreeState)
    : Parametrization(currency, name), priceCurve_(priceCurve), fxSpotToday_(fxSpotToday),
      sigma_(QuantLib::ext::make_shared<PseudoParameter>(1)), kappa_(QuantLib::ext::make_shared<PseudoParameter>(1)),
      driftFreeState_(driftFreeState) {
    sigma_->setParam(0, inverse(0, sigma));
    kappa_->setParam(0, inverse(0, kappa));
}

Real CommoditySchwartzParametrization::VtT(Real t, Real T) {
    Real sig = sigmaParameter();
    Real kap = kappaParameter();
    if (fabs(kap) < QL_EPSILON)
        return sig * sig * (T-t);
    else
        return sig * sig * (1.0 - std::exp(-2.0 * kap * (T-t))) / (2.0 * kap);    
}

} // namespace QuantExt
