/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/survivalprobabilitycurve.hpp
    \brief interpolated survival probability term structure
    \ingroup termstructures
*/

#pragma once

#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! DefaultProbabilityTermStructure based on yield curve
class SurvivalProbabilityCurveFromYield : public SurvivalProbabilityStructure {
public:
    SurvivalProbabilityCurveFromYield(const QuantLib::Handle<YieldTermStructure> yieldTermStructure,
                                      const QuantLib::Handle<QuantLib::Quote>& rr,
                                      const std::vector<Handle<Quote>>& jumps = std::vector<Handle<Quote>>(),
                                      const std::vector<Date>& jumpDates = std::vector<Date>())
        : SurvivalProbabilityStructure(yieldTermStructure->referenceDate(), yieldTermStructure->calendar(),
                                       yieldTermStructure->dayCounter(), jumps, jumpDates),
          yieldTermStructure_(yieldTermStructure), rr_(rr) {
        registerWith(yieldTermStructure_);
        registerWith(rr_);
    }
    //! \name TermStructure interface
    //@{
    Date maxDate() const override;
    //@}

private:
    //! \name DefaultProbabilityTermStructure implementation
    //@{
    Probability survivalProbabilityImpl(Time) const override;
    //@}
    QuantLib::Handle<YieldTermStructure> yieldTermStructure_;
    QuantLib::Handle<QuantLib::Quote> rr_;
};

Date SurvivalProbabilityCurveFromYield::maxDate() const {
    return yieldTermStructure_->maxDate();
}

Probability SurvivalProbabilityCurveFromYield::survivalProbabilityImpl(Time t) const {
    auto df = yieldTermStructure_->discount(t);
    return std::pow(df, 1.0 / (1.0 - rr_->value()));
}

} // namespace QuantExt
