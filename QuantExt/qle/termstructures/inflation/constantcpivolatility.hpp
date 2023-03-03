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

/*! \file qle/termstructures/inflation/constantcpivolatility.hpp
    \brief Constant CPI Volatility Surface
    \ingroup engines
*/
#pragma once

#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>

namespace QuantExt {

class ConstantCPIVolatility : public QuantExt::CPIVolatilitySurface {
public:

    ConstantCPIVolatility(QuantLib::Volatility v, QuantLib::Natural settlementDays, const QuantLib::Calendar&,
                          QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                          const QuantLib::Period& observationLag, QuantLib::Frequency frequency,
                          bool indexIsInterpolated, const QuantLib::Date& capFloorStartDate = QuantLib::Date(),
                          QuantLib::VolatilityType volType = QuantLib::ShiftedLognormal, double displacement = 0.0);

    QuantLib::Date maxDate() const override { return QuantLib::Date::maxDate(); }

    QuantLib::Real minStrike() const override { return QL_MIN_REAL; }

    QuantLib::Real maxStrike() const override { return QL_MAX_REAL; }

    QuantLib::Real atmStrike(const QuantLib::Date& maturity,
                             const QuantLib::Period& obsLag = QuantLib::Period(-1, QuantLib::Days)) const override;

private:
    virtual QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;
    double constantVol_;
};

} // namespace QuantExt
