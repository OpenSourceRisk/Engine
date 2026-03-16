/*
  Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/pairwisevarianceswapengine.cpp
    \brief pairwise variance swap engine
    \ingroup engines
*/

#include <qle/pricingengines/pairwisevarianceswapengine.hpp>

#include <qle/indexes/equityindex.hpp>

#include <ql/indexes/indexmanager.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/math/integrals/segmentintegral.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <iostream>

using namespace QuantLib;

namespace QuantExt {

PairwiseVarianceSwapEngine::PairwiseVarianceSwapEngine(
    const QuantLib::ext::shared_ptr<Index>& index1, const QuantLib::ext::shared_ptr<Index>& index2,
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process1,
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process2, const Handle<YieldTermStructure>& discountingTS,
    Handle<Quote> correlation)
    : index1_(index1), index2_(index2), process1_(process1), process2_(process2), discountingTS_(discountingTS),
      correlation_(correlation) {

    QL_REQUIRE(process1_ && process2_, "Black-Scholes process not present.");

    registerWith(process1_);
    registerWith(process2_);
    registerWith(discountingTS_);
}

void PairwiseVarianceSwapEngine::calculate() const {

    QL_REQUIRE(!discountingTS_.empty(), "Empty discounting term structure handle");

    results_.value = 0.0;

    Date today = QuantLib::Settings::instance().evaluationDate();
    const Date& maturityDate = arguments_.settlementDate;

    if (today > maturityDate)
        return;

    // Variance is defined here as the annualised volatility squared.
    Variances variances = calculateVariances(arguments_.valuationSchedule, arguments_.laggedValuationSchedule, today);
    results_.additionalResults["accruedVariance1"] = variances.accruedVariance1;
    results_.additionalResults["accruedVariance2"] = variances.accruedVariance2;
    results_.additionalResults["accruedBasketVariance"] = variances.accruedBasketVariance;
    results_.additionalResults["futureVariance1"] = variances.futureVariance1;
    results_.additionalResults["futureVariance2"] = variances.futureVariance2;
    results_.additionalResults["futureBasketVariance"] = variances.futureBasketVariance;
    results_.additionalResults["totalVariance1"] = variances.totalVariance1;
    results_.additionalResults["totalVariance2"] = variances.totalVariance2;
    results_.additionalResults["totalBasketVariance"] = variances.totalBasketVariance;

    Real variance1 = variances.totalVariance1;
    Real variance2 = variances.totalVariance2;
    Real basketVariance = variances.totalBasketVariance;
    results_.variance1 = variance1;
    results_.variance2 = variance2;
    results_.basketVariance = basketVariance;

    Real strike1 = arguments_.strike1 * arguments_.strike1;
    Real strike2 = arguments_.strike2 * arguments_.strike2;
    Real basketStrike = arguments_.basketStrike * arguments_.basketStrike;

    if (!QuantLib::close_enough(arguments_.floor, 0.0)) {
        Real floor = arguments_.floor * arguments_.floor;
        variance1 = std::max(floor * strike1, variance1);
        variance2 = std::max(floor * strike2, variance2);
        basketVariance = std::max(floor * basketStrike, basketVariance);
    }

    if (!QuantLib::close_enough(arguments_.cap, 0.0)) {
        Real cap = arguments_.cap * arguments_.cap;
        variance1 = std::min(cap * strike1, variance1);
        variance2 = std::min(cap * strike2, variance2);
        basketVariance = std::min(cap * basketStrike, basketVariance);
    }

    results_.finalVariance1 = variance1;
    results_.finalVariance2 = variance2;
    results_.finalBasketVariance = basketVariance;
    results_.additionalResults["finalVariance1"] = variance1;
    results_.additionalResults["finalVariance2"] = variance2;
    results_.additionalResults["finalBasketVariance"] = basketVariance;

    Real currentNotional1 = 10000.0 * arguments_.notional1 / (2.0 * 100.0 * arguments_.strike1);
    Real currentNotional2 = 10000.0 * arguments_.notional2 / (2.0 * 100.0 * arguments_.strike2);
    Real currentBasketNotional = 10000.0 * arguments_.basketNotional / (2.0 * 100.0 * arguments_.basketStrike);

    results_.additionalResults["varianceAmount1"] = currentNotional1;
    results_.additionalResults["varianceAmount2"] = currentNotional2;
    results_.additionalResults["basketVarianceAmount"] = currentBasketNotional;

    Real equityAmount1 = currentNotional1 * (variance1 - strike1);
    Real equityAmount2 = currentNotional2 * (variance2 - strike2);
    Real equityAmountBasket = currentBasketNotional * (basketVariance - basketStrike);

    results_.equityAmount1 = equityAmount1;
    results_.equityAmount2 = equityAmount2;
    results_.equityAmountBasket = equityAmountBasket;
    results_.additionalResults["equityAmount1"] = equityAmount1;
    results_.additionalResults["equityAmount2"] = equityAmount2;
    results_.additionalResults["equityAmountBasket"] = equityAmountBasket;

    Real pairwiseEquityAmount = equityAmount1 + equityAmount2 + equityAmountBasket;

    results_.pairwiseEquityAmount = pairwiseEquityAmount;

    if (!QuantLib::close_enough(arguments_.payoffLimit, 0.0)) {
        Real equityAmountCap =
            arguments_.payoffLimit * (std::abs(arguments_.notional1) + std::abs(arguments_.notional2));
        Real equityAmountFloor = -1.0 * equityAmountCap;
        pairwiseEquityAmount = std::max(equityAmountFloor, pairwiseEquityAmount);
        pairwiseEquityAmount = std::min(equityAmountCap, pairwiseEquityAmount);
    }

    results_.finalEquityAmount = pairwiseEquityAmount;
    results_.additionalResults["finalEquityAmount"] = pairwiseEquityAmount;

    Real multiplier = arguments_.position == Position::Long ? 1.0 : -1.0;
    DiscountFactor df = discountingTS_->discount(arguments_.settlementDate);

    results_.value = multiplier * df * pairwiseEquityAmount;
}

Variances PairwiseVarianceSwapEngine::calculateVariances(const Schedule& valuationSchedule,
                                                         const Schedule& laggedValuationSchedule,
                                                         const Date& evalDate) const {

    const Date& accrStartDate = valuationSchedule.dates().back();
    const Date& accrEndDate = laggedValuationSchedule.dates().back();

    Variances res;

    for (Size i = 0; i < valuationSchedule.size(); i++) {
        const Date& vd = valuationSchedule.dates()[i];
        const Date& lvd = laggedValuationSchedule.dates()[i];

        // Calculate accrued (squared) variations, i.e. both val date and lagged val date are known
        if (lvd <= evalDate) {
            Real price1 = index1_->fixing(vd);
            Real laggedPrice1 = index1_->fixing(lvd);
            Real variation1 = std::log(laggedPrice1 / price1);
            res.accruedVariance1 += variation1 * variation1;

            Real price2 = index2_->fixing(vd);
            Real laggedPrice2 = index2_->fixing(lvd);
            Real variation2 = std::log(laggedPrice2 / price2);
            res.accruedVariance2 += variation2 * variation2;

            Real basketPrice = price1 + price2;
            Real laggedBasketPrice = laggedPrice1 + laggedPrice2;
            Real basketVariation = std::log(laggedBasketPrice / basketPrice);
            res.accruedBasketVariance += basketVariation * basketVariation;
        } else {
            // At this point, all variation contributions have been accrued, so now calculate the accrued variance
            Real expectedN = valuationSchedule.dates().size() - 1.0;
            Real factor = 252 / (expectedN * arguments_.accrualLag);
            res.accruedVariance1 *= factor;
            res.accruedVariance2 *= factor;
            res.accruedBasketVariance *= factor;

            // Calculate future variance
            Real t = ActualActual(ActualActual::ISDA).yearFraction(evalDate, accrEndDate);
            Real F1 =
                process1_->x0() / process1_->riskFreeRate()->discount(t) * process1_->dividendYield()->discount(t);
            Real F2 =
                process2_->x0() / process2_->riskFreeRate()->discount(t) * process2_->dividendYield()->discount(t);

            Real variance1 = process1_->blackVolatility()->blackVariance(t, F1);
            Real variance2 = process2_->blackVolatility()->blackVariance(t, F2);
            Real basketVariance =
                variance1 + variance2 + 2.0 * std::sqrt(variance1) * std::sqrt(variance2) * correlation_->value();

            res.futureVariance1 = variance1;
            res.futureVariance2 = variance2;
            res.futureBasketVariance = basketVariance;
            break;
        }
    }

    Calendar jointCal =
        JointCalendar(std::vector<Calendar>{valuationSchedule.calendar(), laggedValuationSchedule.calendar(),
                                            index1_->fixingCalendar(), index2_->fixingCalendar()});
    Real accrTime = std::abs(jointCal.businessDaysBetween(accrStartDate, evalDate, true, true));
    Real futTime = std::abs(jointCal.businessDaysBetween(evalDate, accrEndDate, true, false));
    Real totalTime = accrTime + futTime;
    Real accrFactor = accrTime / totalTime;
    Real futFactor = futTime / totalTime;

    res.totalVariance1 = (res.accruedVariance1 * accrFactor) + (res.futureVariance1 * futFactor);
    res.totalVariance2 = (res.accruedVariance2 * accrFactor) + (res.futureVariance2 * futFactor);
    res.totalBasketVariance = (res.accruedBasketVariance * accrFactor) + (res.futureBasketVariance * futFactor);

    return res;
}

} // namespace QuantExt
