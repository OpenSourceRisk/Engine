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

/*! \file ored/scripting/engines/numericlgmriskparticipationagreementengine.hpp
    \brief
*/

#pragma once

#include <ored/scripting/engines/riskparticipationagreementbaseengine.hpp>

#include <qle/models/lgmconvolutionsolver2.hpp>

#include <ql/termstructures/defaulttermstructure.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

class NumericLgmRiskParticipationAgreementEngine : public RiskParticipationAgreementBaseEngine,
                                                   protected QuantExt::LgmConvolutionSolver2 {
public:
    NumericLgmRiskParticipationAgreementEngine(const std::string& baseCcy,
                                               const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
                                               const std::map<std::string, Handle<Quote>>& fxSpots,
                                               const QuantLib::ext::shared_ptr<QuantExt::LinearGaussMarkovModel>& model, const Real sy,
                                               const Size ny, const Real sx, const Size nx,
                                               const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                               const Handle<Quote>& recoveryRate, const Size maxGapDays = Null<Size>(),
                                               const Size maxDiscretisationPoints = Null<Size>());

private:
    Real protectionLegNpv() const override;
};

} // namespace data
} // namespace ore
