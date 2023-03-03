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

/*! \file qle/termstructures/implieddefaulttermstructure.hpp
    \brief implied default term structure
    \ingroup models
*/

#pragma once

#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

class ImpliedDefaultTermStructure : public SurvivalProbabilityStructure {
public:
    ImpliedDefaultTermStructure(const Handle<DefaultProbabilityTermStructure>& t, const Date& referenceDate)
        : SurvivalProbabilityStructure(referenceDate), t_(t) {
        timeOffset_ = t->timeFromReference(referenceDate);
        registerWith(t_);
        enableExtrapolation(t_->allowsExtrapolation());
    }
    DayCounter dayCounter() const { return t_->dayCounter(); }
    Calendar calendar() const { return t_->calendar(); }
    Natural settlementDays() const { return t_->settlementDays(); }
    Date maxDate() const { return t_->maxDate(); }
    const std::vector<Date>& jumpDates() const { return t_->jumpDates(); }
    const std::vector<Time>& jumpTimes() const { return t_->jumpTimes(); }

private:
    Probability survivalProbabilityImpl(Time t) const {
        return t_->survivalProbability(t + timeOffset_) / t_->survivalProbability(t);
    }
    Handle<DefaultProbabilityTermStructure> t_;
    Real timeOffset_;
};
} // namespace QuantExt
