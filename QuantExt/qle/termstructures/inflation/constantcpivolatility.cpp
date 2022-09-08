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

#include <qle/termstructures/inflation/constantcpivolatility.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

ConstantCPIVolatility::ConstantCPIVolatility(const QuantLib::Volatility v, QuantLib::Natural settlementDays,
                                             const QuantLib::Calendar& cal, QuantLib::BusinessDayConvention bdc,
                                             const QuantLib::DayCounter& dc, const QuantLib::Period& observationLag,
                                             QuantLib::Frequency frequency, bool indexIsInterpolated,
                                             const QuantLib::Date& lastAvailableFixingDate)
    : QuantLib::ConstantCPIVolatility(v, settlementDays, cal, bdc, dc, observationLag, frequency, indexIsInterpolated),
      lastAvailableFixingDate_(lastAvailableFixingDate) {}

QuantLib::Volatility ConstantCPIVolatility::totalVariance(const QuantLib::Date& maturityDate, QuantLib::Rate strike,
                                                          const QuantLib::Period& obsLag,
                                                          bool extrapolate) const {
    if (lastAvailableFixingDate_ == QuantLib::Null<QuantLib::Date>()) {
        return CPIVolatilitySurface::totalVariance(maturityDate, strike, obsLag, extrapolate);
    } else {
        QuantLib::Volatility vol = volatility(maturityDate, strike, obsLag, extrapolate);
        
        QuantLib::Period useLag = obsLag;
        if (obsLag == QuantLib::Period(-1, QuantLib::Days)) {
            useLag = observationLag();
        }

        QuantLib::Date effectiveMaturityDate =
            ZeroInflation::effectiveObservationDate(maturityDate, useLag, frequency_, indexIsInterpolated_);
        double t = timeFromReference(effectiveMaturityDate) - timeFromReference(lastAvailableFixingDate_);
        return vol * vol * t;
    }

}

} // namespace QuantExt
