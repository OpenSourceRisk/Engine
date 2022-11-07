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

/*! \file qle/termstructures/inflation/cpivolatilitystructure.hpp
    \brief interpolated correlation term structure
*/

#pragma once

#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>

namespace QuantExt {

/*! Abstract interface. CPI volatility is always with respect to
    some base date of the quoted Zero Coupon Caps and Floor.
    Also deal with lagged observations of an index
    with a (usually different) availability lag.
*/
class CPIVolatilitySurface : public QuantLib::CPIVolatilitySurface {
public:
    /*!  the capfloor startdate is the start date of the quoted market instruments,
     * usually its today, but it could be in the future (e.g. AUCPI)
     */
    CPIVolatilitySurface(QuantLib::Natural settlementDays, const QuantLib::Calendar&,
                         QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                         const QuantLib::Period& observationLag, QuantLib::Frequency frequency,
                         bool indexIsInterpolated, const QuantLib::Date& capFloorStartDate = QuantLib::Date(),
                         QuantLib::VolatilityType volType = QuantLib::ShiftedLognormal, double displacement = 0.0);

    //! Computes the expiry date from the capFloorStartDate()
    virtual QuantLib::Date optionMaturityFromTenor(const QuantLib::Period& tenor) const;

    //! base date will be in the past
    virtual QuantLib::Date baseDate() const override;

    //! Returns the volatility type
    virtual QuantLib::VolatilityType volatilityType() const { return volType_; }
    //! Returns the displacement for lognormal volatilities
    virtual double displacement() const { return displacement_; }

    virtual bool isLogNormal() const { return volatilityType() == QuantLib::ShiftedLognormal; }

    using QuantLib::CPIVolatilitySurface::volatility;

    virtual QuantLib::Volatility volatility(const QuantLib::Period& optionTenor, QuantLib::Rate strike,
                                            const QuantLib::Period& obsLag = QuantLib::Period(-1, QuantLib::Days),
                                            bool extrapolate = false) const override;

    using QuantLib::CPIVolatilitySurface::totalVariance;

    virtual QuantLib::Volatility totalVariance(const QuantLib::Period& tenor, QuantLib::Rate strike,
                                               const QuantLib::Period& obsLag = QuantLib::Period(-1, QuantLib::Days),
                                               bool extrapolate = false) const override;

protected:
    //! Computes the expiry time from the capFloorStartDate()
    //  time from reference till relevant fixing Date for a capFloor expiriy at maturityDate
    virtual double fixingTime(const QuantLib::Date& maturityDate) const;
    QuantLib::Date capFloorStartDate() const;
    QuantLib::VolatilityType volType_;
    double displacement_;

private:
    QuantLib::Date capFloorStartDate_;
};

} // namespace QuantExt
