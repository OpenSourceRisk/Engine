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

#include <numeric>
#include <qle/pricingengines/blackindexcdsoptionengine.hpp>
#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <qle/utilities/time.hpp>

using namespace QuantLib;
using std::string;
using std::vector;

namespace QuantExt {

BlackIndexCdsOptionEngine::BlackIndexCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                                     Real recovery, const Handle<YieldTermStructure>& discount,
                                                     const Handle<QuantExt::CreditVolCurve>& volatility)
    : probabilities_({probability}), recoveries_({recovery}), discount_(discount), volatility_(volatility),
      indexRecovery_(recovery) {
    registerWithMarket();
}

BlackIndexCdsOptionEngine::BlackIndexCdsOptionEngine(
    const vector<Handle<DefaultProbabilityTermStructure>>& probabilities, const vector<Real>& recoveries,
    const Handle<YieldTermStructure>& discount, const Handle<QuantExt::CreditVolCurve>& volatility, Real indexRecovery)
    : probabilities_(probabilities), recoveries_(recoveries), discount_(discount), volatility_(volatility),
      indexRecovery_(indexRecovery) {

    QL_REQUIRE(!probabilities_.empty(), "BlackIndexCdsOptionEngine: need at least one probability curve.");
    QL_REQUIRE(probabilities_.size() == recoveries_.size(), "BlackIndexCdsOptionEngine: mismatch between size"
                                                                << " of probabilities (" << probabilities_.size()
                                                                << ") and recoveries (" << recoveries_.size() << ").");

    registerWithMarket();

    // If the index recovery is not populated, use the average recovery.
    if (indexRecovery_ == Null<Real>())
        indexRecovery_ = accumulate(recoveries_.begin(), recoveries_.end(), 0.0) / recoveries_.size();
}

const vector<Handle<DefaultProbabilityTermStructure>>& BlackIndexCdsOptionEngine::probabilities() const {
    return probabilities_;
}

const vector<Real>& BlackIndexCdsOptionEngine::recoveries() const { return recoveries_; }

const Handle<YieldTermStructure> BlackIndexCdsOptionEngine::discount() const { return discount_; }

const Handle<QuantExt::CreditVolCurve> BlackIndexCdsOptionEngine::volatility() const { return volatility_; }

void BlackIndexCdsOptionEngine::calculate() const {

    // Underlying index CDS
    const auto& cds = *arguments_.swap;

    // If given constituent curves, store constituent notionals. Otherwise, store top level notional.
    if (probabilities_.size() > 1) {
        notionals_ = cds.underlyingNotionals();
        QL_REQUIRE(probabilities_.size() == notionals_.size(), "BlackIndexCdsOptionEngine: mismatch between size"
                                                                   << " of probabilities (" << probabilities_.size()
                                                                   << ") and notionals (" << notionals_.size() << ").");
    } else {
        notionals_ = {cds.notional()};
    }

    // Get additional results of underlying index CDS.
    cds.NPV();
    results_.additionalResults = cds.additionalResults();

    // Calculate option value depending on strike type.
    if (arguments_.strikeType == CdsOption::Spread)
        spreadStrikeCalculate(fep());
    else
        priceStrikeCalculate(fep());
}

Real BlackIndexCdsOptionEngine::fep() const {

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

void BlackIndexCdsOptionEngine::spreadStrikeCalculate(Real fep) const {

    // Underlying index CDS.
    const auto& cds = *arguments_.swap;

    // Add some additional entries to additional results.
    const Real& strike = arguments_.strike;
    results_.additionalResults["strikeSpread"] = strike;
    Real runningSpread = cds.runningSpread();
    results_.additionalResults["runningSpread"] = runningSpread;

    // Calculate risky PV01, as of the valuation date i.e. time 0, for the period from $t_E$ to underlying index
    // CDS maturity $T$. This risky PV01 does not include the non-risky accrual from the CDS premium leg coupon date
    // immediately preceding the expiry date up to the expiry date.
    Real rpv01 = std::abs(cds.couponLegNPV() + cds.accrualRebateNPV()) / (cds.notional() * cds.runningSpread());
    results_.additionalResults["riskyAnnuity"] = rpv01;
    QL_REQUIRE(cds.notional() > 0.0 || close_enough(cds.notional(), 0.0),
               "BlackIndexCdsOptionEngine: notional must not be negative (" << cds.notional() << ")");
    QL_REQUIRE(rpv01 > 0.0, "BlackIndexCdsOptionEngine: risky annuity must be positive (couponLegNPV="
                                << cds.couponLegNPV() << ", accrualRebateNPV=" << cds.accrualRebateNPV()
                                << ", notional=" << cds.notional() << ", runningSpread=" << cds.runningSpread() << ")");

    Real fairSpread = cds.fairSpreadClean();
    results_.additionalResults["forwardSpread"] = fairSpread;

    // FEP adjusted forward spread. F^{Adjusted} in O'Kane 2008, Section 11.7. F' in ICE paper (notation is poor).
    Real Fp = fairSpread + fep / rpv01 / cds.notional();
    results_.additionalResults["fepAdjustedForwardSpread"] = Fp;

    // Adjusted strike spread. K' in O'Kane 2008, Section 11.7. K' in ICE paper (notation is poor).
    Real Kp = close_enough(strike, 0.0)
                  ? 0.0
                  : runningSpread + forwardRiskyAnnuityStrike() * (strike - runningSpread) / rpv01;
    results_.additionalResults["adjustedStrikeSpread"] = Kp;

    // Read the volatility from the volatility surface
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm), strike,
                                              CreditVolCurve::Type::Spread);
    Real stdDev = volatility * std::sqrt(exerciseTime);
    results_.additionalResults["volatility"] = volatility;
    results_.additionalResults["standardDeviation"] = stdDev;

    // Option type
    Option::Type callPut = cds.side() == Protection::Buyer ? Option::Call : Option::Put;
    results_.additionalResults["callPut"] = callPut == Option::Call ? string("Call") : string("Put");

    // NPV. Add the relevant notionals to the additional results also.
    results_.additionalResults["valuationDateNotional"] = cds.notional();
    results_.additionalResults["tradeDateNotional"] = arguments_.tradeDateNtl;
    Real factor = arguments_.tradeDateNtl / cds.notional();

    // Check the forward before plugging it into the black formula
    QL_REQUIRE(Fp > 0.0 || close_enough(stdDev, 0.0),
               "BlackIndexCdsOptionEngine: FEP adjusted forward spread ("
                   << Fp << ") is not positive, can not calculate a reasonable option price");
    // The strike spread might get negative through the adjustment above, but economically the strike is
    // floored at 0.0, so we ensure this here. This lets us compute the black formula as well in all cases.
    Kp = std::max(Kp, 0.0);

    results_.value = rpv01 * cds.notional() * blackFormula(callPut, factor * Kp, Fp, stdDev, 1.0);
}

void BlackIndexCdsOptionEngine::priceStrikeCalculate(Real fep) const {

    // Underlying index CDS.
    const auto& cds = *arguments_.swap;

    // Add some additional entries to additional results.
    const Real& strike = arguments_.strike;
    results_.additionalResults["strikePrice"] = strike;

    // Discount factor to exercise
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    DiscountFactor discToExercise = discount_->discount(exerciseDate);
    results_.additionalResults["discountToExercise"] = discToExercise;

    // NPV from buyer's perspective gives upfront, as of valuation date, with correct sign.
    Real npv = cds.side() == Protection::Buyer ? cds.NPV() : -cds.NPV();
    results_.additionalResults["upfront"] = npv;

    // Add the relevant notionals to the additional results. Adjust the forward price also so that it relates to the
    // notional outstanding as of the trade date of the option. Generally, this is the notional of the version that
    // was on the run on that date. Note that the strike price is in terms of this notional.
    results_.additionalResults["valuationDateNotional"] = cds.notional();
    const Real& tradeDateNtl = arguments_.tradeDateNtl;
    results_.additionalResults["tradeDateNotional"] = tradeDateNtl;

    // Forward price. Note that the forward price relates to the notional outstanding as of the trade date of the
    // option. Generally, this is the notional of the version that was on the run on that date. Note that the strike
    // price is in terms of this notional also. Markit quotes different reference prices, forward prices and price
    // volatilities for different versions (option trade date determines version traded) of the same index series.
    Real forwardPrice = (1 - npv / tradeDateNtl) / discToExercise;
    results_.additionalResults["forwardPrice"] = forwardPrice;

    // Front end protection adjusted forward price.
    Real Fp = forwardPrice - fep / tradeDateNtl / discToExercise;
    results_.additionalResults["fepAdjustedForwardPrice"] = Fp;

    // Read the volatility from the volatility surface
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm), strike,
                                              CreditVolCurve::Type::Price);
    Real stdDev = volatility * std::sqrt(exerciseTime);
    results_.additionalResults["volatility"] = volatility;
    results_.additionalResults["standardDeviation"] = stdDev;

    // If protection buyer, put on price.
    Option::Type cp = cds.side() == Protection::Buyer ? Option::Put : Option::Call;
    results_.additionalResults["callPut"] = cp == Option::Put ? string("Put") : string("Call");

    // Check / adjust the inputs to the black formula before applying it
    QL_REQUIRE(Fp > 0.0 || close_enough(stdDev, 0.0),
               "BlackIndexCdsOptionEngine: FEP adjusted forward price ("
                   << Fp << ") is not positive, can not calculate a reasonable option price");
    QL_REQUIRE(strike > 0 || close_enough(strike, 0.0),
               "BlackIndexCdsOptionEngine: Strike price ("
                   << strike << ") is not positive, can not calculate a reasonable option price");

    // NPV.
    results_.value = tradeDateNtl * blackFormula(cp, strike, Fp, stdDev, discToExercise);
}

Real BlackIndexCdsOptionEngine::forwardRiskyAnnuityStrike() const {

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
        cds.protectionPaymentTime(), cds.protectionStartDate(), boost::shared_ptr<Claim>(), Actual360(true),
        true, cds.tradeDate(), cds.cashSettlementDays());
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

void BlackIndexCdsOptionEngine::registerWithMarket() {
    for (const auto& p : probabilities_)
        registerWith(p);
    registerWith(discount_);
    registerWith(volatility_);
}

} // namespace QuantExt
