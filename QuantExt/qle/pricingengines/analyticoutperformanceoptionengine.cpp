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

#include <qle/pricingengines/analyticoutperformanceoptionengine.hpp>

#include <ql/currencies/exchangeratemanager.hpp>
#include <ql/exercise.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/math/integrals/gaussianquadratures.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/integrals/kronrodintegral.hpp>

namespace QuantExt {

Real AnalyticOutperformanceOptionEngine::getTodaysFxConversionRate(const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex) const {
    // The fx conversion rate as of today should always be taken from the market data, i.e. we should not use a
    // historical fixing, even if it exists, because we should generate sensitivities to market fx spot rate changes.
    // Furthermore, we can get the fx spot rate from the market data even if today is not a valid fixing date for the
    // fx index, that is why we should not use Index::fixing(today, true).
    Real tmp;
    if (fxIndex->useQuote()) {
        tmp = fxIndex->fxQuote()->value();
    } else {
        tmp = ExchangeRateManager::instance()
                  .lookup(fxIndex->sourceCurrency(), fxIndex->targetCurrency())
                  .rate();
    }
    return tmp;
}

AnalyticOutperformanceOptionEngine::AnalyticOutperformanceOptionEngine(
        const ext::shared_ptr<GeneralizedBlackScholesProcess>& process1,
        const ext::shared_ptr<GeneralizedBlackScholesProcess>& process2,
        const Handle<CorrelationTermStructure>& correlation, Size integrationPoints)
    : process1_(process1), process2_(process2), correlationCurve_(correlation), integrationPoints_(integrationPoints) {
    registerWith(process1_);
    registerWith(process2_);
}

class AnalyticOutperformanceOptionEngine::integrand_f {
    const AnalyticOutperformanceOptionEngine* pricer;
  public:
    explicit integrand_f(const AnalyticOutperformanceOptionEngine* pricer, 
        Real phi, Real k, Real m1, Real m2, Real v1, Real v2, Real s1, Real s2, Real i1, Real i2, Real fixingTime)
    : pricer(pricer), phi_(phi), k_(k), m1_(m1), m2_(m2), v1_(v1), v2_(v2), s1_(s1), s2_(s2), i1_(i1), i2_(i2), fixingTime_(fixingTime) {}
    Real operator()(Real x) const {
        return pricer->integrand(x, phi_, k_, m1_, m2_, v1_, v2_, s1_, s2_, i1_, i2_, fixingTime_);
    }

    Real phi_;
    Real k_;
    Real m1_;
    Real m2_;
    Real v1_;
    Real v2_;
    Real s1_;
    Real s2_;
    Real i1_; 
    Real i2_;
    Real fixingTime_;
};


Real AnalyticOutperformanceOptionEngine::integrand(const Real x, Real phi, Real k, Real m1, Real m2, Real v1, Real v2, Real s1, Real s2, Real i1, Real i2, Real fixingTime) const {

    // this is Brigo, 13.16.2 with x = v / sqrt(2)
    Real v = M_SQRT2 * x;
    
    //a positive real number 'a', a negative real number 'b'
    Real a = 1 / i1;
    Real b = -1 / i2;

    CumulativeNormalDistribution cnd;
    
    Real h =
        k - b * s2 * std::exp((m2 - 0.5 * v2 * v2) * fixingTime +
                                 v2 * std::sqrt(fixingTime) * v);
    Real phi1, phi2;
    phi1 = cnd(
        phi * (std::log(a * s1 / h) +
                (m1 + (0.5 - rho(fixingTime) * rho(fixingTime)) * v1 * v1) * fixingTime +
                rho(fixingTime) * v1 * std::sqrt(fixingTime) * v) /
        (v1 * std::sqrt(fixingTime * (1.0 - rho(fixingTime) * rho(fixingTime)))));
    phi2 = cnd(
        phi * (std::log(a * s1 / h) +
                (m1 - 0.5 * v1 * v1) * fixingTime +
                rho(fixingTime) * v1 * std::sqrt(fixingTime) * v) /
        (v1 * std::sqrt(fixingTime * (1.0 - rho(fixingTime) * rho(fixingTime)))));
    Real f = a * phi * s1 *
                 std::exp(m1 * fixingTime -
                          0.5 * rho(fixingTime) * rho(fixingTime) * v1 * v1 * fixingTime +
                          rho(fixingTime) * v1 * std::sqrt(fixingTime) * v) *
                 phi1 -
             phi * h * phi2;
    return std::exp(-x * x) * f;
}

void AnalyticOutperformanceOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");
        
    Option::Type optionType = arguments_.optionType;
    Real phi = optionType == Option::Call ? 1.0 : -1.0;

    Real strike = arguments_.strikeReturn;
    QL_REQUIRE(strike >= 0, "non-negative strike expected");
    
    Time fixingTime = process1_->time(arguments_.exercise->lastDate());

    Real fx1 = arguments_.fxIndex1 ? getTodaysFxConversionRate(arguments_.fxIndex1) : 1.0;
    Real fx2 = arguments_.fxIndex2 ? getTodaysFxConversionRate(arguments_.fxIndex2) : 1.0;

    Real s1 = process1_->stateVariable()->value(); 
    Real s2 = process2_->stateVariable()->value();
    Real i1 = arguments_.initialValue1 * fx1;
    Real i2 = arguments_.initialValue2 * fx2;

    Real v1 = process1_->blackVolatility()->blackVol(
                                            arguments_.exercise->lastDate(), s1);
    Real v2 = process2_->blackVolatility()->blackVol(
                                            arguments_.exercise->lastDate(), s2);

    Real riskFreeRate1 = process1_->riskFreeRate()->zeroRate(
                arguments_.exercise->lastDate(),
                process1_->riskFreeRate()->dayCounter(),
                Continuous, NoFrequency);
    Real riskFreeRate2 = process2_->riskFreeRate()->zeroRate(
                arguments_.exercise->lastDate(),
                process2_->riskFreeRate()->dayCounter(),
                Continuous, NoFrequency);
    Real dividendYield1 = process1_->dividendYield()->zeroRate(
                arguments_.exercise->lastDate(),
                process1_->dividendYield()->dayCounter(),
                Continuous, NoFrequency);
    Real dividendYield2 = process2_->dividendYield()->zeroRate(
                arguments_.exercise->lastDate(),
                process2_->dividendYield()->dayCounter(),
                Continuous, NoFrequency);

    Real m1 = riskFreeRate1 - dividendYield1;
    Real m2 = riskFreeRate2 - dividendYield2;

    Real k = strike;

    Real res = 0.0;
    
    bool hasBarrier = (arguments_.knockInPrice != Null<Real>()) || (arguments_.knockOutPrice != Null<Real>());
    Real integral = 0;
    if (hasBarrier) {
        Real my = (m2 - 0.5 * v2 * v2) * fixingTime; 
        Real vy = v2 * std::sqrt(fixingTime);
        
        Real precision = 1.0e-6;
        
        Real upperBound = Null<Real>();
        Real lowerBound = Null<Real>();
        if (arguments_.knockOutPrice != Null<Real>()) {
            Real koPrice = fx2 * arguments_.knockOutPrice;
            // for the integration variable y : upperBound = log( knockOutPrice / initialPrice2)
            // in Brigo the variable change v = (y - my) / vy is made
            // we make a further variable change  x = v / sqrt(2)
            upperBound = (std::log(koPrice / s2) - my) / ( vy * M_SQRT2);

            if (arguments_.knockInPrice == Null<Real>()) {
                // we estimate the infinite boundary
                lowerBound = -2 * std::fabs(upperBound);
                while(integrand(lowerBound, phi, k, m1, m2, v1, v2, s1, s2, i1, i2, fixingTime) > precision)
                    lowerBound *=2.0;
            }
        }
        
        if (arguments_.knockInPrice != Null<Real>()) {
            Real kiPrice = fx2 * arguments_.knockInPrice;
            // for the integration variable y : lowerBound = log( knockInPrice / initialPrice2)
            // in Brigo the variable change v = (y - my) / vy is made
            // we make a further variable change  x = v / sqrt(2)
            lowerBound = (std::log(kiPrice / s2) - my) / ( vy * M_SQRT2);

            if (arguments_.knockOutPrice == Null<Real>()) {
                // we estimate the infinite boundary
                upperBound = 2 * std::fabs(lowerBound);
                while(integrand(upperBound, phi, k, m1, m2, v1, v2, s1, s2, i1, i2, fixingTime) > precision)
                    upperBound *=2.0;
            }
        }

        QuantLib::ext::shared_ptr<Integrator> integratorBounded = QuantLib::ext::make_shared<GaussKronrodNonAdaptive>(precision, 1000000, 1.0);
        
        QL_REQUIRE(upperBound != Null<Real>(), "AnalyticOutperformanceOptionEngine: expected valid upper bound.");
        QL_REQUIRE(lowerBound != Null<Real>(), "AnalyticOutperformanceOptionEngine: expected valid lower bound.");
        QL_REQUIRE(upperBound > lowerBound, "incorrect knock in levels provided");
        integral +=  (*integratorBounded)(integrand_f(this, phi, k, m1, m2, v1, v2, s1, s2, i1, i2, fixingTime), lowerBound, upperBound);

    } else {
        QuantLib::ext::shared_ptr<GaussianQuadrature> integrator = QuantLib::ext::make_shared<GaussHermiteIntegration>(integrationPoints_);
        integral +=
            (*integrator)(integrand_f(this, phi, k, m1, m2, v1, v2, s1, s2, i1, i2, fixingTime));
    }

    res += (integral / M_SQRTPI);
    DiscountFactor riskFreeDiscount =
        process1_->riskFreeRate()->discount(arguments_.exercise->lastDate());

    results_.value = res * riskFreeDiscount * arguments_.notional;

    Real variance1 = process1_->blackVolatility()->blackVariance(
                                            arguments_.exercise->lastDate(), s1);
    Real variance2 = process2_->blackVolatility()->blackVariance(
                                            arguments_.exercise->lastDate(), s2);
    Real variance = variance1 + variance2 - 2 * rho(fixingTime) * std::sqrt(variance1)*std::sqrt(variance2);
    Real stdDev = std::sqrt(variance);
    results_.standardDeviation = stdDev;

    results_.additionalResults["spot1"] = s1;
    results_.additionalResults["spot2"] = s2;
    results_.additionalResults["fx1"] = fx1;
    results_.additionalResults["fx2"] = fx2;
    results_.additionalResults["blackVol1"] = v1;
    results_.additionalResults["blackVol2"] = v2;
    results_.additionalResults["correlation"] = rho(fixingTime);
    results_.additionalResults["strike"] = strike;
    results_.additionalResults["residualTime"] = fixingTime;
    results_.additionalResults["riskFreeRate1"] = riskFreeRate1;
    results_.additionalResults["riskFreeRate2"] = riskFreeRate2;
    results_.additionalResults["dividendYield1"] = dividendYield1;
    results_.additionalResults["dividendYield2"] = dividendYield2;
    
} // calculate
} // namespace QuantExt
