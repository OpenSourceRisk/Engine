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

/*! \file ored/scripting/engines/riskparticipationagreementbaseengine.hpp
    \brief
*/

#pragma once

#include <qle/instruments/riskparticipationagreement.hpp>

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

    class RiskParticipationAgreementBaseEngine : public QuantExt::RiskParticipationAgreement::engine {
public:
    RiskParticipationAgreementBaseEngine(const std::string& baseCcy,
                                         const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
                                         const std::map<std::string, Handle<Quote>>& fxSpots,
                                         const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                         const Handle<Quote>& recoveryRate, const Size maxGapDays = Null<Size>(),
                                         const Size maxDiscretizsationPoints = Null<Size>());

    static std::vector<Date> buildDiscretisationGrid(const Date& referenceDate, const Date& protectionStart,
                                                     const Date& protectionEnd, const std::vector<Leg>& underlying,
                                                     const Size maxGapDays = Null<Size>(),
                                                     const Size maxDiscretisationPoints = Null<Size>());

protected:
    virtual Real protectionLegNpv() const = 0;

    void calculate() const override;

    std::string baseCcy_;
    mutable std::map<std::string, Handle<YieldTermStructure>> discountCurves_;
    mutable std::map<std::string, Handle<Quote>> fxSpots_;
    Handle<DefaultProbabilityTermStructure> defaultCurve_;
    Handle<Quote> recoveryRate_;
    Size maxGapDays_, maxDiscretisationPoints_;

    // set by base engine, may be used by derived engines
    mutable std::vector<Date> gridDates_;
    mutable Date referenceDate_;
    mutable Real effectiveRecoveryRate_;
};

} // namespace data
} // namespace ore
