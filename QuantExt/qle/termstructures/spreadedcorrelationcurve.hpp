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

/*! \file spreadedcorrelationcurve.hpp
    \brief Spreaded correlation curve
*/

#pragma once

#include <qle/termstructures/correlationtermstructure.hpp>

#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Spreaded Correlation Curve
class SpreadedCorrelationCurve : public CorrelationTermStructure, public LazyObject {
public:
    /*! - times should be consistent with reference ts day counter
        - if useAtmReferenceCorrsOnly, only corrs with strike Null<Real>() are read from the referenceVol,
          otherwise the full reference vol surface (if it is one) is used
     */
    SpreadedCorrelationCurve(const Handle<CorrelationTermStructure>& referenceCorrelation,
                             const std::vector<Time>& times, const std::vector<Handle<Quote>>& corrSpreads,
                             const bool useAtmReferenceVolsOnly = false);
    //@}

    Date maxDate() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    Time minTime() const override;
    void update() override;

private:
    Real correlationImpl(Time t, Real strike) const override;
    void performCalculations() const override;

    Handle<CorrelationTermStructure> referenceCorrelation_;
    std::vector<Time> times_;
    std::vector<Handle<Quote>> corrSpreads_;
    bool useAtmReferenceCorrsOnly_;
    mutable std::vector<Real> data_;
    QuantLib::ext::shared_ptr<Interpolation> interpolation_;
};

} // namespace QuantExt
