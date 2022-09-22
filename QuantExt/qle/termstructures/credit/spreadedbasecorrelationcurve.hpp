/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Spreaded Base Correlation Curve
class SpreadedBaseCorrelationCurve : public BaseCorrelationTermStructure, public LazyObject {
public:
    SpreadedBaseCorrelationCurve(const Handle<BaseCorrelationTermStructure>& baseCurve,
                                 const std::vector<Period>& tenors, const std::vector<double>& detachmentPoints,
                                 const std::vector<std::vector<Handle<Quote>>>& corrSpreads,
                                 const Date& startDate = Date(),
                                 boost::optional<DateGeneration::Rule> rule = boost::none);
    //@}
    void update() override;

    // TermStructure interface
    virtual Date maxDate() const override { return baseCurve_->maxDate(); }

    virtual Time maxTime() const override { return baseCurve_->maxTime(); }
    virtual Time minTime() const override { return baseCurve_->minTime(); }

    virtual double minDetachmentPoint() const override { return baseCurve_->minDetachmentPoint(); }
    virtual double maxDetachmentPoint() const override { return baseCurve_->maxDetachmentPoint(); }

private:
    Real correlationImpl(Time t, Real strike) const override;
    void performCalculations() const override;
    Handle<BaseCorrelationTermStructure> baseCurve_;
    std::vector<std::vector<Handle<Quote>>> corrSpreads_;
    mutable Matrix data_;
    mutable Interpolation2D interpolation_;
};

} // namespace QuantExt
