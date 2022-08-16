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

#include <ql/cashflows/cpicoupon.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/utilities/inflation.hpp>
#include <ql/termstructures/inflation/interpolatedzeroinflationcurve.hpp>

using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::Days;
using QuantLib::Frequency;
using QuantLib::Handle;
using QuantLib::InflationIndex;
using QuantLib::InflationTermStructure;
using QuantLib::inflationYearFraction;
using QuantLib::Period;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Time;
using QuantLib::ZeroInflationTermStructure;
using std::pow;

namespace {

// Throws an error if index doesn't have a historical fixing for fixingDate
void throwExceptionIfHistoricalFixingMissing(const Date& fixingDate, const QuantLib::ZeroInflationIndex& index) {
    QL_REQUIRE(index.hasHistoricalFixing(fixingDate),
               "Historical fixing missing for index " << index.name() << " on " << fixingDate);
}

// Throws an erro if any fixing required to compute a cpi fixing is missing
void checkIfFixingAvailable(const Date& maturity, const Period obsLag, bool interpolated,
                            const QuantLib::ZeroInflationIndex& index) {

    auto fixingPeriod = inflationPeriod(maturity - obsLag, index.frequency());
    throwExceptionIfHistoricalFixingMissing(fixingPeriod.first, index);
    if (interpolated)
        throwExceptionIfHistoricalFixingMissing(fixingPeriod.second + 1 * Days, index);
}
}

namespace QuantExt {

Time inflationTime(const Date& date,
    const boost::shared_ptr<InflationTermStructure>& inflationTs,
    bool indexIsInterpolated,               
    const DayCounter& dayCounter) {

    DayCounter dc = inflationTs->dayCounter();
    if (dayCounter != DayCounter())
        dc = dayCounter;

    return inflationYearFraction(inflationTs->frequency(), indexIsInterpolated, 
        dc, inflationTs->baseDate(), date);
}

Real inflationGrowth(const Handle<ZeroInflationTermStructure>& ts, Time t, const DayCounter& dc, bool indexIsInterpolated) {
    auto lag = inflationTime(ts->referenceDate(), *ts, indexIsInterpolated, dc);
    return pow(1.0 + ts->zeroRate(t - lag), t);
}

Real inflationGrowth(const Handle<ZeroInflationTermStructure>& ts, Time t, bool indexIsInterpolated) {
    return inflationGrowth(ts, t, ts->dayCounter(), indexIsInterpolated);
}

Real inflationLinkedBondQuoteFactor(const boost::shared_ptr<QuantLib::Bond>& bond) {
    QuantLib::Real inflFactor = 1;
    for (auto& cf : bond->cashflows()) {
        if (auto inflCpn = boost::dynamic_pointer_cast<QuantLib::CPICoupon>(cf)) {
            const auto& inflationIndex = boost::dynamic_pointer_cast<QuantLib::ZeroInflationIndex>(inflCpn->index());
            Date settlementDate = bond->settlementDate();
            std::pair<Date, Date> currentInflationPeriod = inflationPeriod(settlementDate, inflationIndex->frequency());
            std::pair<Date, Date> settlementFixingPeriod = inflationPeriod(settlementDate - inflCpn->observationLag(), inflationIndex->frequency());
            Date curveBaseDate = settlementFixingPeriod.first;
            //Date curveBaseDate = inflationIndex->zeroInflationTermStructure()->baseDate();
            Real todaysCPI = inflationIndex->fixing(curveBaseDate);
            if (inflCpn->observationInterpolation() == QuantLib::CPI::Linear) {
                
                std::pair<Date, Date> observationPeriod = inflationPeriod(curveBaseDate, inflationIndex->frequency());
                
                Real indexStart = inflationIndex->fixing(observationPeriod.first);
                Real indexEnd = inflationIndex->fixing(observationPeriod.second + 1 * QuantLib::Days);
                
                todaysCPI = indexStart + (settlementDate - currentInflationPeriod.first) *
                                             (indexEnd - indexStart) /
                                             (Real)(currentInflationPeriod.second - currentInflationPeriod.first);
            }
            QuantLib::Rate baseCPI = inflCpn->baseCPI();
            if (baseCPI == QuantLib::Null<QuantLib::Rate>()) {
                baseCPI =
                    QuantLib::CPI::laggedFixing(inflCpn->cpiIndex(), inflCpn->baseDate() + inflCpn->observationLag(),
                                                inflCpn->observationLag(),
                                                inflCpn->observationInterpolation());
            }
            inflFactor = todaysCPI / baseCPI;
            break;
        }
    }
    return inflFactor;
}
namespace ZeroInflation {

QuantLib::Date lastAvailableFixing(const QuantLib::ZeroInflationIndex& index, const QuantLib::Date& asof) {
    Date availbilityLagFixingDate = inflationPeriod(asof - index.availabilityLag(), index.frequency()).first;
    if (index.hasHistoricalFixing(availbilityLagFixingDate)) {
        return availbilityLagFixingDate;
    } else {
        // The fixing for the inflation period before the availability lag should be there
        return inflationPeriod(availbilityLagFixingDate - 1 * QuantLib::Days, index.frequency()).first;
    }
}

QuantLib::Rate cpiFixing(const QuantLib::ZeroInflationIndex& index, const QuantLib::Date maturity,
                         const QuantLib::Period& obsLag, bool interpolated) {
    if (interpolated) {
        QuantLib::Period oneDay = 1 * QuantLib::Days;
        auto fixingPeriod = inflationPeriod(maturity - obsLag, index.frequency());
        auto paymentPeriod = inflationPeriod(maturity, index.frequency());
        Date I0Date = fixingPeriod.first;
        Date I1Date = fixingPeriod.second + oneDay;
        Rate I0 = index.fixing(I0Date);
        Rate I1 = index.fixing(I1Date);
        return I0 +
               (I1 - I0) * (maturity - paymentPeriod.first) / (paymentPeriod.second + oneDay - paymentPeriod.first);

    } else {
        Date observationDate = inflationPeriod(maturity - obsLag, index.frequency()).first;
        return index.fixing(observationDate);
    }
}


QuantLib::Rate guessCurveBaseRate(const bool baseDateLastKnownFixing, 
                                  const QuantLib::ZeroInflationIndex& index, bool interpolated,
                                  const QuantLib::Date& today, const QuantLib::Period& tenor,
                                  const QuantLib::Period& obsLag, const QuantLib::DayCounter& dc,
                                  const QuantLib::Rate zeroRate) {
    // If the baseDate in Curve is today - obsLag then the quoted zeroRate should be used
    if (!baseDateLastKnownFixing)
        return zeroRate;
    // Otherwise we need to take into account the accrued inflation between rate helper base date and curve base date
    // Check if historical fixings available
    try {
        checkIfFixingAvailable(today, obsLag, interpolated, index);
    } catch (const std::exception& e) {
        QL_FAIL("Can not estimate the curve base date, got " << e.what());
    }

    // TODO check historical fixings available
    Rate instrumentBaseCPI = cpiFixing(index, today, obsLag, interpolated);
    Time timeFromInstrumentBase =
        inflationYearFraction(index.frequency(), interpolated, dc, today - obsLag, today + tenor - obsLag);
    auto fwdCPI = instrumentBaseCPI * std::pow(1 + zeroRate, timeFromInstrumentBase);

    Date curveBaseDate = lastAvailableFixing(index, today);
    double curveBaseFixing = index.fixing(curveBaseDate);

    if (!interpolated) {
        Time timeFromCurveBase =
            inflationYearFraction(index.frequency(), interpolated, dc, curveBaseDate, today + tenor - obsLag);

        return std::pow(fwdCPI / curveBaseFixing, 1.0 / timeFromCurveBase) - 1.0;
    } else {
        // Compute the interpolated  fixing of the ZCIIS at maturity
        auto fixingPeriod = inflationPeriod(today + tenor - obsLag, index.frequency());
        auto paymentPeriod = inflationPeriod(today + tenor, index.frequency());
        // Fixing times from curve base date
        Time d1 = inflationYearFraction(index.frequency(), false, dc, curveBaseDate, fixingPeriod.first);
        Time d2 = inflationYearFraction(index.frequency(), false, dc, curveBaseDate, fixingPeriod.second + 1 * Days);

        // Time interpolation
        Time t = inflationYearFraction(index.frequency(), true, dc, curveBaseDate, today + tenor);
        Time t1 = inflationYearFraction(index.frequency(), false, dc, curveBaseDate, paymentPeriod.first);
        Time t2 = inflationYearFraction(index.frequency(), false, dc, curveBaseDate, paymentPeriod.second + 1 * Days);
        Time interpolationFactor = (t - t1) / (t2 - t1);

        // Root search for a constant rate that the interpolation of both cpi matches the forward cpi
        Real target = fwdCPI / curveBaseFixing;

        std::function<double(double)> objectiveFunction = [&d1, &d2, &interpolationFactor, &target](Rate r) {
            return target - (std::pow(1 + r, d1) + (std::pow(1 + r, d2) - std::pow(1 + r, d1)) * interpolationFactor);
        };

        Rate guess = std::pow(fwdCPI / curveBaseFixing, 1.0 / d2) - 1.0;
        Rate r = guess;
        try {
            QuantLib::Brent solver;
            r = solver.solve(objectiveFunction, 1e-8, guess, -0.1, 0.2);
        } catch (...) {
            r = guess;
        }
        return r;
    }
}

} // namespace ZeroInflation

} // namespace QuantExt
