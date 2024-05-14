/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2007 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

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

#include <qle/pricingengines/analyticeuropeanforwardengine.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/exercise.hpp>

using QuantLib::Date;
using QuantLib::DiscountFactor;
using QuantLib::GeneralizedBlackScholesProcess;
using QuantLib::Handle;
using QuantLib::Null;
using QuantLib::PricingEngine;
using QuantLib::Real;
using QuantLib::Settings;
using QuantLib::StrikedTypePayoff;
using QuantLib::Time;
using QuantLib::VanillaOption;
using QuantLib::YieldTermStructure;
using QuantLib::Handle;
using QuantLib::Exercise;
using QuantLib::DayCounter;
using QuantLib::Error;

namespace QuantExt {

    AnalyticEuropeanForwardEngine::AnalyticEuropeanForwardEngine(
             const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& process)
    : process_(process) {
        registerWith(process_);
    }

    AnalyticEuropeanForwardEngine::AnalyticEuropeanForwardEngine(
             const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& process,
             const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve)
    : process_(process), discountCurve_(discountCurve) {
        registerWith(process_);
        registerWith(discountCurve_);
    }

    void AnalyticEuropeanForwardEngine::calculate() const {

        // if the discount curve is not specified, we default to the
        // risk free rate curve embedded within the GBM process
        QuantLib::ext::shared_ptr<YieldTermStructure> discountPtr = 
            discountCurve_.empty() ? 
            process_->riskFreeRate().currentLink() :
            discountCurve_.currentLink();

        QL_REQUIRE(arguments_.exercise->type() == Exercise::European,
                   "not an European option");

        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff =
            QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-striked payoff given");

        Real variance =
            process_->blackVolatility()->blackVariance(
                                              arguments_.exercise->lastDate(),
                                              payoff->strike());
        DiscountFactor dividendDiscount =
            process_->dividendYield()->discount(
                                             arguments_.forwardDate);
        DiscountFactor df;
        if (arguments_.paymentDate != Date())
            df = discountPtr->discount(arguments_.paymentDate);
        else
            df = discountPtr->discount(arguments_.exercise->lastDate());
        DiscountFactor riskFreeDiscountForFwdEstimation =
            process_->riskFreeRate()->discount(arguments_.forwardDate);
        Real spot = process_->stateVariable()->value();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");
        Real forwardPrice = spot * dividendDiscount / riskFreeDiscountForFwdEstimation;

        QuantLib::BlackCalculator black(payoff, forwardPrice, std::sqrt(variance),df);


        results_.value = black.value();
        results_.delta = black.delta(spot);
        results_.deltaForward = black.deltaForward();
        results_.elasticity = black.elasticity(spot);
        results_.gamma = black.gamma(spot);

        DayCounter rfdc  = discountPtr->dayCounter();
        DayCounter divdc = process_->dividendYield()->dayCounter();
        DayCounter voldc = process_->blackVolatility()->dayCounter();
        Time t = rfdc.yearFraction(process_->riskFreeRate()->referenceDate(),
                                   arguments_.exercise->lastDate());
        results_.rho = black.rho(t);

        t = divdc.yearFraction(process_->dividendYield()->referenceDate(),
                               arguments_.exercise->lastDate());
        results_.dividendRho = black.dividendRho(t);

        t = voldc.yearFraction(process_->blackVolatility()->referenceDate(),
                               arguments_.exercise->lastDate());
        results_.vega = black.vega(t);
        try {
            results_.theta = black.theta(spot, t);
            results_.thetaPerDay =
                black.thetaPerDay(spot, t);
        } catch (Error&) {
            results_.theta = Null<Real>();
            results_.thetaPerDay = Null<Real>();
        }

        results_.strikeSensitivity  = black.strikeSensitivity();
        results_.itmCashProbability = black.itmCashProbability();

        Real tte = process_->blackVolatility()->timeFromReference(arguments_.exercise->lastDate());
        results_.additionalResults["spot"] = spot;
        results_.additionalResults["dividendDiscount"] = dividendDiscount;
        results_.additionalResults["riskFreeDiscount"] = riskFreeDiscountForFwdEstimation;
        results_.additionalResults["forward"] = forwardPrice;
        results_.additionalResults["strike"] = payoff->strike();
        results_.additionalResults["volatility"] = std::sqrt(variance / tte);
        results_.additionalResults["timeToExpiry"] = tte;
        results_.additionalResults["discountFactor"] = df;
    }

}

