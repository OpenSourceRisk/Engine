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

#include <ql/event.hpp>
#include <qle/instruments/equityautodeltahedgedoption.hpp>

using namespace QuantLib;

namespace QuantExt {

EquityAutoDeltaHedgedOption::EquityAutoDeltaHedgedOption(const std::vector<UnderlyingOptionBatch>& batches, Real hedgingVolatility,
                                                         Real driftRate, const Date& observationStartDate,
                                                         const std::string& equityName, const Currency& currency)
    : batches_(batches), hedgingVol_(hedgingVolatility), driftRate_(driftRate),
      observationStartDate_(observationStartDate), equityName_(equityName), currency_(currency) {
    QL_REQUIRE(!batches_.empty(), "EquityAutoDeltaHedgedOption: no option batches specified");
    QL_REQUIRE(hedgingVol_ >= 0.0, "EquityAutoDeltaHedgedOption: hedging volatility must be non-negative");
}

bool EquityAutoDeltaHedgedOption::isExpired() const {
    Date latestExpiry = Date::minDate();
    for (const auto& b : batches_)
        latestExpiry = std::max(latestExpiry, b.expiryDate);
    return detail::simple_event(latestExpiry).hasOccurred();
}

void EquityAutoDeltaHedgedOption::setupArguments(PricingEngine::arguments* args) const {
    auto* arguments = dynamic_cast<EquityAutoDeltaHedgedOption::arguments*>(args);
    QL_REQUIRE(arguments != nullptr, "wrong argument type in EquityAutoDeltaHedgedOption");
    arguments->batches = batches_;
    arguments->hedgingVolatility = hedgingVol_;
    arguments->driftRate = driftRate_;
    arguments->observationStartDate = observationStartDate_;
    arguments->equityName = equityName_;
    arguments->currency = currency_;
}

void EquityAutoDeltaHedgedOption::arguments::validate() const {
    QL_REQUIRE(!batches.empty(), "EquityAutoDeltaHedgedOption::arguments: no batches");
    QL_REQUIRE(hedgingVolatility >= 0.0, "EquityAutoDeltaHedgedOption::arguments: negative hedging vol");
    for (Size i = 0; i < batches.size(); ++i) {
        QL_REQUIRE(batches[i].strike > 0.0,
                   "EquityAutoDeltaHedgedOption::arguments: non-positive strike for batch " << i);
        QL_REQUIRE(observationStartDate < batches[i].expiryDate,
                   "EquityAutoDeltaHedgedOption::arguments: observation start date "
                       << observationStartDate << " must be before expiry " << batches[i].expiryDate
                       << " for batch " << i);
    }
}

} // namespace QuantExt
