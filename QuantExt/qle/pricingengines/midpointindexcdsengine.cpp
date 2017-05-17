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

/*
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008, 2009 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/midpointindexcdsengine.hpp>

namespace QuantExt {

MidPointIndexCdsEngine::MidPointIndexCdsEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                               Real recoveryRate, const Handle<YieldTermStructure>& discountCurve,
                                               boost::optional<bool> includeSettlementDateFlows)
    : MidPointCdsEngineBase(discountCurve, includeSettlementDateFlows), probability_(probability),
      recoveryRate_(recoveryRate), useUnderlyingCurves_(false) {
    registerWith(discountCurve_);
    registerWith(probability_);
}

MidPointIndexCdsEngine::MidPointIndexCdsEngine(
    const std::vector<Handle<DefaultProbabilityTermStructure> >& underlyingProbability,
    const std::vector<Real>& underlyingRecoveryRate, const Handle<YieldTermStructure>& discountCurve,
    boost::optional<bool> includeSettlementDateFlows)
    : MidPointCdsEngineBase(discountCurve, includeSettlementDateFlows), underlyingProbability_(underlyingProbability),
      underlyingRecoveryRate_(underlyingRecoveryRate), useUnderlyingCurves_(true) {
    registerWith(discountCurve_);
    for (Size i = 0; i < underlyingProbability_.size(); ++i)
        registerWith(underlyingProbability_[i]);
}

Real MidPointIndexCdsEngine::survivalProbability(const Date& d) const {
    if (!useUnderlyingCurves_)
        return probability_->survivalProbability(d);
    Real sum = 0.0, sumNotional = 0.0;
    for (Size i = 0; i < underlyingProbability_.size(); ++i) {
        sum += underlyingProbability_[i]->survivalProbability(d) * arguments_.underlyingNotionals[i];
        sumNotional += arguments_.underlyingNotionals[i];
    }
    return sum / sumNotional;
}

Real MidPointIndexCdsEngine::defaultProbability(const Date& d1, const Date& d2) const {
    if (!useUnderlyingCurves_)
        return probability_->defaultProbability(d1, d2);
    Real sum = 0.0, sumNotional = 0.0;
    for (Size i = 0; i < underlyingProbability_.size(); ++i) {
        sum += underlyingProbability_[i]->defaultProbability(d1, d2) * arguments_.underlyingNotionals[i];
        sumNotional += arguments_.underlyingNotionals[i];
    }
    return sum / sumNotional;
}

Real MidPointIndexCdsEngine::recoveryRate() const {
    if (!useUnderlyingCurves_)
        return recoveryRate_;
    Real sum = 0.0, sumNotional = 0.0;
    for (Size i = 0; i < underlyingProbability_.size(); ++i) {
        sum += underlyingRecoveryRate_[i] * arguments_.underlyingNotionals[i];
        sumNotional += arguments_.underlyingNotionals[i];
    }
    return sum / sumNotional;
}

void MidPointIndexCdsEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "no discount term structure set");
    Date refDate;
    if (useUnderlyingCurves_) {
        QL_REQUIRE(underlyingProbability_.size() == arguments_.underlyingNotionals.size(),
                   "number of underlyings (" << arguments_.underlyingNotionals.size()
                                             << ") does not match number of curves (" << underlyingProbability_.size()
                                             << ")");
        for (Size i = 0; i < underlyingProbability_.size(); ++i)
            QL_REQUIRE(!underlyingProbability_.empty(), "no probability term structure set for underlying " << i);
        refDate = underlyingProbability_.front()->referenceDate();
    } else {
        QL_REQUIRE(!probability_.empty(), "no probability term structure set");
        refDate = probability_->referenceDate();
    }

    MidPointCdsEngineBase::calculate(refDate, arguments_, results_);
}
} // namespace QuantExt
