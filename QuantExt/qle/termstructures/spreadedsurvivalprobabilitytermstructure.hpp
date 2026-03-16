/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadedsurvivalprobabilitytermstructure.hpp
    \brief spreaded default term structure
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Spreaded Default Term Structure, the spread is given in terms of loglinearly interpolated survival probabilities.
class SpreadedSurvivalProbabilityTermStructure : public SurvivalProbabilityStructure, public LazyObject {
public:
    enum class Extrapolation { flatFwd, flatZero };
    //! times should be consistent with reference ts day counter
    SpreadedSurvivalProbabilityTermStructure(const Handle<DefaultProbabilityTermStructure>& referenceCurve,
                                             const std::vector<Time>& times, const std::vector<Handle<Quote>>& spreads,
                                             const Extrapolation extrapolation = Extrapolation::flatFwd);
    //@}
    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const override;
    Date maxDate() const override;
    Time maxTime() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    std::vector<Time> times();
    Handle<DefaultProbabilityTermStructure> referenceCurve() const;
    //@}
private:
    void performCalculations() const override;
    Probability survivalProbabilityImpl(Time) const override;
    void update() override;

    Handle<DefaultProbabilityTermStructure> referenceCurve_;
    std::vector<Time> times_;
    std::vector<Handle<Quote>> spreads_;
    mutable std::vector<Real> data_;
    QuantLib::ext::shared_ptr<Interpolation> interpolation_;
    Extrapolation extrapolation_;
};

} // namespace QuantExt
