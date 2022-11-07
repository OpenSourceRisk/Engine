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

#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

CPIVolatilitySurface::CPIVolatilitySurface(QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                                           QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                                           const QuantLib::Period& observationLag, QuantLib::Frequency frequency,
                                           bool indexIsInterpolated, const QuantLib::Date& startDate,
                                           QuantLib::VolatilityType volType, double displacement)
    : QuantLib::CPIVolatilitySurface(settlementDays, cal, bdc, dc, observationLag, frequency, indexIsInterpolated),
      volType_(volType), displacement_(displacement), capFloorStartDate_(startDate) {}

QuantLib::Date CPIVolatilitySurface::capFloorStartDate() const {
    if (capFloorStartDate_ == QuantLib::Date()) {
        return referenceDate();
    } else {
        return capFloorStartDate_;
    }
}

QuantLib::Date CPIVolatilitySurface::optionMaturityFromTenor(const QuantLib::Period& tenor) const {
    return calendar().advance(capFloorStartDate(), tenor, businessDayConvention());
}

QuantLib::Date CPIVolatilitySurface::baseDate() const {
    // Depends on interpolation, or not, of observed index
    // and observation lag with which it was built.
    // We want this to work even if the index does not
    // have a term structure.
    if (indexIsInterpolated()) {
        return capFloorStartDate() - observationLag();
    } else {
        return QuantLib::inflationPeriod(capFloorStartDate() - observationLag(), frequency()).first;
    }
}

double CPIVolatilitySurface::fixingTime(const QuantLib::Date& maturityDate) const {
    return timeFromReference(ZeroInflation::fixingDate(maturityDate, observationLag(), frequency(), indexIsInterpolated()));
}

QuantLib::Volatility CPIVolatilitySurface::volatility(const QuantLib::Period& optionTenor, QuantLib::Rate strike,
                                                      const QuantLib::Period& obsLag, bool extrapolate) const {
    QuantLib::Date maturityDate = optionMaturityFromTenor(optionTenor);
    return QuantLib::CPIVolatilitySurface::volatility(maturityDate, strike, obsLag, extrapolate);
}

QuantLib::Volatility CPIVolatilitySurface::totalVariance(const QuantLib::Period& optionTenor, QuantLib::Rate strike,
                                                         const QuantLib::Period& obsLag, bool extrapolate) const {
    QuantLib::Date maturityDate = optionMaturityFromTenor(optionTenor);
    return QuantLib::CPIVolatilitySurface::totalVariance(maturityDate, strike, obsLag, extrapolate);
}

} // namespace QuantExt
