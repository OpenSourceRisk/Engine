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

#include <qle/pricingengines/indexcdsoptionbaseengine.hpp>

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/utilities/time.hpp>

#include <numeric>

using namespace QuantLib;
using std::string;
using std::vector;

namespace QuantExt {

IndexCdsOptionBaseEngine::IndexCdsOptionBaseEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                                     Real recovery, const Handle<YieldTermStructure>& discount,
                                                     const Handle<QuantExt::CreditVolCurve>& volatility)
    : probabilities_({probability}), recoveries_({recovery}), discount_(discount), volatility_(volatility),
      indexRecovery_(recovery) {
    registerWithMarket();
}

IndexCdsOptionBaseEngine::IndexCdsOptionBaseEngine(
    const vector<Handle<DefaultProbabilityTermStructure>>& probabilities, const vector<Real>& recoveries,
    const Handle<YieldTermStructure>& discount, const Handle<QuantExt::CreditVolCurve>& volatility, Real indexRecovery)
    : probabilities_(probabilities), recoveries_(recoveries), discount_(discount), volatility_(volatility),
      indexRecovery_(indexRecovery) {

    QL_REQUIRE(!probabilities_.empty(), "IndexCdsOptionBaseEngine: need at least one probability curve.");
    QL_REQUIRE(probabilities_.size() == recoveries_.size(), "IndexCdsOptionBaseEngine: mismatch between size"
                                                                << " of probabilities (" << probabilities_.size()
                                                                << ") and recoveries (" << recoveries_.size() << ").");

    registerWithMarket();

    // If the index recovery is not populated, use the average recovery.
    if (indexRecovery_ == Null<Real>())
        indexRecovery_ = accumulate(recoveries_.begin(), recoveries_.end(), 0.0) / recoveries_.size();
}

const vector<Handle<DefaultProbabilityTermStructure>>& IndexCdsOptionBaseEngine::probabilities() const {
    return probabilities_;
}

const vector<Real>& IndexCdsOptionBaseEngine::recoveries() const { return recoveries_; }

const Handle<YieldTermStructure> IndexCdsOptionBaseEngine::discount() const { return discount_; }

const Handle<QuantExt::CreditVolCurve> IndexCdsOptionBaseEngine::volatility() const { return volatility_; }

void IndexCdsOptionBaseEngine::calculate() const {

    // Underlying index CDS
    const auto& cds = *arguments_.swap;

    // If given constituent curves, store constituent notionals. Otherwise, store top level notional.
    if (probabilities_.size() > 1) {
        notionals_ = cds.underlyingNotionals();
        QL_REQUIRE(probabilities_.size() == notionals_.size(), "IndexCdsOptionBaseEngine: mismatch between size"
                                                                   << " of probabilities (" << probabilities_.size()
                                                                   << ") and notionals (" << notionals_.size() << ").");
    } else {
        notionals_ = {cds.notional()};
    }

    // Get additional results of underlying index CDS.
    cds.NPV();
    results_.additionalResults = cds.additionalResults();

    // call engine-specific calculation
    doCalc();
}

Real IndexCdsOptionBaseEngine::fep() const {

    // Exercise date
    const Date& exerciseDate = arguments_.exercise->dates().front();

    // Realised FEP
    results_.additionalResults["realisedFEP"] = arguments_.realisedFep;

    // Unrealised FEP
    Real fep = 0.0;
    for (Size i = 0; i < probabilities_.size(); ++i) {
        fep += (1 - recoveries_[i]) * probabilities_[i]->defaultProbability(exerciseDate) * notionals_[i];
    }
    results_.additionalResults["UnrealisedFEP"] = fep;

    // Total FEP
    fep += arguments_.realisedFep;
    results_.additionalResults["FEP"] = fep;

    // Discounted FEP
    Real discount = discount_->discount(exerciseDate);
    fep *= discount;
    results_.additionalResults["discountedFEP"] = fep;

    return fep;
}

void IndexCdsOptionBaseEngine::registerWithMarket() {
    for (const auto& p : probabilities_)
        registerWith(p);
    registerWith(discount_);
    registerWith(volatility_);
}

Real IndexCdsOptionBaseEngine::forwardRiskyAnnuityStrike() const {

    // Underlying index CDS.
    const auto& cds = *arguments_.swap;

    // This method returns RPV01(0; t_e, T, K) / SP(t_e; K). This is the quantity in formula 11.9 of O'Kane 2008.
    // There is a slight modification in that we divide by the survival probability to t_E using the flat curve at
    // the strike spread that we create here.

    // Standard index CDS schedule.
    Schedule schedule = MakeSchedule()
                            .from(cds.protectionStartDate())
                            .to(cds.maturity())
                            .withCalendar(WeekendsOnly())
                            .withFrequency(Quarterly)
                            .withConvention(Following)
                            .withTerminationDateConvention(Unadjusted)
                            .withRule(DateGeneration::CDS2015);

    // Derive hazard rate curve from a single forward starting CDS matching the characteristics of underlying index
    // CDS with a running spread equal to the strike.
    const Real& strike = arguments_.strike;
    Real accuracy = 1e-8;

    auto strikeCds = boost::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 1 / accuracy, strike, schedule, Following, Actual360(), cds.settlesAccrual(),
        cds.protectionPaymentTime(), cds.protectionStartDate(), boost::shared_ptr<Claim>(), Actual360(true), true,
        cds.tradeDate(), cds.cashSettlementDays());
    // dummy engine
    strikeCds->setPricingEngine(boost::make_shared<MidPointCdsEngine>(
        Handle<DefaultProbabilityTermStructure>(
            boost::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0, Actual365Fixed())),
        0.0, Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed()))));

    Real hazardRate;
    try {
        hazardRate = strikeCds->impliedHazardRate(0.0, discount_, Actual365Fixed(), indexRecovery_, accuracy);
    } catch (const std::exception& e) {
        QL_FAIL("can not imply fair hazard rate for CDS at option strike "
                << strike << ". Is the strike correct? Exception: " << e.what());
    }

    Handle<DefaultProbabilityTermStructure> dph(
        boost::make_shared<FlatHazardRate>(discount_->referenceDate(), hazardRate, Actual365Fixed()));

    // Calculate the forward risky strike annuity.
    strikeCds->setPricingEngine(boost::make_shared<QuantExt::MidPointCdsEngine>(dph, indexRecovery_, discount_));
    Real rpv01_K = std::abs(strikeCds->couponLegNPV() + strikeCds->accrualRebateNPV()) /
                   (strikeCds->notional() * strikeCds->runningSpread());
    results_.additionalResults["riskyAnnuityStrike"] = rpv01_K;
    QL_REQUIRE(rpv01_K > 0.0, "BlackIndexCdsOptionEngine: strike based risky annuity must be positive.");

    // Survival to exercise
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Probability spToExercise = dph->survivalProbability(exerciseDate);
    results_.additionalResults["strikeBasedSurvivalToExercise"] = spToExercise;

    // Forward risky annuity strike (still has discount but divides out the survival probability)
    Real rpv01_K_fwd = rpv01_K / spToExercise;
    results_.additionalResults["forwardRiskyAnnuityStrike"] = rpv01_K_fwd;

    return rpv01_K_fwd;
}

} // namespace QuantExt
