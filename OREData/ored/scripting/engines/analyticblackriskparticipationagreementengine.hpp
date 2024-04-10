/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/engines/analyticblackriskparticipationagreementengine.hpp
    \brief
*/

#pragma once

#include <ored/scripting/engines/riskparticipationagreementbaseengine.hpp>

#include <ql/indexes/swapindex.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

class AnalyticBlackRiskParticipationAgreementEngine : public RiskParticipationAgreementBaseEngine {
public:
    AnalyticBlackRiskParticipationAgreementEngine(
        const std::string& baseCcy, const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
        const std::map<std::string, Handle<Quote>>& fxSpots,
        const Handle<DefaultProbabilityTermStructure>& defaultCurve, const Handle<Quote>& recoveryRate,
        const Handle<SwaptionVolatilityStructure>& volatility, const QuantLib::ext::shared_ptr<SwapIndex>& swapIndexBase,
        const bool matchUnderlyingTenor, const Real reversion, const bool alwaysRecomputeOptionRepresentation,
        const Size maxGapDays = Null<Size>(), const Size maxDiscretisationPoints = Null<Size>());

private:
    Real protectionLegNpv() const override;

    const Handle<SwaptionVolatilityStructure> volatility_;
    const QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase_;
    const bool matchUnderlyingTenor_;
    const Real reversion_;
    const bool alwaysRecomputeOptionRepresentation_;
};

} // namespace data
} // namespace ore
