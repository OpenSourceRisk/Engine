/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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
#include <qle/models/commodityschwartzconstantparametrization.hpp>

namespace QuantExt {

CommoditySchwartzConstantParametrization::CommoditySchwartzConstantParametrization(const Currency& currency, const std::string& name,
                                                                   const Handle<QuantExt::PriceTermStructure>& priceCurve,
                                                                   const Handle<Quote>& fxSpotToday,
                                                                   const Real sigma, const Real kappa, const Real a,
                                                                   bool driftFreeState)
    : CommoditySchwartzParametrization(currency, name, priceCurve, fxSpotToday, driftFreeState),
      sigma_(QuantLib::ext::make_shared<PseudoParameter>(1)), 
      kappa_(QuantLib::ext::make_shared<PseudoParameter>(1)), 
      a_(QuantLib::ext::make_shared<PseudoParameter>(1)) {
    sigma_->setParam(0, inverse(0, sigma));
    kappa_->setParam(0, inverse(0, kappa));
    a_->setParam(0, inverse(0, a));
}

Real CommoditySchwartzConstantParametrization::VtT(Real t, Real T) {
    Real sig = sigmaParameter();
    Real kap = kappaParameter();
    Real season = m(T);
    if (fabs(kap) < QL_EPSILON)
        return sig * sig *  season * season * (T-t);
    else
        return sig * sig * season * season * (1.0 - std::exp(-2.0 * kap * (T-t))) / (2.0 * kap);
}

} // namespace QuantExt
