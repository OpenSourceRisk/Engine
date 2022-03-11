/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file flatteneddefaultcurve.hpp
    \brief Default curve using a flat par rate at a given date.
    \ingroup models
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Default curve using a flat par rate at a given date.
/* The curve implies a fair spread for a CDS traded at the reference date of the source curve with
   maturity given by samplingDate. The curve is then using a flat hazard rate giving a zero NPV
   for this fair CDS. The main use case is to back out a flat index cds curve (e.g. for the 5Y
   underlying) from a source curve that was bootstrapped from several tenors (e.g. 3Y,5Y,7Y,10Y). */
class FlattenedDefaultCurve : public SurvivalProbabilityStructure, public LazyObject {
public:
    FlattenedDefaultCurve(const Handle<DefaultProbabilityTermStructure>& source, const Handle<Quote>& recovery,
                          const Handle<YieldTermStructure>& discount, const Date& samplingDate);

    Date maxDate() const override { return Date::maxDate(); }

    const Date& referenceDate() const override { return source_->referenceDate(); }

    void update() override;

    void performCalculations() const override;

protected:
    Real survivalProbabilityImpl(Time t) const override;
    Handle<DefaultProbabilityTermStructure> source_;
    Handle<Quote> recovery_;
    Handle<YieldTermStructure> discount_;
    Date samplingDate_;
    mutable Real flatRate_;
};

} // namespace QuantExt
