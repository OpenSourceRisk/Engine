/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/varswapengine.cpp
    \brief equity variance swap engine
*/

#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/indexes/indexmanager.hpp>

using std::string;
using namespace QuantLib;

namespace QuantExt {

GeneralisedReplicatingVarianceSwapEngine::GeneralisedReplicatingVarianceSwapEngine( const string& equityName,
            const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
            const Handle<YieldTermStructure>& discountingTS,
            const Calendar& calendar,
            Size numPuts,
            Size numCalls,
            Real stepSize) : 
        equityName_(equityName),
        process_(process),
        discountingTS_(discountingTS),
        calendar_(calendar),
        numPuts_(numPuts),
        numCalls_(numCalls),
        stepSize_(stepSize) {

QL_REQUIRE(process_, "Black-Scholes process not present.");
QL_REQUIRE(!discountingTS_.empty(), "Empty discounting term structure handle");
QL_REQUIRE(!calendar_.empty(), "Calendar not provided.");
QL_REQUIRE(numPuts_ > 0, "Invalid number of Puts, must be > 0");
QL_REQUIRE(numCalls_ > 0, "Invalid number of Calls, must be > 0");
QL_REQUIRE(stepSize_ > 0, "Invalid stepSize, must be > 0");

registerWith(process_);
registerWith(discountingTS_);
}

void GeneralisedReplicatingVarianceSwapEngine::calculate() const {

    results_.value = 0.0;

    Date today = Settings::instance().evaluationDate();

    if (today >= arguments_.maturityDate)
        return;

    // Variance is defined here as the annualised volatility squared.
    Real variance = 0;
    if (arguments_.startDate > today) {
        // forward starting.
        QL_FAIL("Cannot price Forward starting variance swap");
        //variance = calculateFutureVariance();
    }
    else if (arguments_.startDate == today) {
        // The only time the QL price works
        variance = calculateFutureVariance();
        results_.additionalResults["accruedVariance"] = 0;
        results_.additionalResults["futureVariance"] = variance;
    }
    else {
        // Get weighted average of Future and Realised variancies.
        Real accVar = calculateAccruedVariance();
        Real futVar = calculateFutureVariance();
        results_.additionalResults["accruedVariance"] = accVar;
        results_.additionalResults["futureVariance"] = futVar;

        Real totalTime = calendar_.businessDaysBetween(arguments_.startDate, arguments_.maturityDate, true, true);
        Real accTime = calendar_.businessDaysBetween(arguments_.startDate, today, true, true);
        Real futTime = calendar_.businessDaysBetween(today, arguments_.maturityDate, false, true);

        variance = (accVar * accTime / totalTime) + (futVar * futTime / totalTime);
    }

    DiscountFactor df = discountingTS_->discount(arguments_.maturityDate);
    Real multiplier = arguments_.position == Position::Long ? 1.0 : -1.0;

    results_.variance = variance;
    results_.value = multiplier * df * arguments_.notional * 10000.0 * (variance - arguments_.strike); //factor of 10000 to convert vols to market quotes
}

Real GeneralisedReplicatingVarianceSwapEngine::calculateAccruedVariance() const {
    // return annualised accrued variance
    string eqIndex = "EQ/" + equityName_;
    QL_REQUIRE(IndexManager::instance().hasHistory(eqIndex), "No historical fixings for " << eqIndex);
    const TimeSeries<Real>& history = IndexManager::instance().getHistory(eqIndex);

    // Calculate historical variance from arguments_.startDate to today
    Date today = Settings::instance().evaluationDate();

    Real variance = 0.0;
    Size counter = 0;
    Date firstDate = calendar_.advance(arguments_.startDate, -1, Days);
    Real last = history[firstDate];
    QL_REQUIRE(last != Null<Real>(), "No fixing for " << eqIndex << " on date " << firstDate << ". This is required for fixing the return on the first day of the variance swap.");

    for (Date d = arguments_.startDate; d < today; d = calendar_.advance(d, 1, Days)) {
        Real price = history[d];
        QL_REQUIRE(price != Null<Real>(), "No fixing for " << eqIndex << " on date " << d);
        Real move = log(price / last);
        variance += move * move;
        counter++;
        last = price;
    }

    // Now do final move. Yesterday is a fixing, todays price is in equityPrice_
    Real lastMove = log(process_->x0() / last);
    variance += lastMove * lastMove;
    counter++;

    // Annualize
    Size days = 252; // FIXME: maybe get days from calendar?
    return days * variance / counter;
}

Real GeneralisedReplicatingVarianceSwapEngine::calculateFutureVariance() const {

    Date today = Settings::instance().evaluationDate();
    Real timeToMaturity = ActualActual().yearFraction(today, arguments_.maturityDate);

    //  The pillars of the IV surface are usually quoted in terms of the spot at the maturities for which varswaps are more common.
    Real dMoneyness = stepSize_ * sqrt(timeToMaturity);
    QL_REQUIRE(numPuts_ * dMoneyness < 1, "Variance swap engine: too many puts or too large a moneyness step specified. If #puts * step size * sqrt(timeToMaturity) >=1 this would lead to negative strikes in the replicating options.");

    std::vector<Real> callStrikes(numCalls_);
    for (Size i = 0; i < numCalls_; i++)
        callStrikes[i] = process_->x0() * (1 + i * dMoneyness);

    std::vector<Real> putStrikes(numPuts_);
    for (Size i = 0; i < numPuts_; i++)
        putStrikes[i] = process_->x0() * (1 - i * dMoneyness);

    QL_REQUIRE(!callStrikes.empty() && !putStrikes.empty(),
        "no strike(s) given");
    QL_REQUIRE(*std::min_element(putStrikes.begin(), putStrikes.end())>0.0,
        "min put strike must be positive");
    QL_REQUIRE(*std::min_element(callStrikes.begin(), callStrikes.end()) ==
        *std::max_element(putStrikes.begin(), putStrikes.end()),
        "min call and max put strikes differ");

    weights_type optionWeights;
    computeOptionWeights(callStrikes, Option::Call, optionWeights, dMoneyness*  100.0);
    computeOptionWeights(putStrikes, Option::Put, optionWeights, dMoneyness * 100.0);

    Real variance = computeReplicatingPortfolio(optionWeights);

    DiscountFactor riskFreeDiscount =
        process_->riskFreeRate()->discount(arguments_.maturityDate);
    Real multiplier;
    switch (arguments_.position) {
    case Position::Long:
        multiplier = 1.0;
        break;
    case Position::Short:
        multiplier = -1.0;
        break;
    default:
        QL_FAIL("Unknown position");
    }
    results_.additionalResults["optionWeights"] = optionWeights;

    return variance;
}

void GeneralisedReplicatingVarianceSwapEngine::computeOptionWeights(
    const std::vector<Real>& availStrikes,
    const Option::Type type,
    weights_type& optionWeights,
    Real dk) const {
    if (availStrikes.empty())
        return;

    std::vector<Real> strikes = availStrikes;

    // add end-strike for piecewise approximation
    switch (type) {
    case Option::Call:
        std::sort(strikes.begin(), strikes.end());
        strikes.push_back(strikes.back() + dk);
        break;
    case Option::Put:
        std::sort(strikes.begin(), strikes.end(), std::greater<Real>());
        strikes.push_back(std::max(strikes.back() - dk, 0.0));
        break;
    default:
        QL_FAIL("invalid option type");
    }

    // remove duplicate strikes
    std::vector<Real>::iterator last =
        std::unique(strikes.begin(), strikes.end());
    strikes.erase(last, strikes.end());

    // compute weights
    Real f = strikes.front();
    Real slope, prevSlope = 0.0;

    for (std::vector<Real>::const_iterator k = strikes.begin();
        // added end-strike discarded
        k<strikes.end() - 1;
        ++k) {
        slope = std::fabs((computeLogPayoff(*(k + 1), f) -
            computeLogPayoff(*k, f)) /
            (*(k + 1) - *k));
        boost::shared_ptr<StrikedTypePayoff> payoff(
            new PlainVanillaPayoff(type, *k));
        if (k == strikes.begin())
            optionWeights.push_back(std::make_pair(payoff, slope));
        else
            optionWeights.push_back(
                std::make_pair(payoff, slope - prevSlope));
        prevSlope = slope;
    }
}

Real GeneralisedReplicatingVarianceSwapEngine::computeReplicatingPortfolio(
    const weights_type& optionWeights) const {

    boost::shared_ptr<Exercise> exercise(
        new EuropeanExercise(arguments_.maturityDate));
    boost::shared_ptr<PricingEngine> optionEngine(
        new AnalyticEuropeanEngine(process_, discountingTS_));
    Real optionsValue = 0.0;

    for (weights_type::const_iterator i = optionWeights.begin();
        i < optionWeights.end(); ++i) {
        boost::shared_ptr<StrikedTypePayoff> payoff = i->first;
        EuropeanOption option(payoff, exercise);
        option.setPricingEngine(optionEngine);
        Real weight = i->second;
        optionsValue += option.NPV() * weight;
    }

    Real f = optionWeights.front().first->strike();
    return 2.0 * forecastingRate() -
        2.0 / residualTime() *
        (((underlying() / forecastingDiscount() - f) / f) +
            std::log(f / underlying())) +
        optionsValue / riskFreeDiscount();
}

Real GeneralisedReplicatingVarianceSwapEngine::computeLogPayoff(
    const Real strike,
    const Real callPutStrikeBoundary) const {
    Real f = callPutStrikeBoundary;
    return (2.0 / residualTime()) * (((strike - f) / f) - std::log(strike / f));
}
}

