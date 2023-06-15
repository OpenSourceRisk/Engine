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

#include <qle/pricingengines/indexcdsoptionbaseengine.hpp>

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/utilities/time.hpp>

#include <numeric>

using namespace QuantLib;
using std::string;
using std::vector;

namespace QuantExt {

IndexCdsOptionBaseEngine::IndexCdsOptionBaseEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                                   Real recovery,
                                                   const Handle<YieldTermStructure>& discountSwapCurrency,
                                                   const Handle<YieldTermStructure>& discountTradeCollateral,
                                                   const Handle<QuantExt::CreditVolCurve>& volatility)
    : probabilities_({probability}), recoveries_({recovery}), discountSwapCurrency_(discountSwapCurrency),
      discountTradeCollateral_(discountTradeCollateral), volatility_(volatility), indexRecovery_(recovery) {
    registerWithMarket();
}

IndexCdsOptionBaseEngine::IndexCdsOptionBaseEngine(const vector<Handle<DefaultProbabilityTermStructure>>& probabilities,
                                                   const vector<Real>& recoveries,
                                                   const Handle<YieldTermStructure>& discountSwapCurrency,
                                                   const Handle<YieldTermStructure>& discountTradeCollateral,
                                                   const Handle<QuantExt::CreditVolCurve>& volatility,
                                                   Real indexRecovery)
    : probabilities_(probabilities), recoveries_(recoveries), discountSwapCurrency_(discountSwapCurrency),
      discountTradeCollateral_(discountTradeCollateral), volatility_(volatility), indexRecovery_(indexRecovery) {

    QL_REQUIRE(!probabilities_.empty(), "IndexCdsOptionBaseEngine: need at least one probability curve.");
    QL_REQUIRE(probabilities_.size() == recoveries_.size(), "IndexCdsOptionBaseEngine: mismatch between size"
                                                                << " of probabilities (" << probabilities_.size()
                                                                << ") and recoveries (" << recoveries_.size() << ").");

    registerWithMarket();

    // If the index recovery is not populated, use the average recovery.
    if (indexRecovery_ == Null<Real>())
        indexRecovery_ = accumulate(recoveries_.begin(), recoveries_.end(), 0.0) / recoveries_.size();
}

const vector<Handle<DefaultProbabilityTermStructure>>& IndexCdsOptionBaseEngine::probabilities() const {
    return probabilities_;
}

const vector<Real>& IndexCdsOptionBaseEngine::recoveries() const { return recoveries_; }

const Handle<YieldTermStructure> IndexCdsOptionBaseEngine::discountSwapCurrency() const {
    return discountSwapCurrency_;
}

const Handle<YieldTermStructure> IndexCdsOptionBaseEngine::discountTradeCollateral() const {
    return discountTradeCollateral_;
}

const Handle<QuantExt::CreditVolCurve> IndexCdsOptionBaseEngine::volatility() const { return volatility_; }

void IndexCdsOptionBaseEngine::calculate() const {

    // Underlying index CDS
    const auto& cds = *arguments_.swap;

    // If given constituent curves, store constituent notionals. Otherwise, store top level notional.
    if (probabilities_.size() > 1) {
        notionals_ = cds.underlyingNotionals();
        QL_REQUIRE(probabilities_.size() == notionals_.size(), "IndexCdsOptionBaseEngine: mismatch between size"
                                                                   << " of probabilities (" << probabilities_.size()
                                                                   << ") and notionals (" << notionals_.size() << ").");
    } else {
        notionals_ = {cds.notional()};
    }

    // Get additional results of underlying index CDS.
    cds.NPV();
    results_.additionalResults = cds.additionalResults();

    // call engine-specific calculation
    doCalc();
}

Real IndexCdsOptionBaseEngine::fep() const {

    // Exercise date
    const Date& exerciseDate = arguments_.exercise->dates().front();

    // Realised FEP
    results_.additionalResults["realisedFEP"] = arguments_.realisedFep;

    // Unrealised FEP
    Real fep = 0.0;
    for (Size i = 0; i < probabilities_.size(); ++i) {
        fep += (1 - recoveries_[i]) * probabilities_[i]->defaultProbability(exerciseDate) * notionals_[i];
    }
    results_.additionalResults["UnrealisedFEP"] = fep;

    // Total FEP
    fep += arguments_.realisedFep;
    results_.additionalResults["FEP"] = fep;

    // Discounted FEP
    fep *= discountTradeCollateral_->discount(exerciseDate);
    results_.additionalResults["discountedFEP"] = fep;

    return fep;
}

void IndexCdsOptionBaseEngine::registerWithMarket() {
    for (const auto& p : probabilities_)
        registerWith(p);
    registerWith(discountTradeCollateral_);
    registerWith(discountSwapCurrency_);
    registerWith(volatility_);
}

} // namespace QuantExt
