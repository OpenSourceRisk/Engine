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
 Copyright (C) 2008 Roland Stamm
 Copyright (C) 2009 Jose Aparicio

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

#include <qle/pricingengines/blackindexcdsoptionengine.hpp>

#include <ql/exercise.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

BlackIndexCdsOptionEngine::BlackIndexCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                                     Real recoveryRate, const Handle<YieldTermStructure>& termStructure,
                                                     const Handle<BlackVolTermStructure>& volatility)
    : BlackCdsOptionEngineBase(termStructure, volatility), probability_(probability), recoveryRate_(recoveryRate),
      useUnderlyingCurves_(false) {
    registerWith(probability_);
    registerWith(termStructure_);
    registerWith(volatility_);
}

BlackIndexCdsOptionEngine::BlackIndexCdsOptionEngine(
    const std::vector<Handle<DefaultProbabilityTermStructure> >& underlyingProbability,
    const std::vector<Real>& underlyingRecoveryRate, const Handle<YieldTermStructure>& termStructure,
    const Handle<BlackVolTermStructure>& volatility)
    : BlackCdsOptionEngineBase(termStructure, volatility), underlyingProbability_(underlyingProbability),
      underlyingRecoveryRate_(underlyingRecoveryRate), useUnderlyingCurves_(true) {
    for (Size i = 0; i < underlyingProbability.size(); ++i)
        registerWith(underlyingProbability_[i]);
    registerWith(termStructure_);
    registerWith(volatility_);
}

Real BlackIndexCdsOptionEngine::defaultProbability(const Date& d) const {
    if (!useUnderlyingCurves_)
        return probability_->defaultProbability(d);
    Real sum = 0.0, sumNotional = 0.0;
    for (Size i = 0; i < underlyingProbability_.size(); ++i) {
        sum += underlyingProbability_[i]->defaultProbability(d) * arguments_.swap->underlyingNotionals()[i];
        sumNotional += arguments_.swap->underlyingNotionals()[i];
    }
    return sum / sumNotional;
}

Real BlackIndexCdsOptionEngine::recoveryRate() const {
    if (!useUnderlyingCurves_)
        return recoveryRate_;
    Real sum = 0.0, sumNotional = 0.0;
    for (Size i = 0; i < underlyingProbability_.size(); ++i) {
        sum += underlyingRecoveryRate_[i];
        sumNotional += arguments_.swap->underlyingNotionals()[i];
    }
    return sum / sumNotional;
}

void BlackIndexCdsOptionEngine::calculate() const {
    BlackCdsOptionEngineBase::calculate(*arguments_.swap, arguments_.exercise->dates().front(), arguments_.knocksOut,
                                        results_);
}

} // namespace QuantExt
