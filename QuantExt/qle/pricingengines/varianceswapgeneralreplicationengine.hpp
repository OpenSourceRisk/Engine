/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/varswapengine.hpp
    \brief equity variance swap engine
*/

#pragma once
#include <ql/instruments/varianceswap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/quote.hpp>

#include <ql/instruments/europeanoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/exercise.hpp>

using namespace QuantLib;

namespace QuantExt {

class VarSwapEngine : public VarianceSwap::engine {
public:
    VarSwapEngine(
                //! Equity name (needed to look up fixings)
                const std::string& equityName,
                //! Equity Spot price
                const Handle<Quote>& equityPrice,
                //! Forward curve for equity
                const Handle<YieldTermStructure>& yieldTS,
                //! Equity Dividend Rate
                const Handle<YieldTermStructure>& dividendTS,
                //! Equity Volatility
                const Handle<BlackVolTermStructure>& volTS,
                //! Discounting Curve (may be the same as yieldTS)
                const Handle<YieldTermStructure>& discountingTS,
                //! Number of Puts
                Size numPuts = 11,
                //! Number of Calls
                Size numCalls = 11,
                //! Default Moneyness StepSize for 1Y swap (scaled by sqrt(T))
                Real stepSize = 0.05);

    void calculate() const;
    private:
    Real calculateAccruedVariance() const;
    Real calculateFutureVariance() const;

    std::string equityName_;
    Handle<Quote> equityPrice_;
    Handle<YieldTermStructure> yieldTS_;
    Handle<YieldTermStructure> dividendTS_;
    Handle<BlackVolTermStructure> volTS_;
    Handle<YieldTermStructure> discountingTS_;
    Calendar fixingsCalendar_;
    Size numPuts_;
    Size numCalls_;
    Real stepSize_;
};

class ReplicatingVarianceSwapEngine2 : public VarianceSwap::engine {
public:
    typedef std::vector<std::pair<
        boost::shared_ptr<StrikedTypePayoff>, Real> > weights_type;
    // constructor
    ReplicatingVarianceSwapEngine2(
        const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
        Real dk = 5.0,
        const std::vector<Real>& callStrikes = std::vector<Real>(),
        const std::vector<Real>& putStrikes = std::vector<Real>());
    ReplicatingVarianceSwapEngine2(
        const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
        const Handle<YieldTermStructure> discountingTS,
        Real dk = 5.0,
        const std::vector<Real>& callStrikes = std::vector<Real>(),
        const std::vector<Real>& putStrikes = std::vector<Real>());
    void calculate() const;
protected:
    // helper methods
    void computeOptionWeights(const std::vector<Real>&,
        const Option::Type,
        weights_type& optionWeights) const;
    Real computeLogPayoff(const Real, const Real) const;
    Real computeReplicatingPortfolio(
        const weights_type& optionWeights) const;
    Rate riskFreeRate() const;
    DiscountFactor riskFreeDiscount() const;
    Rate forecastingRate() const;
    DiscountFactor forecastingDiscount() const;
    Real underlying() const;
    Time residualTime() const;
    Calendar calendar() const;
private:
    boost::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Handle<YieldTermStructure> discountingTS_;
    Real dk_;
    std::vector<Real> callStrikes_, putStrikes_;
    Calendar  calendar_;
};


// inline definitions

inline ReplicatingVarianceSwapEngine2::ReplicatingVarianceSwapEngine2(
    const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
    const Handle<YieldTermStructure> discountingTS,
    Real dk,
    const std::vector<Real>& callStrikes,
    const std::vector<Real>& putStrikes)
    : process_(process), discountingTS_(discountingTS),
    dk_(dk), callStrikes_(callStrikes), putStrikes_(putStrikes) {

    QL_REQUIRE(process_, "no process given");
    QL_REQUIRE(!callStrikes.empty() && !putStrikes.empty(),
        "no strike(s) given");
    QL_REQUIRE(*std::min_element(putStrikes.begin(), putStrikes.end())>0.0,
        "min put strike must be positive");
    QL_REQUIRE(*std::min_element(callStrikes.begin(), callStrikes.end()) ==
        *std::max_element(putStrikes.begin(), putStrikes.end()),
        "min call and max put strikes differ");
}

inline ReplicatingVarianceSwapEngine2::ReplicatingVarianceSwapEngine2(
    const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
    Real dk,
    const std::vector<Real>& callStrikes,
    const std::vector<Real>& putStrikes)
    : process_(process), dk_(dk),
    callStrikes_(callStrikes), putStrikes_(putStrikes) {
    discountingTS_ = process->riskFreeRate();
    QL_REQUIRE(process_, "no process given");
    QL_REQUIRE(!callStrikes.empty() && !putStrikes.empty(),
        "no strike(s) given");
    QL_REQUIRE(*std::min_element(putStrikes.begin(), putStrikes.end())>0.0,
        "min put strike must be positive");
    QL_REQUIRE(*std::min_element(callStrikes.begin(), callStrikes.end()) ==
        *std::max_element(putStrikes.begin(), putStrikes.end()),
        "min call and max put strikes differ");
}

inline void ReplicatingVarianceSwapEngine2::computeOptionWeights(
    const std::vector<Real>& availStrikes,
    const Option::Type type,
    weights_type& optionWeights) const {
    if (availStrikes.empty())
        return;

    std::vector<Real> strikes = availStrikes;

    // add end-strike for piecewise approximation
    switch (type) {
    case Option::Call:
        std::sort(strikes.begin(), strikes.end());
        strikes.push_back(strikes.back() + dk_);
        break;
    case Option::Put:
        std::sort(strikes.begin(), strikes.end(), std::greater<Real>());
        strikes.push_back(std::max(strikes.back() - dk_, 0.0));
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


inline Real ReplicatingVarianceSwapEngine2::computeLogPayoff(
    const Real strike,
    const Real callPutStrikeBoundary) const {
    Real f = callPutStrikeBoundary;
    return (2.0 / residualTime()) * (((strike - f) / f) - std::log(strike / f));
}


inline
    Real ReplicatingVarianceSwapEngine2::computeReplicatingPortfolio(
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


// calculate variance via replicating portfolio
inline void ReplicatingVarianceSwapEngine2::calculate() const {
    weights_type optionWeigths;
    computeOptionWeights(callStrikes_, Option::Call, optionWeigths);
    computeOptionWeights(putStrikes_, Option::Put, optionWeigths);

    results_.variance = computeReplicatingPortfolio(optionWeigths);

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
    results_.value = multiplier * riskFreeDiscount * arguments_.notional *
        (results_.variance - arguments_.strike);

    results_.additionalResults["optionWeights"] = optionWeigths;
}


inline Real ReplicatingVarianceSwapEngine2::underlying() const {
    return process_->x0();
}


inline Time ReplicatingVarianceSwapEngine2::residualTime() const {
    return process_->time(arguments_.maturityDate);
}


inline Rate ReplicatingVarianceSwapEngine2::riskFreeRate() const {
    return discountingTS_->zeroRate(residualTime(), Continuous,
        NoFrequency, true);
}

inline Rate ReplicatingVarianceSwapEngine2::forecastingRate() const {
    return process_->riskFreeRate()->zeroRate(residualTime(), Continuous,
        NoFrequency, true);
}


inline
    DiscountFactor ReplicatingVarianceSwapEngine2::riskFreeDiscount() const {
    return discountingTS_->discount(residualTime());
}

inline
    DiscountFactor ReplicatingVarianceSwapEngine2::forecastingDiscount() const {
    return process_->riskFreeRate()->discount(residualTime());
}

inline
    Calendar ReplicatingVarianceSwapEngine2::calendar() const {
    return calendar_;
}

}   //namespace QuantExt
