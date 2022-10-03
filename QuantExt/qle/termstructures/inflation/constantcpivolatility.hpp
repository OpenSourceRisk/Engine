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

#include <ql/termstructures/volatility/inflation/constantcpivolatility.hpp>

namespace QuantExt {

//! Constant surface, no K or T dependence.
class ConstantCPIVolatility : public QuantLib::ConstantCPIVolatility {
public:
    //! \name Constructor
    //@{
    //! calculate the reference date based on the global evaluation date
    ConstantCPIVolatility(QuantLib::Volatility v, QuantLib::Natural settlementDays, const QuantLib::Calendar&,
                          QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                          const QuantLib::Period& observationLag, QuantLib::Frequency frequency,
                          bool indexIsInterpolated, const QuantLib::Date& capFloorStartDate = QuantLib::Date());

    //! base date will be in the past
    virtual QuantLib::Date baseDate() const override;

private:
    QuantLib::Date capFloorStartDate() const;

    QuantLib::Date capFloorStartDate_;
};

} // namespace QuantExt
