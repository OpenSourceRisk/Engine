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

namespace QuantExt {
using namespace QuantLib;

/*! The engine takes a vector of default curves and recovery rates. For the given
    main result state it will produce the same results as the MidPointCdsEngine.
    In addition a result with label "stateNPV" is produced containing the NPV
    for each given default curve / recovery rate and an additional entry with
    a default value w.r.t. the last given recovery rate in the vector. */
class MidPointCdsEngineMultiState : public QuantLib::MidPointCdsEngine {
public:
    MidPointCdsEngineMultiState(const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves,
                                const std::vector<Handle<Quote>> recoveryRates,
                                const Handle<YieldTermStructure>& discountCurve, const Size mainResultState,
                                const boost::optional<bool> includeSettlementDateFlows = boost::none);

    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; };
    const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves() const { return defaultCurves_; };
    const std::vector<Handle<Quote>>& recoveryRates() const { return recoveryRates_; };

private:
    void linkCurves(Size i) const;
    Real calculateDefaultValue() const;

    std::vector<Handle<DefaultProbabilityTermStructure>> defaultCurves_;
    std::vector<Handle<Quote>> recoveryRates_;
    Size mainResultState_;
};

} // namespace QuantExt
