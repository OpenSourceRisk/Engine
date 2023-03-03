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

#include <qle/pricingengines/discountingbondrepoengine.hpp>

#include <ql/cashflows/cashflows.hpp>

namespace QuantExt {

DiscountingBondRepoEngine::DiscountingBondRepoEngine(const Handle<YieldTermStructure>& repoCurve,
                                                     const bool includeSecurityLeg)
    : repoCurve_(repoCurve), includeSecurityLeg_(includeSecurityLeg) {}

void DiscountingBondRepoEngine::calculate() const {
    QL_REQUIRE(!repoCurve_.empty(), "DiscountingBondRepoEngine::calculate(): repoCurve_ is empty()");
    Real multiplier = arguments_.cashLegPays ? -1.0 : 1.0;
    Real cashLegNpv = multiplier * CashFlows::npv(arguments_.cashLeg, **repoCurve_, false);
    Real securityLegNpv = -multiplier * arguments_.security->NPV() * arguments_.securityMultiplier;
    results_.additionalResults["CashLegNPV"] = cashLegNpv;
    results_.additionalResults["SecurityLegNPV"] = securityLegNpv;
    results_.value = cashLegNpv + (includeSecurityLeg_ ? securityLegNpv : 0.0);
}

} // namespace QuantExt
