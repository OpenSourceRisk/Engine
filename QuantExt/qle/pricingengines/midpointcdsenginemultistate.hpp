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

/*! \file qle/pricingengines/midpointcdsenginemultistate.hpp
    \brief Risky Bond Engine
    \ingroup engines
*/

#pragma once

#include <ql/pricingengines/credit/midpointcdsengine.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! The engine will compute a price w.r.t. each given default curve and
  store them as a vector in an additional result "stateNpv". In addition
  the engine computes a default value w.r.t. the given recovery, which
  is the last element of the stateNpv result. The usual NPV (i.e. results.value)
  is computed w.r.t. the default curve marked by the mainResultState. */
class MidPointCdsEngineMultiState : public QuantLib::CreditDefaultSwap::engine {
public:
    MidPointCdsEngineMultiState(const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves,
                                const std::vector<Handle<Quote>> recoveryRates,
                                const Handle<YieldTermStructure>& discountCurve, const Size mainResultState,
                                const boost::optional<bool> includeSettlementDateFlows = boost::none);

    void calculate() const;
    Real calculateNpv(const Size state) const;
    Real calculateDefaultValue() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; };
    const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves() const { return defaultCurves_; };
    const std::vector<Handle<Quote>>& recoveryRates() const { return recoveryRates_; };

private:
    const Handle<YieldTermStructure> discountCurve_;
    const std::vector<Handle<DefaultProbabilityTermStructure>> defaultCurves_;
    const Size mainResultState_;
    const std::vector<Handle<Quote>> recoveryRates_;
    const boost::optional<bool> includeSettlementDateFlows_;
};

} // namespace QuantExt
