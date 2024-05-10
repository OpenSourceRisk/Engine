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
#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>

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

// Throws an error if any fixing required to compute a cpi fixing is missing
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
    const QuantLib::ext::shared_ptr<InflationTermStructure>& inflationTs,
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

Real inflationLinkedBondQuoteFactor(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond) {
    QuantLib::Real inflFactor = 1;
    for (auto& cf : bond->cashflows()) {
        if (auto inflCpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::CPICoupon>(cf)) {
            const auto& inflationIndex = QuantLib::ext::dynamic_pointer_cast<QuantLib::ZeroInflationIndex>(inflCpn->index());
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

void addInflationIndexToMap(
    std::map<std::tuple<std::string, QuantLib::CPI::InterpolationType, QuantLib::Frequency, QuantLib::Period>,
             QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>>& inflationIndices,
    const QuantLib::ext::shared_ptr<QuantLib::Index>& index, QuantLib::CPI::InterpolationType interpolation,
    Frequency couponFrequency, Period observationLag) {
    if (index != nullptr) {
        const auto zInfIndex = QuantLib::ext::dynamic_pointer_cast<QuantLib::ZeroInflationIndex>(index);
        std::string name = index->name();
        const auto key = std::make_tuple(name, interpolation, couponFrequency, observationLag);
        if (zInfIndex != nullptr && inflationIndices.count(key) == 0) {
            inflationIndices[key] = zInfIndex;
        }
    }
};

std::map<std::tuple<std::string, QuantLib::CPI::InterpolationType, QuantLib::Frequency, QuantLib::Period>,
         QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>>
extractAllInflationUnderlyingFromBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond) {

    std::map<std::tuple<std::string, QuantLib::CPI::InterpolationType, Frequency, Period>,
             QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>>
        inflationIndices;
    if (bond != nullptr) {
        for (const auto& cf : bond->cashflows()) {
            if (auto cp = QuantLib::ext::dynamic_pointer_cast<QuantLib::CPICoupon>(cf)) {
                addInflationIndexToMap(inflationIndices, cp->index(), cp->observationInterpolation(),
                                       cp->index()->frequency(), cp->observationLag());
            } else if (auto cp = QuantLib::ext::dynamic_pointer_cast<QuantLib::CPICashFlow>(cf)) {
                addInflationIndexToMap(inflationIndices, cp->index(), cp->interpolation(), cp->frequency(),
                                       cp->observationLag());
            }
        }
    }
    return inflationIndices;
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

QuantLib::Rate cpiFixing(const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const QuantLib::Date& maturity,
                         const QuantLib::Period& obsLag, bool interpolated) {
    QuantLib::CPI::InterpolationType interpolation = interpolated ? QuantLib::CPI::Linear : QuantLib::CPI::Flat;
    return QuantLib::CPI::laggedFixing(index, maturity, obsLag, interpolation);
}

QuantLib::Date curveBaseDate(const bool baseDateLastKnownFixing, const QuantLib::Date& refDate,
                             const QuantLib::Period obsLagCurve, const QuantLib::Frequency curveFreq,
                             const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index) {
    if (baseDateLastKnownFixing) {
        QL_REQUIRE(index, "can not compute curve base date based on the last known index fixing if no index provided");
        return lastAvailableFixing(*index, refDate);
    } else {
        return QuantLib::inflationPeriod(refDate - obsLagCurve, curveFreq).first;
    }
}

QuantLib::Date fixingDate(const QuantLib::Date& d, const QuantLib::Period obsLag, const QuantLib::Frequency freq,
                          bool interpolated) {
    Date obsDate = d - obsLag;
    if (!interpolated)
        obsDate = QuantLib::inflationPeriod(obsDate, freq).first;
    return obsDate;
}

QuantLib::Rate guessCurveBaseRate(const bool baseDateLastKnownFixing, const QuantLib::Date& swapStart,
                                  const QuantLib::Date& asof,
                                  const QuantLib::Period& swapTenor, const QuantLib::DayCounter& swapZCLegDayCounter,
                                  const QuantLib::Period& swapObsLag, const QuantLib::Rate zeroCouponRate,
                                  const QuantLib::Period& curveObsLag, const QuantLib::DayCounter& curveDayCounter,
                                  const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const bool interpolated,
                                  const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality) {
    QuantLib::ext::shared_ptr<QuantLib::MultiplicativePriceSeasonality> multiplicativeSeasonality =
        seasonality ? QuantLib::ext::dynamic_pointer_cast<QuantLib::MultiplicativePriceSeasonality>(seasonality) : nullptr;  
    
    QL_REQUIRE(seasonality ==  nullptr || multiplicativeSeasonality,
               "Only multiplicative seasonality supported at the moment");

    Date swapBaseDate = ZeroInflation::fixingDate(swapStart, swapObsLag, index->frequency(), interpolated);

    Date curveBaseDate =
        ZeroInflation::curveBaseDate(baseDateLastKnownFixing, asof, curveObsLag, index->frequency(), index);

    // If the baseDate in Curve is today - obsLag then the quoted zeroRate should be used
    if (!baseDateLastKnownFixing && swapBaseDate == curveBaseDate)
        return zeroCouponRate;
    
    QL_REQUIRE(index, "can not compute base cpi of the zero coupon swap");
    // Otherwise we need to take into account the accrued inflation between rate helper base date and curve base date
    // Check if historical fixings available
    try {
        checkIfFixingAvailable(swapStart, swapObsLag, interpolated, *index);
    } catch (const std::exception& e) {
        QL_FAIL("Can not estimate the curve base date, got " << e.what());
    }

    Date swapMaturity = swapStart + swapTenor;
    Date swapObservationDate = ZeroInflation::fixingDate(swapMaturity, swapObsLag, index->frequency(), interpolated);

    // TODO check historical fixings available
    Rate instrumentBaseCPI = cpiFixing(index, swapStart, swapObsLag, interpolated);
    Time timeFromSwapBase =
        inflationYearFraction(index->frequency(), interpolated, swapZCLegDayCounter, swapBaseDate, swapObservationDate);

    auto fwdCPI = instrumentBaseCPI * std::pow(1 + zeroCouponRate, timeFromSwapBase);

    double curveBaseFixing = index->fixing(curveBaseDate);

    if (!interpolated) {
        Time timeFromCurveBase = inflationYearFraction(index->frequency(), interpolated, curveDayCounter, curveBaseDate,
                                                       swapObservationDate);
        double rateWithSeasonality = std::pow(fwdCPI / curveBaseFixing, 1.0 / timeFromCurveBase) - 1.0;
        
        if (multiplicativeSeasonality) {
            double factorAt = multiplicativeSeasonality->seasonalityFactor(swapObservationDate);
            double factorBase = multiplicativeSeasonality->seasonalityFactor(curveBaseDate);
            double seasonalityFactor = std::pow(factorAt / factorBase, 1.0 / timeFromCurveBase);
            return (rateWithSeasonality + 1) / seasonalityFactor - 1;
        } else {
            return rateWithSeasonality;
        }
    } else {
        // Compute the interpolated  fixing of the ZCIIS at maturity
        auto fixingPeriod = inflationPeriod(swapObservationDate, index->frequency());
        auto paymentPeriod = inflationPeriod(swapMaturity, index->frequency());
        // Fixing times from curve base date
        Time timeToFixing1 =
            inflationYearFraction(index->frequency(), false, curveDayCounter, curveBaseDate, fixingPeriod.first);
        Time timeToFixing2 = inflationYearFraction(index->frequency(), false, curveDayCounter, curveBaseDate,
                                        fixingPeriod.second + 1 * Days);

        // Time interpolation
        Time timeToPayment =
            inflationYearFraction(index->frequency(), true, curveDayCounter, curveBaseDate, swapMaturity);
        Time timeToStartPaymentPeriod =
            inflationYearFraction(index->frequency(), false, curveDayCounter, curveBaseDate, paymentPeriod.first);
        Time timeToEndPaymenetPeriod = inflationYearFraction(index->frequency(), false, curveDayCounter, curveBaseDate,
                                        paymentPeriod.second + 1 * Days);
        Time interpolationFactor =
            (timeToPayment - timeToStartPaymentPeriod) / (timeToEndPaymenetPeriod - timeToStartPaymentPeriod);

        // Root search for a constant rate that the interpolation of both cpi matches the forward cpi
        Real target = fwdCPI / curveBaseFixing;

        Real seasonalityFactor1 = 1.0;
        Real seasonalityFactor2 = 1.0;

        if (multiplicativeSeasonality) {
            double factorAt1 = multiplicativeSeasonality->seasonalityFactor(fixingPeriod.first);
            double factorAt2 = multiplicativeSeasonality->seasonalityFactor(fixingPeriod.second + 1 * Days);
            double factorBase = multiplicativeSeasonality->seasonalityFactor(curveBaseDate);
            seasonalityFactor1 = factorAt1 / factorBase;
            seasonalityFactor2 = factorAt2 / factorBase;
        }


        std::function<double(double)> objectiveFunction = [&timeToFixing1, &timeToFixing2, &interpolationFactor,
                                                           &target, &seasonalityFactor1, &seasonalityFactor2](Rate r) {
            return target - (std::pow(1 + r, timeToFixing1) * seasonalityFactor1  +
                             (std::pow(1 + r, timeToFixing2) * seasonalityFactor2 -
                              std::pow(1 + r, timeToFixing1) * seasonalityFactor1) *
                                 interpolationFactor);
        };

        Rate guess = std::pow(fwdCPI / curveBaseFixing, 1.0 / timeToFixing2) - 1.0;
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

bool isCPIVolSurfaceLogNormal(const QuantLib::ext::shared_ptr<QuantLib::CPIVolatilitySurface>& surface) {
    if (auto qvs = QuantLib::ext::dynamic_pointer_cast<QuantExt::CPIVolatilitySurface>(surface)) {
        return qvs->isLogNormal();
    } else {
        // QuantLib::CPIVolatilitySurface doesn't support volType so assume it's log normal
        return true;
    }
}

} // namespace ZeroInflation

} // namespace QuantExt
