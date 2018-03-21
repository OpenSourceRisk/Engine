/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/varswapengine.cpp
    \brief equity variance swap engine
*/

#include <qle/pricingengines/varswapengine.hpp>
#include <ql/pricingengines/forward/replicatingvarianceswapengine.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/indexes/indexmanager.hpp>

using std::string;
using namespace QuantLib;

namespace QuantExt {

VarSwapEngine::VarSwapEngine( const string& equityName,
            const Handle<Quote>& equityPrice,
            const Handle<YieldTermStructure>& yieldTS,
            const Handle<YieldTermStructure>& dividendTS,
            const Handle<BlackVolTermStructure>& volTS,
            const Handle<YieldTermStructure>& discountingTS,
            Size numPuts,
            Size numCalls,
            Real stepSize) : 
        equityName_(equityName),
        equityPrice_(equityPrice),
        yieldTS_(yieldTS),
        dividendTS_(dividendTS),
        volTS_(volTS),
        discountingTS_(discountingTS),
        numPuts_(numPuts),
        numCalls_(numCalls),
        stepSize_(stepSize) {

QL_REQUIRE(!equityPrice_.empty(), "empty equity quote handle");
QL_REQUIRE(!yieldTS_.empty(), "empty yield term structure handle");
QL_REQUIRE(!dividendTS_.empty(), "empty dividend term structure handle");
QL_REQUIRE(!volTS_.empty(), "empty equity vol term structure handle");
QL_REQUIRE(!discountingTS_.empty(), "empty discounting term structure handle");
        
QL_REQUIRE(numPuts_ > 0, "Invalid number of Puts, must be > 0");
QL_REQUIRE(numCalls_ > 0, "Invalid number of Calls, must be > 0");
QL_REQUIRE(stepSize_ > 0, "Invalid stepSize, must be > 0");

registerWith(equityPrice_);
registerWith(yieldTS_);
registerWith(dividendTS_);
registerWith(volTS_);
registerWith(discountingTS_);
}

void VarSwapEngine::calculate() const {

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
    } else if (arguments_.startDate == today) {
        // The only time the QL price works
        variance = calculateFutureVariance();
    } else {
        // Get weighted average of Future and Realised variancies.
        Real futVar = calculateFutureVariance();
        Real accVar = calculateAccruedVariance();

        Real totalTime = (arguments_.maturityDate - arguments_.startDate) / 365.0;
        Real accTime = (today - arguments_.startDate) / 365.0;
        Real futTime = (arguments_.maturityDate - today) / 365.0;

        variance = (accVar * accTime / totalTime) + (futVar * futTime / totalTime);
    }

    DiscountFactor df = discountingTS_->discount(arguments_.maturityDate);
    Real multiplier = arguments_.position == Position::Long ? 1.0 : -1.0;

    results_.variance = variance;
    results_.value = multiplier * df * arguments_.notional * (variance - arguments_.strike);
}

Real VarSwapEngine::calculateAccruedVariance() const {
    // return annualised accrued variance
    string eqIndex = "EQ_" + equityName_;
    QL_REQUIRE(IndexManager::instance().hasHistory (eqIndex), "No historical fixings for " << eqIndex);
    const TimeSeries<Real>& history = IndexManager::instance().getHistory(eqIndex);

    Calendar cal = TARGET(); // FIXME: Should be part of instrument really, but that is in QL.

    // Calculate historical variance from arguments_.startDate to today
    Date today = Settings::instance().evaluationDate();

    Real variance = 0.0;
    Size counter = 0;
    Date firstDate = cal.advance(arguments_.startDate, -1, Days);
    Real last = history[firstDate];
    //QL_REQUIRE(last != Null<Real>(), "No fixing for " << eqIndex << " on date " << firstDate);
    if (last == Null<Real>()) {
        last = history[arguments_.startDate]; // just assume a flat move for now.
        QL_REQUIRE(last != Null<Real>(), "No fixing for " << eqIndex << " on date " << firstDate);
    }

    for (Date d = arguments_.startDate; d < today; d = cal.advance(d, 1, Days)) {
        Real price = history[d];
        QL_REQUIRE(price != Null<Real>(), "No fixing for " << eqIndex << " on date " << d);
        Real move = log(price/last);
        variance += move * move;
        counter ++;
        last = price;
    }
    // Now do final move. Yesterday is a fixing, todays price is in equityPrice_
    Real lastMove = log(equityPrice_->value()/last);
    variance += lastMove * lastMove;
    counter ++;

    // Annualize
    Size days = 255; // FIXME: get days from calendar - but the instrument doesn't have one!
    return days * variance / counter;
}

Real VarSwapEngine::calculateFutureVariance() const {

    Date today = Settings::instance().evaluationDate();
    Real time = ActualActual().yearFraction(today, arguments_.maturityDate);


    // Use a replicatingVarianceSwapEngine to price the future Variance segment of the Leg
    boost::shared_ptr<VarianceSwap> vs(new VarianceSwap(Position::Long, arguments_.strike,
                                                        1.0, today, arguments_.maturityDate));

    //  The pillars of the IV surface (to my knowledge) are usually quoted in terms of the spot at the maturities for which varswaps are more common so I'm going with spot instead.
    //  Real fwd = equityPrice_->value() * dividendTS_->discount(time) / yieldTS_->discount(time);
    Real dMoneyness = stepSize_ * sqrt(time);
    QL_REQUIRE(numPuts_ * dMoneyness < 1, "Variance swap engine: too many puts or too large a moneyness step specified. If #puts * step size * sqrt(time) >=1 this would lead to negative strikes in the replicating options.");

    std::vector<Real> callStrikes (numCalls_);
    for (Size i = 0; i < numCalls_; i++)
        callStrikes[i] = equityPrice_->value() * (1 + i*dMoneyness);

    std::vector<Real> putStrikes (numPuts_);
    for (Size i = 0; i < numPuts_; i++)
        putStrikes[i] = equityPrice_->value() * (1 - i*dMoneyness);

    boost::shared_ptr<GeneralizedBlackScholesProcess> process(
        new BlackScholesMertonProcess(equityPrice_, dividendTS_, yieldTS_, volTS_));

    boost::shared_ptr<PricingEngine> vsEng(
        new ReplicatingVarianceSwapEngine(process, dMoneyness*100, callStrikes, putStrikes));

    vs->setPricingEngine(vsEng);
    return vs->variance();
}

}

