/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/termstructures/yield/ZeroSpreadedTermStructure.hpp>
#include <boost/make_shared.hpp>
#include <boost/date_time.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                                       const Handle<Quote>& securitySpread,
                                                       const Handle<Quote>& recoveryRate,
                                                       string timestepPeriod, Real timestepSize,
                                                       boost::optional<bool> includeSettlementDateFlows)
    : defaultCurve_(defaultCurve), securitySpread_(securitySpread), recoveryRate_(recoveryRate),
      timestepPeriod_(timestepPeriod), timestepSize_(timestepSize),
      includeSettlementDateFlows_(includeSettlementDateFlows) {
    discountCurve_ =
        Handle<YieldTermStructure>(boost::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread));
    registerWith(discountCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
}

void DiscountingRiskyBondEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    results_.valuationDate = (*discountCurve_)->referenceDate();
    results_.value = calculateNpv(results_.valuationDate);

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();
    // a bond's cashflow on settlement date is never taken into
    // account, so we might have to play it safe and recalculate
    // same parameters as above, we can avoid another call
    if (!includeRefDateFlows && results_.valuationDate == arguments_.settlementDate) {
        results_.settlementValue = results_.value;
    } else {
        // no such luck
        results_.settlementValue = calculateNpv(arguments_.settlementDate);
    }
}

Real DiscountingRiskyBondEngine::calculateNpv(Date npvDate) const {
    Real npvValue = 0;

    for (auto& cf : arguments_.cashflows) {
        if (cf->hasOccurred(npvDate, includeSettlementDateFlows_))
            continue;

        Probability S = defaultCurve_->survivalProbability(cf->date());
        npvValue += cf->amount() * S * discountCurve_->discount(cf->date());

        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(cf);
        if (coupon) {
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= npvDate && npvDate <= endDate) ? npvDate : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = defaultCurve_->defaultProbability(effectiveStartDate, endDate);

            npvValue += cf->amount() * recoveryRate_->value() * P * discountCurve_->discount(defaultDate);
        }
    }

    if (arguments_.cashflows.size() == 1) {
        boost::shared_ptr<Redemption> redemption = boost::dynamic_pointer_cast<Redemption>(arguments_.cashflows[0]);
        if (redemption) {
            Date startDate = npvDate;
            while (startDate < redemption->date()) {
                Date stepDate = startDate + timestepSize_ * Months;
                Date endDate = (stepDate > redemption->date()) ? redemption->date() : stepDate;
                Date defaultDate = startDate + (endDate - startDate) / 2;
                Probability P = defaultCurve_->defaultProbability(startDate, endDate);

                npvValue += redemption->amount() * recoveryRate_->value() * P * discountCurve_->discount(defaultDate);
                startDate = stepDate;
            }
        }
    }

    return npvValue;
}
}