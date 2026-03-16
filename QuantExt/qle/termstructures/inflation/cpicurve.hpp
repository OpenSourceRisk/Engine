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
#pragma once
#include <ql/termstructures/inflationtermstructure.hpp>
namespace QuantExt {
class CPICurve : public QuantLib::ZeroInflationTermStructure {
public:
    CPICurve(QuantLib::Date baseDate, QuantLib::Real baseCPI, const QuantLib::Period& observationLag,
             QuantLib::Frequency frequency, const QuantLib::DayCounter& dayCounter,
             const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = {});

    CPICurve(const QuantLib::Date& referenceDate, QuantLib::Date baseDate, QuantLib::Real baseCPI,
             const QuantLib::Period& observationLag, QuantLib::Frequency frequency,
             const QuantLib::DayCounter& dayCounter,
             const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = {});

    CPICurve(QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar, QuantLib::Date baseDate,
             QuantLib::Real baseCPI, const QuantLib::Period& observationLag, QuantLib::Frequency frequency,
             const QuantLib::DayCounter& dayCounter,
             const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = {});

    QuantLib::Real baseCPI() const { return baseCPI_; }

    QuantLib::Real CPI(const QuantLib::Date& d, bool extrapolate = false) const;

protected:
    QuantLib::Rate baseCPI_;

    QuantLib::Rate zeroRateImpl(QuantLib::Time t) const override;

    virtual QuantLib::Rate forwardCPIImpl(QuantLib::Time t) const = 0;

private:
    void check() const;
};
} // namespace QuantExt
