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

#include <qle/pricingengines/analyticeuropeanengineautodeltahedge.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

AnalyticEuropeanEngineAutoDeltaHedge::AnalyticEuropeanEngineAutoDeltaHedge(
    const Handle<Quote>& spot,
    const Handle<YieldTermStructure>& discountCurve,
    const Handle<YieldTermStructure>& dividendCurve,
    const Handle<YieldTermStructure>& forecastCurve,
    const Handle<BlackVolTermStructure>& marketVol,
    const Handle<QuantExt::EquityIndex2>& equityIndex)
    : spot_(spot), discountCurve_(discountCurve), dividendCurve_(dividendCurve),
      forecastCurve_(forecastCurve), marketVol_(marketVol), equityIndex_(equityIndex) {
    registerWith(spot_);
    registerWith(discountCurve_);
    registerWith(dividendCurve_);
    registerWith(forecastCurve_);
    registerWith(marketVol_);
    registerWith(equityIndex_);
}

void AnalyticEuropeanEngineAutoDeltaHedge::calculate() const {

    Date today = Settings::instance().evaluationDate();
    Real hedgingVol = arguments_.hedgingVolatility;
    Real driftRate = arguments_.driftRate;
    Date obsStartDate = arguments_.observationStartDate;
    bool hedgingStarted = (today >= obsStartDate);

    Calendar fixingCal = equityIndex_->fixingCalendar();
    DayCounter dc = Actual365Fixed();
    CumulativeNormalDistribution N_cdf;

    Real totalNpv = 0.0;

    for (Size i = 0; i < arguments_.batches.size(); ++i) {
        const auto& batch = arguments_.batches[i];
        Real K = batch.strike;
        Real Ni = batch.quantity;
        Real premium = batch.premium;
        Date expiryDate = batch.expiryDate;
        Option::Type type = batch.type;
        Real phi = (type == Option::Call) ? 1.0 : -1.0;

        auto payoff = QuantLib::ext::make_shared<PlainVanillaPayoff>(type, K);
        auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

        // --- Realized past hedge P&L ---
        Real realizedHedgePnL = 0.0;

        if (hedgingStarted) {
            Date obsDate = fixingCal.adjust(obsStartDate, Following);

            Real prevFwd = Null<Real>();
            Real prevDelta = 0.0;

            for (Date d = obsDate; d <= today; d = fixingCal.advance(d, 1, Days)) {
                Real spotD = equityIndex_->fixing(d);

                Time tToExpiry = dc.yearFraction(d, expiryDate);
                Real fwdD = spotD * std::exp(driftRate * tToExpiry);

                if (prevFwd != Null<Real>()) {
                    realizedHedgePnL += prevDelta * (fwdD - prevFwd);
                }

                if (tToExpiry > 0.0) {
                    Real stdDev = hedgingVol * std::sqrt(tToExpiry);
                    Real d1 = (std::log(fwdD / K) + 0.5 * stdDev * stdDev) / stdDev;
                    prevDelta = N_cdf(phi * d1);
                } else {
                    prevDelta = 0.0;
                }

                prevFwd = fwdD;
            }
        }

        // --- Market-vol option: PV of E[Option_Final^i] ---
        auto marketOption = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
        auto marketProcess = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            spot_, dividendCurve_, forecastCurve_, marketVol_);
        marketOption->setPricingEngine(
            QuantLib::ext::make_shared<QuantLib::AnalyticEuropeanEngine>(marketProcess, discountCurve_));
        Real marketOptionNpv = marketOption->NPV();

        // --- Future hedge P&L: BS option at hedging vol ---
        Real hedgeOptionNpv = 0.0;
        if (hedgingStarted && today < expiryDate) {
            auto hedgeOption = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
            Handle<BlackVolTermStructure> hedgeVol(QuantLib::ext::make_shared<BlackConstantVol>(
                today, NullCalendar(), hedgingVol, Actual365Fixed()));
            hedgeVol->enableExtrapolation();
            Handle<YieldTermStructure> fwdRateCurve(QuantLib::ext::make_shared<FlatForward>(
                today, driftRate, Actual365Fixed()));
            Handle<YieldTermStructure> zeroRateCurve(QuantLib::ext::make_shared<FlatForward>(
                today, 0.0, Actual365Fixed()));
            auto hedgeProcess = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                spot_, zeroRateCurve, fwdRateCurve, hedgeVol);
            hedgeOption->setPricingEngine(
                QuantLib::ext::make_shared<QuantLib::AnalyticEuropeanEngine>(hedgeProcess, discountCurve_));
            hedgeOptionNpv = hedgeOption->NPV();
        }

        // --- Discount factor for deterministic cashflows at expiry ---
        DiscountFactor df = discountCurve_->discount(expiryDate);

        // --- Aggregate: N^i * [realizedPnL * DF + C_hedge - C_market + Premium * DF]
        Real batchNpv = Ni * (realizedHedgePnL * df + hedgeOptionNpv - marketOptionNpv + premium * df);
        totalNpv += batchNpv;

        // Store per-batch results
        std::string suffix = std::to_string(i);
        results_.additionalResults["realizedHedgePnL_" + suffix] = realizedHedgePnL;
        results_.additionalResults["hedgeOptionNpv_" + suffix] = hedgeOptionNpv;
        results_.additionalResults["marketOptionNpv_" + suffix] = marketOptionNpv;
        results_.additionalResults["batchNpv_" + suffix] = batchNpv;
    }

    results_.value = totalNpv;
}

} // namespace QuantExt
