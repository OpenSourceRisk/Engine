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

/*! \file cdsvolcurve.hpp
    \brief vol curve for cds and index cds
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/credit/defaulttermstructure.hpp>

namespace QuantExt {
using QuantLib::Date;

class CdsVolCurve : public QuantLib::VolatilityTermStructure {
public:
    enum class Type { Price, Spread };
    CdsVolCurve(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
                const Handle<CdsCurve>& underlyingCurve, const Type& type);
    CdsVolCurve(Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
                const Handle<CdsCurve>& underlyingCurve, const Type& type);

    Real volatility(const Date& exerciseDate, const Period& underlyingTerm, const Real strike,
                    const Type& targetType) const;
    Real volatility(const Date& exerciseDate, const Date& underlyingMaturity, const Real strike,
                    const Type& targetType) const;
    virtual Real volatility(const Real exerciseTime, const Real underlyingLength, const Real strike,
                            const Type& targetType) const = 0;

    Handle<CdsCurve> underlyingCurve() const;
    const Type& type() const;

protected:
    Date maxDate() const override;
    Rate minStrike() const override;
    Rate maxStrike() const override;

    Type type_;
}

class InterpolatingCdsVolCurve : public CdsVolCurve {
public:
    enum class StrikeType { Relative, Absolute };

    InterpolatingCdsVolCurve(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc,
                             const DayCounter& dc, const Handle<CdsCurve>& underlyingCurve, const Type& type,
                             const StrikeType& strikeType, const std::vector<Period>& optionTerms,
                             const std::vector<Period>& underlyingTerms, const std::vector<Real>& strikes,
                             const std::vector<std::vector<std::vector<Handle<Quote>>>>& quotes);

    InterpolatingCdsVolCurve(Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                             const DayCounter& dc, const Handle<CdsCurve>& underlyingCurve, const Type& type,
                             const StrikeType& strikeType, const std::vector<Period>& optionTerms,
                             const std::vector<Period>& underlyingTerms, const std::vector<Real>& strikes,
                             const std::vector<std::vector<std::vector<Handle<Quote>>>>& quotes);

    Real volatility(const Real exerciseTime, const Real underlyingLength, const Real strike,
                    const Type& targetType) const override;

private:
    StrikeTypet strikeType_;
};

} // namespace QuantExt
