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

#include <ored/scripting/engines/numericlgmriskparticipationagreementengine_tlock.hpp>

#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

NumericLgmRiskParticipationAgreementEngineTLock::NumericLgmRiskParticipationAgreementEngineTLock(
    const std::string& baseCcy, const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
    const std::map<std::string, Handle<Quote>>& fxSpots, const QuantLib::ext::shared_ptr<QuantExt::LinearGaussMarkovModel>& model,
    const Real sy, const Size ny, const Real sx, const Size nx, const Handle<YieldTermStructure>& treasuryCurve,
    const Handle<DefaultProbabilityTermStructure>& defaultCurve, const Handle<Quote>& recoveryRate,
    const Size timeStepsPerYear)
    : LgmConvolutionSolver2(model, sy, ny, sx, nx), baseCcy_(baseCcy), discountCurves_(discountCurves),
      fxSpots_(fxSpots), treasuryCurve_(treasuryCurve), defaultCurve_(defaultCurve), recoveryRate_(recoveryRate),
      timeStepsPerYear_(timeStepsPerYear) {
    registerWith(QuantExt::LgmConvolutionSolver2::model());
    for (auto const& d : discountCurves_)
        registerWith(d.second);
    for (auto const& s : fxSpots_)
        registerWith(s.second);
    fxSpots_[baseCcy] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    registerWith(treasuryCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
}

void NumericLgmRiskParticipationAgreementEngineTLock::calculate() const {

    QL_REQUIRE(!discountCurves_[baseCcy_].empty(),
               "RiskParticipationAgreementEngineTLock::calculate(): empty discount curve for base ccy " << baseCcy_);
    QL_REQUIRE(!defaultCurve_.empty(), "RiskParticipationAgreementEngineTLock::calculate(): empty default curve");
    QL_REQUIRE(arguments_.fixedRecoveryRate != Null<Real>() || !recoveryRate_.empty(),
               "RiskParticipationAgreementEngineTLock::calculate(): empty recovery and trade does not specify "
               "fixed recovery");

    // asof date for valuation

    referenceDate_ = discountCurves_[baseCcy_]->referenceDate();

    // effective recovery rate to use

    Real effectiveRecoveryRate =
        arguments_.fixedRecoveryRate == Null<Real>() ? recoveryRate_->value() : arguments_.fixedRecoveryRate;

    // Compute the fee leg NPV

    Real fee = 0.0;
    Size idx = 0;
    for (auto const& l : arguments_.protectionFee) {
        for (auto const& c : l) {
            if (c->date() <= referenceDate_)
                continue;
            QL_REQUIRE(!discountCurves_[arguments_.protectionFeeCcys[idx]].empty(),
                       "RiskParticipationAgreementEngineTLock::calculate(): empty discount curve for ccy "
                           << arguments_.protectionFeeCcys[idx]);
            QL_REQUIRE(!fxSpots_[arguments_.protectionFeeCcys[idx]].empty(),
                       "RiskParticipationAgreementEngineTLock::calculate(): empty fx spot for ccy pair "
                           << arguments_.protectionFeeCcys[idx] + baseCcy_);
            // the fee is only paid if the reference entity is still alive at the payment date
            fee += c->amount() * discountCurves_[arguments_.protectionFeeCcys[idx]]->discount(c->date()) *
                   fxSpots_[arguments_.protectionFeeCcys[idx]]->value() * defaultCurve_->survivalProbability(c->date());
            // accrual settlement using the mid of the coupon periods
            auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c);
            if (cpn && arguments_.settlesAccrual) {
                Date start = std::max(cpn->accrualStartDate(), referenceDate_);
                Date end = cpn->accrualEndDate();
                if (start < end) {
                    Date mid = start + (end - start) / 2;
                    fee += cpn->accruedAmount(mid) * discountCurves_[arguments_.protectionFeeCcys[idx]]->discount(mid) *
                           fxSpots_[arguments_.protectionFeeCcys[idx]]->value() *
                           defaultCurve_->defaultProbability(start, end);
                }
            }
        }
        ++idx;
    }

    // We have a zero protection npv, if the protection end is <= ref date. We also set the protection value to zero,
    // if the termination date < ref date. This is not entirely correct in case the payment date is still in the future
    // and there is a positive payoff, but we can not compute the bond yield on past dates.

    Real protection = 0.0;
    if (arguments_.protectionEnd > referenceDate_ && arguments_.terminationDate >= referenceDate_) {

        if (arguments_.terminationDate == referenceDate_) {

            // Handle the case termination date = ref date

            Real optionPv = std::max(0.0, computePayoff().at(0));
            Date riskHorizon = std::min(arguments_.protectionEnd, arguments_.paymentDate);
            Real t = discountCurves_[baseCcy_]->timeFromReference(riskHorizon);
            if (riskHorizon > referenceDate_) {
                protection = optionPv * defaultCurve_->defaultProbability(0.0, t) * (1.0 - effectiveRecoveryRate) *
                             discountCurves_[baseCcy_]->discount(t / 2.0);
            }

        } else {

            // Case termination date > ref date: We build a grid on which we compute the positive part of the NPV of the
            // underlying TLock. The last grid point is the termination date

            Real t_term = discountCurves_[baseCcy_]->timeFromReference(arguments_.terminationDate);

            Size effTimeSteps =
                std::max<Size>(1, std::lround(static_cast<Real>(std::max<Size>(timeStepsPerYear_, 1)) * t_term + 0.5));

            TimeGrid grid(t_term, effTimeSteps);

            QuantExt::RandomVariable underlyingPv(gridSize(), 0.0);
            std::vector<Real> optionPv(grid.size(), 0.0);

            // roll back the payoff and positive pv of the TLock from the termination date, and calculate the option pvs

            underlyingPv = computePayoff();
            optionPv.back() = expectation(max(underlyingPv, QuantExt::RandomVariable(gridSize(), 0.0))).at(0);

            for (Size i = grid.size() - 1; i > 0; --i) {
                Real t_from = grid[i];
                Real t_to = grid[i - 1];

                underlyingPv = rollback(underlyingPv, t_from, t_to);
                optionPv[i - 1] = expectation(max(underlyingPv, QuantExt::RandomVariable(gridSize(), 0.0))).at(0);
            }

            // compute the protection npv

            for (Size i = 0; i < grid.size(); ++i) {
                Real t0 = 0.0;
                if (i > 0) {
                    t0 = 0.5 * (grid[i - 1] + grid[i]);
                }
                Real t1 = discountCurves_[baseCcy_]->timeFromReference(arguments_.paymentDate);
                if (i < grid.size() - 1) {
                    t1 = 0.5 * (grid[i] + grid[i + 1]);
                }
                t1 = std::min(t1, discountCurves_[baseCcy_]->timeFromReference(arguments_.protectionEnd));
                if (t1 > t0 && !QuantLib::close_enough(t0, t1)) {
                    protection +=
                        optionPv[i] * defaultCurve_->defaultProbability(t0, t1) * (1.0 - effectiveRecoveryRate);
                }
            }
        }
    }

    // multiply the protection npv by the participation rate

    protection *= arguments_.participationRate;

    // compute the total NPV, we buy the protection if we pay the fee

    results_.value = (arguments_.protectionFeePayer ? 1.0 : -1.0) * (protection - fee);
}

QuantExt::RandomVariable NumericLgmRiskParticipationAgreementEngineTLock::computePayoff() const {

    Date settlement = arguments_.bond->settlementDate(arguments_.terminationDate);
    Real multiplier = (arguments_.payer ? -1.0 : 1.0) * arguments_.bondNotional * arguments_.bond->notional(settlement);

    if (arguments_.terminationDate == referenceDate_) {
        Real price = BondFunctions::cleanPrice(*arguments_.bond, **treasuryCurve_, settlement);
        Real yield =
            BondFunctions::yield(*arguments_.bond, price, arguments_.dayCounter, Compounded, Semiannual, settlement);
        Real dv01 = price / 100.0 *
                    BondFunctions::duration(*arguments_.bond, yield, arguments_.dayCounter, Compounded, Semiannual,
                                            Duration::Modified, settlement);
        return QuantExt::RandomVariable(gridSize(), multiplier * (arguments_.referenceRate - yield) * dv01 *
                                              discountCurves_[baseCcy_]->discount(arguments_.paymentDate));
    }

    QuantExt::RandomVariable result(gridSize(), 0.0);

    LgmImpliedYtsFwdFwdCorrected modelCurve(model(), treasuryCurve_, treasuryCurve_->dayCounter(), false, true);
    modelCurve.move(arguments_.terminationDate, 0.0);

    Real t = discountCurves_[baseCcy_]->timeFromReference(arguments_.terminationDate);
    Real t_pay = discountCurves_[baseCcy_]->timeFromReference(arguments_.paymentDate);
    auto states = stateGrid(t);

    for (Size i = 0; i < states.size(); ++i) {

        modelCurve.state(states[i]);

        // use hardcoded conventions for Treasury bonds
        Real price = BondFunctions::cleanPrice(*arguments_.bond, modelCurve, settlement);
        Real yield =
            BondFunctions::yield(*arguments_.bond, price, arguments_.dayCounter, Compounded, Semiannual, settlement);
        Real dv01 = price / 100.0 *
                    BondFunctions::duration(*arguments_.bond, yield, arguments_.dayCounter, Compounded, Semiannual,
                                            Duration::Modified, settlement);

        result.set(i, multiplier * (arguments_.referenceRate - yield) * dv01 *
                          model()->discountBond(t, t_pay, states[i], discountCurves_[baseCcy_]) /
                          model()->numeraire(t, states[i], discountCurves_[baseCcy_]));
    }

    return result;
}

} // namespace data
} // namespace ore
