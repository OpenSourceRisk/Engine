/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/analyticeuropeanenginedeltagamma.hpp>
#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <ql/exercise.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

AnalyticEuropeanEngineDeltaGamma::AnalyticEuropeanEngineDeltaGamma(
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process, const std::vector<Time>& bucketTimesDeltaGamma,
    const std::vector<Time>& bucketTimesVega, const bool computeDeltaVega, const bool computeGamma, bool linearInZero)
    : process_(process), bucketTimesDeltaGamma_(bucketTimesDeltaGamma), bucketTimesVega_(bucketTimesVega),
      computeDeltaVega_(computeDeltaVega), computeGamma_(computeGamma), linearInZero_(linearInZero) {
    registerWith(process_);
    QL_REQUIRE((!bucketTimesDeltaGamma_.empty() && !bucketTimesVega_.empty()) || (!computeDeltaVega && !computeGamma),
               "bucket times are empty, although sensitivities have to be calculated");
}

void AnalyticEuropeanEngineDeltaGamma::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");

    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff, "non-striked payoff given");

    Real variance = process_->blackVolatility()->blackVariance(arguments_.exercise->lastDate(), payoff->strike());
    DiscountFactor dividendDiscount = process_->dividendYield()->discount(arguments_.exercise->lastDate());
    DiscountFactor riskFreeDiscount = process_->riskFreeRate()->discount(arguments_.exercise->lastDate());
    Real spot = process_->stateVariable()->value();
    QL_REQUIRE(spot > 0.0, "negative or null underlying given");
    Real forwardPrice = spot * dividendDiscount / riskFreeDiscount;

    Option::Type type = payoff->optionType();
    Real w = type == Option::Call ? 1.0 : -1.0;
    Real strike = payoff->strike();
    Real stdDev = std::sqrt(variance);

    Real npv = riskFreeDiscount * blackFormula(type, strike, forwardPrice, std::sqrt(variance), 1.0, 0.0);
    results_.value = npv;

    Date referenceDate = process_->riskFreeRate()->referenceDate();
    Date exerciseDate = arguments_.exercise->lastDate();

    // the vol structure day counter is the unique one we are using for consistency reasons
    DayCounter dc = process_->blackVolatility()->dayCounter();
    Time t = dc.yearFraction(referenceDate, exerciseDate);

    CumulativeNormalDistribution cnd;
    Real d1 = std::log(forwardPrice / strike) / stdDev + 0.5 * stdDev;

    Real npvr = 0.0, npvq = 0.0, npvs = 0.0;
    if (computeDeltaVega_) {
        npvs = w * cnd(w * d1) * dividendDiscount;
        results_.additionalResults["deltaSpot"] = npvs;
        Real singleVega =
            std::sqrt(t) * blackFormulaStdDevDerivative(strike, forwardPrice, stdDev, 1.0, 0.0) * riskFreeDiscount;
        std::map<Date, Real> vegaRaw;
        vegaRaw[exerciseDate] = singleVega;
        std::vector<Real> resVega = detail::rebucketDeltas(bucketTimesVega_, vegaRaw, referenceDate, dc, true);
        results_.additionalResults["vega"] = resVega;
        std::map<Date, Real> deltaRateRaw, deltaDividendRaw;
        Real tmp = dividendDiscount * w * cnd(w * d1);
        npvr = t * (-npv + spot * tmp);
        deltaRateRaw[exerciseDate] = npvr;
        npvq = -t * spot * tmp;
        deltaDividendRaw[exerciseDate] = npvq;
        std::vector<Real> resDeltaRate =
            detail::rebucketDeltas(bucketTimesDeltaGamma_, deltaRateRaw, referenceDate, dc, linearInZero_);
        results_.additionalResults["deltaRate"] = resDeltaRate;
        std::vector<Real> resDeltaDividend =
            detail::rebucketDeltas(bucketTimesDeltaGamma_, deltaDividendRaw, referenceDate, dc, linearInZero_);
        results_.additionalResults["deltaDividend"] = resDeltaDividend;
    }

    if (computeGamma_) {
        Real tmp = cnd.derivative(d1) / (forwardPrice * stdDev);
        results_.additionalResults["gammaSpot"] = tmp * dividendDiscount * dividendDiscount / riskFreeDiscount;
        std::map<Date, Real> gammaRateRaw;
        std::map<std::pair<Date, Date>, Real> gammaDivRaw, gammaRateDivRaw;
        gammaRateRaw[exerciseDate] =
            t * (-npvr + t * spot * spot * dividendDiscount * dividendDiscount / riskFreeDiscount * tmp);
        gammaDivRaw[std::make_pair(exerciseDate, exerciseDate)] =
            -t * (npvq - t * spot * spot * dividendDiscount * dividendDiscount / riskFreeDiscount * tmp);
        gammaRateDivRaw[std::make_pair(exerciseDate, exerciseDate)] =
            t * (-npvq - t * spot * dividendDiscount * w * cnd(w * d1) -
                 t * spot * spot * dividendDiscount * dividendDiscount / riskFreeDiscount * tmp);
        Matrix resGamma = detail::rebucketGammas(bucketTimesDeltaGamma_, gammaRateRaw, gammaDivRaw, gammaRateDivRaw,
                                                 true, referenceDate, dc, linearInZero_);
        results_.additionalResults["gamma"] = resGamma;
        std::map<Date, Real> gammaSpotRateRaw, gammaSpotDivRaw;
        gammaSpotRateRaw[exerciseDate] = t * (-npvs + dividendDiscount * w * cnd(w * d1) +
                                              spot * dividendDiscount * dividendDiscount / riskFreeDiscount * tmp);
        gammaSpotDivRaw[exerciseDate] = -t * (dividendDiscount * w * cnd(w * d1) +
                                              spot * dividendDiscount * dividendDiscount / riskFreeDiscount * tmp);
        std::vector<Real> resGammaSpotRate =
            detail::rebucketDeltas(bucketTimesDeltaGamma_, gammaSpotRateRaw, referenceDate, dc, linearInZero_);

        results_.additionalResults["gammaSpotRate"] = resGammaSpotRate;
        std::vector<Real> resGammaSpotDiv =
            detail::rebucketDeltas(bucketTimesDeltaGamma_, gammaSpotDivRaw, referenceDate, dc, linearInZero_);

        results_.additionalResults["gammaSpotDiv"] = resGammaSpotDiv;
    }

    results_.additionalResults["bucketTimesDeltaGamma"] = bucketTimesDeltaGamma_;
    results_.additionalResults["bucketTimesVega"] = bucketTimesVega_;

} // calculate
} // namespace QuantExt
