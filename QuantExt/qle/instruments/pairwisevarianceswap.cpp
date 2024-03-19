/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/instruments/pairwisevarianceswap.hpp>
#include <ql/event.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

PairwiseVarianceSwap::PairwiseVarianceSwap(Position::Type position, Real strike1, Real strike2, Real basketStrike,
                                           Real notional1, Real notional2, Real basketNotional, Real cap, Real floor,
                                           Real payoffLimit, int accrualLag, Schedule valuationSchedule,
                                           Schedule laggedValuationSchedule, Date settlementDate)
    : position_(position), strike1_(strike1), strike2_(strike2), basketStrike_(basketStrike), notional1_(notional1),
      notional2_(notional2), basketNotional_(basketNotional), cap_(cap), floor_(floor), payoffLimit_(payoffLimit),
      accrualLag_(accrualLag), valuationSchedule_(valuationSchedule), laggedValuationSchedule_(laggedValuationSchedule),
      settlementDate_(settlementDate) {}

Real PairwiseVarianceSwap::variance1() const {
    calculate();
    QL_REQUIRE(variance1_ != Null<Real>(), "result not available");
    return variance1_;
}

Real PairwiseVarianceSwap::variance2() const {
    calculate();
    QL_REQUIRE(variance2_ != Null<Real>(), "result not available");
    return variance2_;
}

Real PairwiseVarianceSwap::basketVariance() const {
    calculate();
    QL_REQUIRE(basketVariance_ != Null<Real>(), "result not available");
    return basketVariance_;
}

void PairwiseVarianceSwap::setupArguments(PricingEngine::arguments* args) const {
    auto* arguments = dynamic_cast<PairwiseVarianceSwap::arguments*>(args);
    QL_REQUIRE(arguments != nullptr, "wrong argument type");

    arguments->position = position_;
    arguments->strike1 = strike1_;
    arguments->strike2 = strike2_;
    arguments->basketStrike = basketStrike_;
    arguments->notional1 = notional1_;
    arguments->notional2 = notional2_;
    arguments->basketNotional = basketNotional_;
    arguments->cap = cap_;
    arguments->floor = floor_;
    arguments->payoffLimit = payoffLimit_;
    arguments->accrualLag = accrualLag_;
    arguments->valuationSchedule = valuationSchedule_;
    arguments->laggedValuationSchedule = laggedValuationSchedule_;
    arguments->settlementDate = settlementDate_;
}

void PairwiseVarianceSwap::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const auto* results = dynamic_cast<const PairwiseVarianceSwap::results*>(r);
    variance1_ = results->variance1;
    variance2_ = results->variance2;
    basketVariance_ = results->basketVariance;
}

void PairwiseVarianceSwap::arguments::validate() const {
    QL_REQUIRE(strike1 != Null<Real>(), "no strike given for first underlying");
    QL_REQUIRE(strike1 > 0.0, "negative or null strike given for first underlying");
    QL_REQUIRE(strike2 != Null<Real>(), "no strike given for second underlying");
    QL_REQUIRE(strike2 > 0.0, "negative or null strike given for second underlying");
    QL_REQUIRE(basketStrike != Null<Real>(), "no strike given for basket");
    QL_REQUIRE(basketStrike > 0.0, "negative or null strike given for basket");

    QL_REQUIRE(notional1 != Null<Real>(), "no notional given for first underlying");
    QL_REQUIRE(notional1 > 0.0, "negative or null notional given for first underlying");
    QL_REQUIRE(notional2 != Null<Real>(), "no notional given for second underlying");
    QL_REQUIRE(notional2 > 0.0, "negative or null notional given for second underlying");
    QL_REQUIRE(basketNotional != Null<Real>(), "no notional given for basket");
    QL_REQUIRE(basketNotional > 0.0, "negative or null notional given for basket");

    QL_REQUIRE(cap != Null<Real>(), "no cap given");
    QL_REQUIRE(cap >= 0.0, "cap must be non-negative");
    QL_REQUIRE(floor != Null<Real>(), "no floor given");
    QL_REQUIRE(floor >= 0.0, "floor must be non-negative");
    QL_REQUIRE(payoffLimit != Null<Real>(), "no payoff limit given");
    QL_REQUIRE(payoffLimit > 0.0, "payoff limit must be non-negative");
    QL_REQUIRE(accrualLag != Null<int>(), "no accrual lag given");

    QL_REQUIRE(!valuationSchedule.empty(), "no valuation schedule given");
    QL_REQUIRE(!laggedValuationSchedule.empty(), "no lagged valuation schedule given");
    QL_REQUIRE(valuationSchedule.dates().size() == laggedValuationSchedule.dates().size(),
               "valuation schedule and lagged valuation schedule must have the same size");
    QL_REQUIRE(settlementDate != Date(), "null settlement date given");
}

bool PairwiseVarianceSwap::isExpired() const { return detail::simple_event(settlementDate_).hasOccurred(); }

void PairwiseVarianceSwap::setupExpired() const {
    Instrument::setupExpired();
    variance1_ = variance2_ = basketVariance_ = Null<Real>();
}

} // namespace QuantExt
