/*
  Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/varianceswapgeneralreplicationengine.cpp
    \brief equity variance swap engine
    \ingroup engines
*/

#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>

#include <qle/indexes/equityindex.hpp>

#include <ql/indexes/indexmanager.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/math/integrals/segmentintegral.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/calendars/jointcalendar.hpp>

#include <iostream>

using namespace QuantLib;

namespace QuantExt {

GeneralisedReplicatingVarianceSwapEngine::GeneralisedReplicatingVarianceSwapEngine(
    const QuantLib::ext::shared_ptr<Index>& index, const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
    const Handle<YieldTermStructure>& discountingTS, const VarSwapSettings settings, const bool staticTodaysSpot)
    : index_(index), process_(process), discountingTS_(discountingTS), settings_(settings),
      staticTodaysSpot_(staticTodaysSpot) {

    QL_REQUIRE(process_, "Black-Scholes process not present.");

    registerWith(process_);
    registerWith(discountingTS_);
}

void GeneralisedReplicatingVarianceSwapEngine::calculate() const {

    QL_REQUIRE(!discountingTS_.empty(), "Empty discounting term structure handle");

    results_.value = 0.0;

    Date today = QuantLib::Settings::instance().evaluationDate();

    if (today >= arguments_.maturityDate)
        return;

    // set up calendar combining holidays form index and instrument
    Calendar jointCal = JointCalendar(arguments_.calendar, index_->fixingCalendar());

    // Variance is defined here as the annualised volatility squared.
    Real variance = 0;
    if (arguments_.startDate > today) {
        // This calculation uses the (time-weighted) additivity of variance to calculate the result.
        Real tsTime = jointCal.businessDaysBetween(today, arguments_.startDate, true, true);
        Real teTime = jointCal.businessDaysBetween(today, arguments_.maturityDate, false, true);
        Real fwdTime =
            jointCal.businessDaysBetween(arguments_.startDate, arguments_.maturityDate, true, true);
        variance = (calculateFutureVariance(arguments_.maturityDate) * teTime -
                    calculateFutureVariance(arguments_.startDate) * tsTime) /
                   fwdTime;
        results_.additionalResults["accruedVariance"] = 0;
        results_.additionalResults["futureVariance"] = variance;
    } else if (arguments_.startDate == today) {
        // The only time the QL price works
        variance = calculateFutureVariance(arguments_.maturityDate);
        results_.additionalResults["accruedVariance"] = 0;
        results_.additionalResults["futureVariance"] = variance;
    } else {
        // Get weighted average of Future and Realised variancies.
        Real accVar = calculateAccruedVariance(jointCal);
        Real futVar = calculateFutureVariance(arguments_.maturityDate);
        results_.additionalResults["accruedVariance"] = accVar;
        results_.additionalResults["futureVariance"] = futVar;
        Real totalTime =
            jointCal.businessDaysBetween(arguments_.startDate, arguments_.maturityDate, true, true);
        Real accTime = jointCal.businessDaysBetween(arguments_.startDate, today, true, true);
        Real futTime = jointCal.businessDaysBetween(today, arguments_.maturityDate, false, true);
        variance = (accVar * accTime / totalTime) + (futVar * futTime / totalTime);
    }

    results_.additionalResults["totalVariance"] = variance;

    DiscountFactor df = discountingTS_->discount(arguments_.maturityDate);
    results_.additionalResults["MaturityDiscountFactor"] = df;
    Real multiplier = arguments_.position == Position::Long ? 1.0 : -1.0;

    results_.variance = variance;
    results_.value = multiplier * df * arguments_.notional * 10000.0 *
                     (variance - arguments_.strike); // factor of 10000 to convert vols to market quotes

    Real volStrike = std::sqrt(arguments_.strike);
    results_.additionalResults["VarianceNotional"] = arguments_.notional;
    results_.additionalResults["VarianceStrike"] = arguments_.strike;
    results_.additionalResults["VolatilityStrike"] = volStrike;
    results_.additionalResults["VegaNotional"] = arguments_.notional * 2 * 100 * volStrike;
}

Real GeneralisedReplicatingVarianceSwapEngine::calculateAccruedVariance(const Calendar& jointCal) const {
    // return annualised accrued variance
    // Calculate historical variance from arguments_.startDate to today
    Date today = QuantLib::Settings::instance().evaluationDate();

    std::map<Date, Real> dividends;
    if (arguments_.addPastDividends) {
        if (auto eqIndex = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(index_)) {
            auto divs = eqIndex->dividendFixings();
            for (const auto& d : divs)
                dividends[d.exDate] = d.rate;
        }
    }

    Real variance = 0.0;
    Size counter = 0;
    Date firstDate = jointCal.adjust(arguments_.startDate);
    Real last = index_->fixing(firstDate);
    QL_REQUIRE(last != Null<Real>(),
               "No fixing for " << index_->name() << " on date " << firstDate
                                << ". This is required for fixing the return on the first day of the variance swap.");

    for (Date d = jointCal.advance(firstDate, 1, Days); d < today;
         d = jointCal.advance(d, 1, Days)) {
        Real price = index_->fixing(d);
        QL_REQUIRE(price != Null<Real>(), "No fixing for " << index_->name() << " on date " << d);
        QL_REQUIRE(price > 0.0, "Fixing for " << index_->name() << " on date " << d << " must be greater than zero.");
        // Add historical dividend payment back to price
        Real dividend = dividends[d] != Null<Real>() ? dividends[d] : 0;
        Real move = log((price + dividend) / last);
        variance += move * move;
        counter++;
        last = price;
    }

    // Now do final move. Yesterday is a fixing, todays price is in equityPrice_
    Real dividend = dividends[today] != Null<Real>() ? dividends[today] : 0;
    Real x0 = staticTodaysSpot_ && cachedTodaysSpot_ != Null<Real>() ? cachedTodaysSpot_ : process_->x0();
    Real lastMove = log((x0 + dividend) / last);
    variance += lastMove * lastMove;
    counter++;

    // cache todays spot if this switch is active
    if (staticTodaysSpot_)
        cachedTodaysSpot_ = x0;

    // Annualize
    Size days = 252; // FIXME: maybe get days from calendar?
    return days * variance / counter;
}

Real GeneralisedReplicatingVarianceSwapEngine::calculateFutureVariance(const Date& maturity) const {

    // calculate maturity time

    Date today = QuantLib::Settings::instance().evaluationDate();
    Real T = ActualActual(ActualActual::ISDA).yearFraction(today, maturity);

    // calculate forward

    Real F = process_->x0() / process_->riskFreeRate()->discount(T) * process_->dividendYield()->discount(T);

    // set up integrator

    QuantLib::ext::shared_ptr<Integrator> integrator;
    if (settings_.scheme == VarSwapSettings::Scheme::GaussLobatto) {
        integrator = QuantLib::ext::make_shared<GaussLobattoIntegral>(settings_.maxIterations, QL_MAX_REAL, settings_.accuracy);
    } else if (settings_.scheme == VarSwapSettings::Scheme::Segment) {
        integrator = QuantLib::ext::make_shared<SegmentIntegral>(settings_.steps);
    } else {
        QL_FAIL("GeneralisedReplicationVarianceSwapEngine: internal error, unknown scheme");
    }

    // set up replication integrand

    auto replication = [F, T, this](Real K) {
        if (K < 1E-10)
            return 0.0;
        return blackFormula(K < F ? Option::Put : Option::Call, K, F,
                            std::sqrt(std::max(0.0, process_->blackVolatility()->blackVariance(T, K, true)))) /
               (K * K);
    };

    // determine lower and upper integration bounds

    Real lower = F, upper = F;

    if (settings_.bounds == VarSwapSettings::Bounds::Fixed) {
        Real tmp = std::max(0.01, T);
        Real stdDev = std::max(0.01, process_->blackVolatility()->blackVol(tmp, F, true)) * std::sqrt(tmp);
        lower = F * std::exp(settings_.fixedMinStdDevs * stdDev);
        upper = F * std::exp(settings_.fixedMaxStdDevs * stdDev);
    } else if (settings_.bounds == VarSwapSettings::Bounds::PriceThreshold) {
        Size i = 0, j = 0;
        for (; i < settings_.maxPriceThresholdSteps && replication(lower) > settings_.priceThreshold; ++i)
            lower *= (1.0 - settings_.priceThresholdStep);
        for (; j < settings_.maxPriceThresholdSteps && replication(upper) > settings_.priceThreshold; ++j)
            upper *= (1.0 + settings_.priceThresholdStep);
        QL_REQUIRE(i < settings_.maxPriceThresholdSteps && j < settings_.maxPriceThresholdSteps,
                   "GeneralisedReplicatingVarianceSwapEngine(): far otm call / put prices do not go to zero, put("
                       << lower << ")=" << replication(lower)
                       << " (vol=" << process_->blackVolatility()->blackVol(T, lower, true) << "), call(" << upper
                       << ")=" << replication(upper)
                       << ", vol=" << process_->blackVolatility()->blackVol(T, upper, true) << ", threshold is "
                       << settings_.priceThreshold << ", check validity of volatility surface (are vols exploding?)");
    } else {
        QL_FAIL("GeneralisedReplicationVarianceSwapEngine: internal error, unknown bounds");
    }

    // calculate the integration integral

    try {
        Real res = 0.0;
        if (!close_enough(lower, F))
            res += integrator->operator()(replication, lower, F);
        if (!close_enough(upper, F))
            res += integrator->operator()(replication, F, upper);
        return 2.0 / T * res;
    } catch (const std::exception& e) {
        QL_FAIL("GeneralisedReplicatingVarianceSwapEngine(): error during calculation, check volatility input and "
                "resulting replication integrand: "
                << e.what());
    }
}

} // namespace QuantExt
