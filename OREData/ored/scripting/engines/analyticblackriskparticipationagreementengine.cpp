/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/scripting/engines/analyticblackriskparticipationagreementengine.hpp>
#include <ored/scripting/engines/numericlgmriskparticipationagreementengine.hpp>
#include <qle/models/representativeswaption.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>

namespace ore {
namespace data {

AnalyticBlackRiskParticipationAgreementEngine::AnalyticBlackRiskParticipationAgreementEngine(
    const std::string& baseCcy, const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
    const std::map<std::string, Handle<Quote>>& fxSpots, const Handle<DefaultProbabilityTermStructure>& defaultCurve,
    const Handle<Quote>& recoveryRate, const Handle<SwaptionVolatilityStructure>& volatility,
    const QuantLib::ext::shared_ptr<SwapIndex>& swapIndexBase, const bool matchUnderlyingTenor, const Real reversion,
    const bool alwaysRecomputeOptionRepresentation, const Size maxGapDays, const Size maxDiscretisationPoints)
    : RiskParticipationAgreementBaseEngine(baseCcy, discountCurves, fxSpots, defaultCurve, recoveryRate, maxGapDays,
                                           maxDiscretisationPoints),
      volatility_(volatility), swapIndexBase_(swapIndexBase), matchUnderlyingTenor_(matchUnderlyingTenor),
      reversion_(reversion), alwaysRecomputeOptionRepresentation_(alwaysRecomputeOptionRepresentation) {
    registerWith(volatility_);
    registerWith(swapIndexBase_);
}

Real AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv() const {

    QL_REQUIRE(!volatility_.empty(),
               "AnalyticBlackRiskParticipationAgreementEngine::calculate(): empty discount curve");

    // check if we can reuse the swaption representation, otherwise compute it

    if (alwaysRecomputeOptionRepresentation_ || arguments_.optionRepresentationReferenceDate == Date() ||
        referenceDate_ != arguments_.optionRepresentationReferenceDate) {

        results_.optionRepresentationReferenceDate = referenceDate_;
        results_.optionRepresentationPeriods.clear();
        results_.optionRepresentation.clear();

        /* we construct one swaption per floating rate coupon on the midpoint of the accrual period
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

        QL_REQUIRE(
            arguments_.underlying.size() == 2,
            "AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv(): expected two underlying legs, got "
                << arguments_.underlying.size());
        QL_REQUIRE(arguments_.underlyingCcys[0] == arguments_.underlyingCcys[1],
                   "AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv(): expected underlying legs in "
                   "samwe currency, got "
                       << arguments_.underlyingCcys[0] << ", " << arguments_.underlyingCcys[1]);
        QL_REQUIRE(!discountCurves_[arguments_.underlyingCcys[0]].empty(),
                   "AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty discount curve for ccy "
                       << arguments_.underlyingCcys[0]);

        QuantExt::RepresentativeSwaptionMatcher matcher(arguments_.underlying, arguments_.underlyingPayer, swapIndexBase_,
                                              matchUnderlyingTenor_, discountCurves_[arguments_.underlyingCcys[0]],
                                              reversion_);
        for (auto const& e : results_.optionRepresentationPeriods) {
            auto swp = matcher.representativeSwaption(
                std::get<0>(e), QuantExt::RepresentativeSwaptionMatcher::InclusionCriterion::PayDateGtExercise);
            // swp might be null, if there are not underlying flows left, this is handled below
            results_.optionRepresentation.push_back(swp);
        }
    } else {
        results_.optionRepresentationReferenceDate = arguments_.optionRepresentationReferenceDate;
        results_.optionRepresentationPeriods = arguments_.optionRepresentationPeriods;
        results_.optionRepresentation = arguments_.optionRepresentation;
        QL_REQUIRE(
            results_.optionRepresentation.size() == results_.optionRepresentationPeriods.size(),
            "AnalyticBlackRiskParticipationAgreementEngine::calculate(): inconsistent swaption representation periods");
    }

    // attach an engine to the representative swaptions

    QuantLib::ext::shared_ptr<PricingEngine> engine;
    if (volatility_->volatilityType() == ShiftedLognormal) {
        engine = QuantLib::ext::make_shared<BlackSwaptionEngine>(discountCurves_[arguments_.underlyingCcys[0]], volatility_);
    } else {
        engine =
            QuantLib::ext::make_shared<BachelierSwaptionEngine>(discountCurves_[arguments_.underlyingCcys[0]], volatility_);
    }

    for (auto s : results_.optionRepresentation) {
        if (s) {
            if (auto tmp = QuantLib::ext::dynamic_pointer_cast<Swaption>(s)) {
                s->setPricingEngine(engine);
            } else {
                QL_FAIL("AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv(): internal error, could not "
                        "cast representative instrument to Swaption");
            }
        }
    }

    // compute a CVA using the representative swaptions

    QL_REQUIRE(!fxSpots_[arguments_.underlyingCcys[0]].empty(),
               "AnalyticBlackRiskParticipationAgreementEngine::protectionLegNpv(): empty fx spot for ccy pair "
                   << arguments_.underlyingCcys[0] + baseCcy_);

    Real cva = 0.0;
    std::vector<Real> optionPv(results_.optionRepresentationPeriods.size(), 0.0);
    for (Size i = 0; i < results_.optionRepresentationPeriods.size(); ++i) {
        if (auto s = results_.optionRepresentation[i]) {
            Real pd = defaultCurve_->defaultProbability(std::get<1>(results_.optionRepresentationPeriods[i]),
                                                        std::get<2>(results_.optionRepresentationPeriods[i]));
            Real swpNpv = s ? s->NPV() : 0.0; // if s = null, there are no underlying flows left => NPV = 0
            cva += pd * (1.0 - effectiveRecoveryRate_) * swpNpv * fxSpots_[arguments_.underlyingCcys[0]]->value();
            optionPv[i] = swpNpv;
        }
    }

    // detach pricing engine from result swaption representation

    QuantLib::ext::shared_ptr<PricingEngine> emptyEngine;
    for (auto s : results_.optionRepresentation)
        if (s)
            s->setPricingEngine(emptyEngine);

    // set additional results

    std::vector<Date> optionExerciseDates;
    for (auto const& e : results_.optionRepresentationPeriods) {
        optionExerciseDates.push_back(std::get<0>(e));
    }

    results_.additionalResults["OptionNpvs"] = optionPv;
    results_.additionalResults["FXSpot"] = fxSpots_[arguments_.underlyingCcys[0]]->value();
    results_.additionalResults["OptionExerciseDates"] = optionExerciseDates;

    // return result

    return arguments_.participationRate * cva;
}

} // namespace data
} // namespace ore
