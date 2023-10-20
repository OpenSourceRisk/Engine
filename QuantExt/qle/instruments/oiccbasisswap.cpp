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

#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/instruments/oiccbasisswap.hpp>

namespace QuantExt {

OvernightIndexedCrossCcyBasisSwap::OvernightIndexedCrossCcyBasisSwap(
    Real payNominal, Currency payCurrency, const Schedule& paySchedule,
    const boost::shared_ptr<OvernightIndex>& payIndex, Real paySpread, Real recNominal, Currency recCurrency,
    const Schedule& recSchedule, const boost::shared_ptr<OvernightIndex>& recIndex, Real recSpread)
    : Swap(2), payNominal_(payNominal), recNominal_(recNominal), payCurrency_(payCurrency), recCurrency_(recCurrency),
      paySchedule_(paySchedule), recSchedule_(recSchedule), payIndex_(payIndex), recIndex_(recIndex),
      paySpread_(paySpread), recSpread_(recSpread), currency_(2) {

    registerWith(payIndex);
    registerWith(recIndex);
    initialize();
}

void OvernightIndexedCrossCcyBasisSwap::initialize() {
    legs_[0] = OvernightLeg(paySchedule_, payIndex_).withNotionals(payNominal_).withSpreads(paySpread_);
    legs_[0].insert(legs_[0].begin(),
                    boost::shared_ptr<CashFlow>(new SimpleCashFlow(-payNominal_, paySchedule_.dates().front())));
    legs_[0].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(payNominal_, paySchedule_.dates().back())));

    legs_[1] = OvernightLeg(recSchedule_, recIndex_).withNotionals(recNominal_).withSpreads(recSpread_);
    legs_[1].insert(legs_[1].begin(),
                    boost::shared_ptr<CashFlow>(new SimpleCashFlow(-recNominal_, recSchedule_.dates().front())));
    legs_[1].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(recNominal_, recSchedule_.dates().back())));

    for (Size j = 0; j < 2; ++j) {
        for (Leg::iterator i = legs_[j].begin(); i != legs_[j].end(); ++i)
            registerWith(*i);
    }

    payer_[0] = -1.0;
    payer_[1] = +1.0;

    currency_[0] = payCurrency_;
    currency_[1] = recCurrency_;
}

Spread OvernightIndexedCrossCcyBasisSwap::fairPayLegSpread() const {
    calculate();
    QL_REQUIRE(fairPayLegSpread_ != Null<Real>(), "result not available");
    return fairPayLegSpread_;
}

Spread OvernightIndexedCrossCcyBasisSwap::fairRecLegSpread() const {
    calculate();
    QL_REQUIRE(fairRecLegSpread_ != Null<Real>(), "result not available");
    return fairRecLegSpread_;
}

Real OvernightIndexedCrossCcyBasisSwap::payLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "result not available");
    return legBPS_[0];
}

Real OvernightIndexedCrossCcyBasisSwap::recLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[1] != Null<Real>(), "result not available");
    return legBPS_[1];
}

Real OvernightIndexedCrossCcyBasisSwap::payLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[0] != Null<Real>(), "result not available");
    return legNPV_[0];
}

Real OvernightIndexedCrossCcyBasisSwap::recLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[1] != Null<Real>(), "result not available");
    return legNPV_[1];
}

void OvernightIndexedCrossCcyBasisSwap::setupArguments(PricingEngine::arguments* args) const {
    Swap::setupArguments(args);

    OvernightIndexedCrossCcyBasisSwap::arguments* arguments =
        dynamic_cast<OvernightIndexedCrossCcyBasisSwap::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");

    arguments->currency = currency_;
    arguments->paySpread = paySpread_;
    arguments->recSpread = recSpread_;
}

void OvernightIndexedCrossCcyBasisSwap::fetchResults(const PricingEngine::results* r) const {
    Swap::fetchResults(r);

    const OvernightIndexedCrossCcyBasisSwap::results* results =
        dynamic_cast<const OvernightIndexedCrossCcyBasisSwap::results*>(r);
    QL_REQUIRE(results != 0, "wrong result type");

    fairRecLegSpread_ = results->fairRecLegSpread;
    fairPayLegSpread_ = results->fairPayLegSpread;
}

void OvernightIndexedCrossCcyBasisSwap::results::reset() {
    Swap::results::reset();
    fairPayLegSpread = Null<Real>();
    fairRecLegSpread = Null<Real>();
}
} // namespace QuantExt
