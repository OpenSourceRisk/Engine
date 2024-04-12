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

#include <ored/scripting/engines/analyticxccyblackriskparticipationagreementengine.hpp>
#include <qle/models/representativefxoption.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/compositequote.hpp>

namespace ore {
namespace data {

AnalyticXCcyBlackRiskParticipationAgreementEngine::AnalyticXCcyBlackRiskParticipationAgreementEngine(
    const std::string& baseCcy, const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
    const std::map<std::string, Handle<Quote>>& fxSpots, const Handle<DefaultProbabilityTermStructure>& defaultCurve,
    const Handle<Quote>& recoveryRate, const Handle<BlackVolTermStructure>& volatility,
    const bool alwaysRecomputeOptionRepresentation, const Size maxGapDays, const Size maxDiscretisationPoints)
    : RiskParticipationAgreementBaseEngine(baseCcy, discountCurves, fxSpots, defaultCurve, recoveryRate, maxGapDays,
                                           maxDiscretisationPoints),
      volatility_(volatility), alwaysRecomputeOptionRepresentation_(alwaysRecomputeOptionRepresentation) {
    registerWith(volatility_);
}

Real AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv() const {

    QL_REQUIRE(arguments_.exercise == nullptr,
               "AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): callability is not supported");

    QL_REQUIRE(!volatility_.empty(),
               "AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty discount curve");

    std::string domCcy = arguments_.underlyingCcys[0];
    std::string forCcy = domCcy;

    for (auto const& ccy : arguments_.underlyingCcys) {
        // take any ccy != domCcy as the for ccy, the matcher will check if all underlying legs are in dom or for ccy
        if (ccy != domCcy)
            forCcy = ccy;
    }

    QL_REQUIRE(!discountCurves_[domCcy].empty(),
               "AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty discount curve for ccy "
                   << domCcy);
    QL_REQUIRE(!discountCurves_[forCcy].empty(),
               "AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty discount curve for ccy "
                   << forCcy);
    QL_REQUIRE(
        !fxSpots_[domCcy].empty(),
        "AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty discount curve for ccy pair "
            << domCcy + baseCcy_);
    QL_REQUIRE(
        !fxSpots_[forCcy].empty(),
        "AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty discount curve for ccy pair "
            << forCcy + baseCcy_);

    struct divide {
        Real operator()(Real x, Real y) const { return x / y; }
    };

    Handle<Quote> fxSpot(QuantLib::ext::make_shared<CompositeQuote<divide>>(fxSpots_[forCcy], fxSpots_[domCcy], divide()));

    // check if we can reuse the fx option representation, otherwise compute it

    if (alwaysRecomputeOptionRepresentation_ || arguments_.optionRepresentationReferenceDate == Date() ||
        referenceDate_ != arguments_.optionRepresentationReferenceDate) {

        results_.optionRepresentationReferenceDate = referenceDate_;
        results_.optionRepresentationPeriods.clear();
        results_.optionRepresentation.clear();

        /* we construct one fx option per floating rate coupon on the midpoint of the accrual period
           but only keep those with an underlying length of at least 1M */

        for (Size i = 0; i < gridDates_.size() - 1; ++i) {
            Date start = gridDates_[i];
            Date end = gridDates_[i + 1];
            Date mid = start + (end - start) / 2;
            // mid might be = reference date degenerate cases where the first two discretisation points
            // are only one day apart from each other
            if (mid + 1 * Months <= arguments_.underlyingMaturity && mid > discountCurves_[baseCcy_]->referenceDate())
                results_.optionRepresentationPeriods.push_back(std::make_tuple(mid, start, end));
        }

        for (auto const& e : results_.optionRepresentationPeriods) {
            Date d = std::get<0>(e);
            QuantExt::RepresentativeFxOptionMatcher matcher(arguments_.underlying, arguments_.underlyingPayer,
                                                  arguments_.underlyingCcys, std::get<0>(e), forCcy, domCcy,
                                                  discountCurves_[forCcy], discountCurves_[domCcy], fxSpot);
            bool instrumentSet = false;
            if (!close_enough(matcher.amount1(), 0.0) && forCcy != domCcy) {
                Real strike = -matcher.amount2() / matcher.amount1();
                if (strike > 0.0 && !close_enough(strike, 0.0)) {
                    // the amounts correspond to an actual FX Option
                    Option::Type type = matcher.amount1() > 0.0 ? Option::Call : Option::Put;
                    results_.optionRepresentation.push_back(QuantLib::ext::make_shared<VanillaOption>(
                        QuantLib::ext::make_shared<PlainVanillaPayoff>(type, strike), QuantLib::ext::make_shared<EuropeanExercise>(d)));
                    results_.optionMultiplier.push_back(std::abs(matcher.amount1()));
                    instrumentSet = true;
                }
            }
            if (!instrumentSet) {
                // otherwise set up a trivial instrument representing the positive part of the amount in dom ccy
                Real amount = std::max(0.0, matcher.amount1() * fxSpot->value() / discountCurves_[domCcy]->discount(d) *
                                                    discountCurves_[forCcy]->discount(d) +
                                                matcher.amount2());
                results_.optionRepresentation.push_back(QuantLib::ext::make_shared<Swap>(
                    std::vector<Leg>{{QuantLib::ext::make_shared<SimpleCashFlow>(amount, d)}}, std::vector<bool>{false}));
                results_.optionMultiplier.push_back(1.0);
            }
        }

    } else {
        results_.optionRepresentationReferenceDate = arguments_.optionRepresentationReferenceDate;
        results_.optionRepresentationPeriods = arguments_.optionRepresentationPeriods;
        results_.optionRepresentation = arguments_.optionRepresentation;
        results_.optionMultiplier = arguments_.optionMultiplier;
        QL_REQUIRE(results_.optionRepresentation.size() == results_.optionRepresentationPeriods.size(),
                   "AnalyticXCcyBlackRiskParticipationAgreementEngine::calculate(): inconsistent option representation "
                   "periods");
        QL_REQUIRE(results_.optionRepresentation.size() == results_.optionMultiplier.size(),
                   "AnalyticXCcyBlackRiskParticipationAgreementEngine::calculate(): inconsistent option multiplier");
    }

    // attach an engine to the representative option (or swap)

    auto swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurves_[domCcy]);
    auto optionEngine = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
        fxSpot, discountCurves_[forCcy], discountCurves_[domCcy], volatility_));

    for (auto s : results_.optionRepresentation) {
        if (auto tmp = QuantLib::ext::dynamic_pointer_cast<VanillaOption>(s))
            s->setPricingEngine(optionEngine);
        else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<Swap>(s))
            s->setPricingEngine(swapEngine);
        else {
            QL_FAIL("AnalyticXCcyBlackRiskParticipationAgreementEngine::protectionLegNpv(): internal error, could not "
                    "cast representative instrument to either VanillaOption or Swap");
        }
    }

    // compute a CVA using the representative options

    QL_REQUIRE(!fxSpots_[domCcy].empty(),
               "AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty fx spot for ccy pair "
                   << domCcy + baseCcy_);

    Real cva = 0.0;
    std::vector<Real> optionPv(results_.optionRepresentationPeriods.size(), 0.0);
    for (Size i = 0; i < results_.optionRepresentationPeriods.size(); ++i) {
        Real pd = defaultCurve_->defaultProbability(std::get<1>(results_.optionRepresentationPeriods[i]),
                                                    std::get<2>(results_.optionRepresentationPeriods[i]));
        Real swpNpv = results_.optionRepresentation[i]->NPV() * results_.optionMultiplier[i];
        cva += pd * (1.0 - effectiveRecoveryRate_) * swpNpv * fxSpots_[domCcy]->value();
        optionPv[i] = swpNpv;
    }

    // detach pricing engine from result swaption representation

    QuantLib::ext::shared_ptr<PricingEngine> emptyEngine;
    for (auto s : results_.optionRepresentation)
        s->setPricingEngine(emptyEngine);

    // set additional results

    std::vector<Real> optionStrikes;
    std::vector<Date> optionExerciseDates;
    for (auto const& r : results_.optionRepresentation) {
        if (auto o = QuantLib::ext::dynamic_pointer_cast<VanillaOption>(r)) {
            if (auto p = QuantLib::ext::dynamic_pointer_cast<PlainVanillaPayoff>(o->payoff())) {
                optionStrikes.push_back(p->strike());
            } else {
                optionStrikes.push_back(0.0);
            }
            if (!o->exercise()->dates().empty()) {
                optionExerciseDates.push_back(o->exercise()->dates().front());
            } else {
                optionExerciseDates.push_back(Date());
            }
        } else if (auto s = QuantLib::ext::dynamic_pointer_cast<Swap>(r)) {
            optionStrikes.push_back(0.0);
            if (!s->leg(0).empty()) {
                optionExerciseDates.push_back(s->leg(0).front()->date());
            } else {
                optionExerciseDates.push_back(Date());
            }
        }
    }

    results_.additionalResults["OptionNpvs"] = optionPv;
    results_.additionalResults["FXSpot"] = fxSpots_[domCcy]->value();
    results_.additionalResults["BaseCurrency"] = baseCcy_;
    results_.additionalResults["DomesticCurrency"] = domCcy;
    results_.additionalResults["ForeignCurrency"] = forCcy;
    results_.additionalResults["OptionMultiplier"] = results_.optionMultiplier;
    results_.additionalResults["OptionStrikes"] = optionStrikes;
    results_.additionalResults["OptionExerciseDates"] = optionExerciseDates;

    // return result

    return arguments_.participationRate * cva;
}

} // namespace data
} // namespace ore
