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

#include <qle/pricingengines/accrualbondrepoengine.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/settings.hpp>

namespace QuantExt {

AccrualBondRepoEngine::AccrualBondRepoEngine(const bool includeSecurityLeg) : includeSecurityLeg_(includeSecurityLeg) {}

void AccrualBondRepoEngine::calculate() const {
    Date today = Settings::instance().evaluationDate();
    Real multiplier = arguments_.cashLegPays ? -1.0 : 1.0;
    Real cashLegAccrual = 0.0, cashLegNominal = 0.0, cashLegNpv = 0.0;
    auto cf = CashFlows::nextCashFlow(arguments_.cashLeg, false);
    if (cf != arguments_.cashLeg.end()) {
        auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(*cf);
        cashLegNominal = multiplier * cpn->nominal();
        cashLegAccrual = multiplier * CashFlows::accruedAmount(arguments_.cashLeg, false);
        cashLegNpv = cashLegNominal + cashLegAccrual;
    }
    Real securityLegNpv = -multiplier * arguments_.security->NPV() * arguments_.securityMultiplier;
    Real securityLegAccrual = -multiplier * arguments_.security->accruedAmount(today) / 100.0 *
                              arguments_.security->notional(today) * arguments_.securityMultiplier;
    results_.additionalResults["CashLegNominal"] = cashLegNominal;
    results_.additionalResults["CashLegAccrual"] = cashLegAccrual;
    results_.additionalResults["CashLegNPV"] = cashLegNpv;
    results_.additionalResults["SecurityQuantity"] = arguments_.securityMultiplier;
    results_.additionalResults["SecurityLegCleanNPV"] = securityLegNpv - securityLegAccrual;
    results_.additionalResults["SecurityLegAccrual"] = securityLegAccrual;
    results_.additionalResults["SecurityLegNPV"] = securityLegNpv;
    results_.value = cashLegNpv + (includeSecurityLeg_ ? securityLegNpv : 0.0);
}

} // namespace QuantExt
