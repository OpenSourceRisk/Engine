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

/*! \file qle/pricingengines/varianceswapgeneralreplicationengine.hpp
    \brief equity variance swap engine
    \ingroup engines
*/

#ifndef quantext_varswap_engine_hpp
#define quantext_varswap_engine_hpp

#include <ql/instruments/varianceswap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/quote.hpp>

#include <ql/instruments/europeanoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/exercise.hpp>

using namespace QuantLib;

namespace QuantExt {

class GeneralisedReplicatingVarianceSwapEngine : public VarianceSwap::engine {
public:
    typedef std::vector<std::pair<
        boost::shared_ptr<StrikedTypePayoff>, Real> > weights_type;
    GeneralisedReplicatingVarianceSwapEngine(
        //! Equity name (needed to look up fixings)
        const std::string& equityName,
        //! Generalised Brownian Motion
        const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
        //! Discounting Curve (may be the same as yieldTS)
        const Handle<YieldTermStructure>& discountingTS,
        //! Calendar for Accrued Variance
        const Calendar& calendar,
        //! Number of Puts
        Size numPuts = 11,
        //! Number of Calls
        Size numCalls = 11,
        //! Default Moneyness StepSize for 1Y swap (scaled by sqrt(T))
        Real stepSize = 0.05);

    void calculate() const;
protected:
    // helper methods
    void computeOptionWeights(const std::vector<Real>&,
        const Option::Type,
        weights_type& optionWeights, 
        Real dk) const;
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
    Real calculateAccruedVariance() const;
    Real calculateFutureVariance() const;

    std::string equityName_;
    boost::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Handle<YieldTermStructure> discountingTS_;
    Calendar calendar_;
    Size numPuts_;
    Size numCalls_;
    Real stepSize_;
};

// inline definitions

inline Real GeneralisedReplicatingVarianceSwapEngine::underlying() const {
    return process_->x0();
}


inline Time GeneralisedReplicatingVarianceSwapEngine::residualTime() const {
    return ActualActual().yearFraction(Settings::instance().evaluationDate(), arguments_.maturityDate);
}


inline Rate GeneralisedReplicatingVarianceSwapEngine::riskFreeRate() const {
    return discountingTS_->zeroRate(residualTime(), Continuous,
        NoFrequency, true);
}

inline Rate GeneralisedReplicatingVarianceSwapEngine::forecastingRate() const {
    return process_->riskFreeRate()->zeroRate(residualTime(), Continuous,
        NoFrequency, true);
}


inline
    DiscountFactor GeneralisedReplicatingVarianceSwapEngine::riskFreeDiscount() const {
    return discountingTS_->discount(residualTime());
}

inline
    DiscountFactor GeneralisedReplicatingVarianceSwapEngine::forecastingDiscount() const {
    return process_->riskFreeRate()->discount(residualTime());
}

inline
    Calendar GeneralisedReplicatingVarianceSwapEngine::calendar() const {
    return calendar_;
}

}   //namespace QuantExt

#endif
