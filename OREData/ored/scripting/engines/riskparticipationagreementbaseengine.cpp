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

#include <ored/scripting/engines/riskparticipationagreementbaseengine.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/quotes/simplequote.hpp>

// #include <iostream> // just for debugging

namespace ore {
namespace data {

RiskParticipationAgreementBaseEngine::RiskParticipationAgreementBaseEngine(
    const std::string& baseCcy, const std::map<std::string, Handle<YieldTermStructure>>& discountCurves,
    const std::map<std::string, Handle<Quote>>& fxSpots, const Handle<DefaultProbabilityTermStructure>& defaultCurve,
    const Handle<Quote>& recoveryRate, const Size maxGapDays, const Size maxDiscretisationPoints)
    : baseCcy_(baseCcy), discountCurves_(discountCurves), fxSpots_(fxSpots), defaultCurve_(defaultCurve),
      recoveryRate_(recoveryRate), maxGapDays_(maxGapDays), maxDiscretisationPoints_(maxDiscretisationPoints) {
    QL_REQUIRE(maxGapDays == Null<Size>() || maxGapDays >= 1,
               "invalid maxGapDays (" << maxGapDays << "), must be >= 1");
    QL_REQUIRE(maxDiscretisationPoints_ == Null<Size>() || maxDiscretisationPoints_ >= 1,
               "invalid maxDiscretisationPoints (" << maxDiscretisationPoints << "), must be >= 3");
    for (auto const& d : discountCurves_)
        registerWith(d.second);
    for (auto const& s : fxSpots_)
        registerWith(s.second);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    fxSpots_[baseCcy_] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
}

// just for debugging
// namespace {
// void logGrid(const std::vector<Date>& original, const std::vector<Date>& now) {
//     for (Size i = 0; i < original.size(); ++i) {
//         if (std::find(now.begin(), now.end(), original[i]) != now.end())
//             std::clog << "| ";
//         else
//             std::clog << "  ";
//     }
//     std::clog << " (" << (now.size() - 1) << ")"
//               << "\n";
// }
// } // namespace

std::vector<Date> RiskParticipationAgreementBaseEngine::buildDiscretisationGrid(
    const Date& referenceDate, const Date& protectionStart, const Date& protectionEnd,
    const std::vector<Leg>& underlying, const Size maxGapDays, const Size maxDiscretisationPoints) {

    QL_REQUIRE(protectionEnd > referenceDate,
               "protection end (" << protectionEnd << ") must be > reference date (" << referenceDate << ")");

    // collect the accrual end dates of the float coupons

    std::vector<Date> accrualDates;
    for (auto const& l : underlying) {
        for (auto const& c : l) {
            if (auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                accrualDates.push_back(f->accrualEndDate());
            }
        }
    }

    // build the discretisation grid

    accrualDates.push_back(std::max(protectionStart, referenceDate));
    accrualDates.push_back(protectionEnd);

    std::sort(accrualDates.begin(), accrualDates.end());
    auto it = std::unique(accrualDates.begin(), accrualDates.end());
    accrualDates.resize(it - accrualDates.begin());

    auto itStart = std::lower_bound(accrualDates.begin(), accrualDates.end(), referenceDate);
    auto itEnd = std::upper_bound(accrualDates.begin(), accrualDates.end(), protectionEnd);

    QL_REQUIRE(std::distance(itStart, itEnd) >= 2, "got invalid discretisationGrid for RPA, this is unexpected");

    std::vector<Date> gridDates(itStart, itEnd);

    // add additional dates on mid points of intervals that are exceeding the max gap given

    if (maxGapDays != Null<Size>()) {
        bool refining;
        // just to really make sure we exit the loop below!
        Size refiningIterations = 0;
        do {
            refining = false;
            for (auto it = gridDates.begin(); it < gridDates.end() - 1; ++it) {
                if (*(it + 1) - *it > std::max<int>(1, maxGapDays)) {
                    it = gridDates.insert(it + 1, *it + (*(it + 1) - *it) / 2);
                    refining = true;
                }
            }
        } while (refining && ++refiningIterations < 100);
        QL_REQUIRE(refiningIterations < 100, "discretisationGrid refinement failed, this is unexpected");
    }

    // if the number of intervals exceeds the max allowed number, remove points at the beginning and end like this
    //
    // (1) |  x  |  x  |  x  |  x  |  x  |  x  | x  |  x  |  x  |   =>
    // (2) |        x        |  x  |  x  |  x  | x  |  x  |  x  |   =>
    // (3) |        x        |  x  |  x  |  x  |       x        |   etc.
    //
    // where each '|' marks an entry in gridDates and 'x' marks the midpoint of the intervals,
    // until we reach the max allowed number of discretisation points.

    // just for debugging
    // std::vector<Date> originalGridDates(gridDates);
    // logGrid(originalGridDates, gridDates);

    Size currentNumberOfDiscretisationPoints = gridDates.size() - 1,
         previousNumberOfDiscretisationPoints = QL_MAX_INTEGER;

    while (maxDiscretisationPoints != Null<Size>() && currentNumberOfDiscretisationPoints > maxDiscretisationPoints &&
           currentNumberOfDiscretisationPoints < previousNumberOfDiscretisationPoints) {

        previousNumberOfDiscretisationPoints = currentNumberOfDiscretisationPoints;
        Size currentLeftIndex = 0, currentRightIndex = gridDates.size() - 1;
        bool nextErasureOnLeft = true, canRemoveFurtherPointsInPass = true;

        while (currentNumberOfDiscretisationPoints > maxDiscretisationPoints && canRemoveFurtherPointsInPass) {
            if (nextErasureOnLeft) {
                if (gridDates.size() >= currentLeftIndex + 4 && currentLeftIndex + 3 <= currentRightIndex) {
                    gridDates.erase(std::next(gridDates.begin(), currentLeftIndex + 1),
                                    std::next(gridDates.begin(), currentLeftIndex + 3));
                    currentNumberOfDiscretisationPoints -= 2;
                    currentRightIndex -= 2;
                    currentLeftIndex++;
                    nextErasureOnLeft = false;
                } else {
                    canRemoveFurtherPointsInPass = false;
                }
            } else {
                if (currentRightIndex >= 3 && currentLeftIndex + 3 <= currentRightIndex) {
                    gridDates.erase(std::next(gridDates.begin(), currentRightIndex - 2),
                                    std::next(gridDates.begin(), currentRightIndex));
                    currentNumberOfDiscretisationPoints -= 2;
                    currentRightIndex -= 3;
                    nextErasureOnLeft = true;
                } else {
                    canRemoveFurtherPointsInPass = false;
                }
            }
            // just for debugging
            // logGrid(originalGridDates, gridDates);
        }
    }

    return gridDates;
}

void RiskParticipationAgreementBaseEngine::calculate() const {

    QL_REQUIRE(!discountCurves_[baseCcy_].empty(),
               "RiskParticipationAgreementBaseEngine::calculate(): empty discount curve for ccy" << baseCcy_);
    QL_REQUIRE(!defaultCurve_.empty(), "RiskParticipationAgreementBaseEngine::calculate(): empty default curve");
    QL_REQUIRE(arguments_.fixedRecoveryRate != Null<Real>() || !recoveryRate_.empty(),
               "RiskParticipationAgreementBaseEngine::calculate(): empty recovery and trade does not specify "
               "fixed recovery");

    // asof date for valuation

    referenceDate_ = discountCurves_[baseCcy_]->referenceDate();

    // effective recovery rate to use

    effectiveRecoveryRate_ =
        arguments_.fixedRecoveryRate == Null<Real>() ? recoveryRate_->value() : arguments_.fixedRecoveryRate;

    // compute the fee leg NPV

    Real fee = 0.0;
    Size idx = 0;
    std::vector<std::vector<Date>> feeStartDates;
    std::vector<std::vector<Date>> feeEndDates;
    std::vector<std::vector<Date>> feePayDates;
    std::vector<std::vector<Date>> feeMidDates;
    std::vector<std::vector<double>> feeAmounts;
    std::vector<std::vector<double>> feeMidAccrueds;
    std::vector<std::vector<double>> feeMidDiscounts;
    std::vector<std::vector<double>> feeDiscounts;
    std::vector<std::vector<double>> feeSurvivalProbs;
    std::vector<std::vector<double>> feePeriodPDs;
    std::vector<double> feeFXSpot;
    for (auto const& l : arguments_.protectionFee) {
        QL_REQUIRE(!discountCurves_[arguments_.protectionFeeCcys[idx]].empty(),
                   "RiskParticipationAgreementBaseEngine::calculate(): empty discount curve for ccy "
                       << arguments_.protectionFeeCcys[idx]);
        QL_REQUIRE(!fxSpots_[arguments_.protectionFeeCcys[idx]].empty(),
                   "RiskParticipationAgreementBaseEngine::calculate(): empty fx spot for ccy "
                       << arguments_.protectionFeeCcys[idx] + baseCcy_);
        feeFXSpot.push_back(fxSpots_[arguments_.protectionFeeCcys[idx]]->value());
        std::vector<Date> thisFeeStartDates;
        std::vector<Date> thisFeeEndDates;
        std::vector<Date> thisFeePayDates;
        std::vector<Date> thisFeeMidDates;
        std::vector<double> thisFeeAmounts;
        std::vector<double> thisFeeMidAccrueds;
        std::vector<double> thisFeeMidDiscounts;
        std::vector<double> thisFeeDiscounts;
        std::vector<double> thisFeeSurvivalProbs;
        std::vector<double> thisFeePeriodPDs;
        for (auto const& c : l) {
            if (c->date() <= referenceDate_)
                continue;
            thisFeePayDates.push_back(c->date());
            thisFeeAmounts.push_back(c->amount());
            thisFeeDiscounts.push_back(discountCurves_[arguments_.protectionFeeCcys[idx]]->discount(c->date()));
            thisFeeSurvivalProbs.push_back(defaultCurve_->survivalProbability(c->date()));
            // the fee is only paid if the reference entity is still alive at the payment date
            fee += thisFeeAmounts.back() * thisFeeDiscounts.back() * feeFXSpot.back() * thisFeeSurvivalProbs.back();
            // accrual settlement using the mid of the coupon periods
            auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c);
            if (cpn && arguments_.settlesAccrual) {
                Date start = std::max(cpn->accrualStartDate(), referenceDate_);
                Date end = cpn->accrualEndDate();
                thisFeeStartDates.push_back(start);
                thisFeeEndDates.push_back(end);
                if (start < end) {
                    Date mid = start + (end - start) / 2;
                    thisFeeMidDates.push_back(mid);
                    thisFeeMidAccrueds.push_back(cpn->accruedAmount(mid));
                    thisFeeMidDiscounts.push_back(discountCurves_[arguments_.protectionFeeCcys[idx]]->discount(mid));
                    thisFeePeriodPDs.push_back(defaultCurve_->defaultProbability(start, end));
                    fee += thisFeeMidAccrueds.back() * thisFeeMidDiscounts.back() * feeFXSpot.back() *
                           thisFeePeriodPDs.back();
                }
            }
        }
        feeStartDates.push_back(thisFeeStartDates);
        feeEndDates.push_back(thisFeeEndDates);
        feePayDates.push_back(thisFeePayDates);
        feeMidDates.push_back(thisFeeMidDates);
        feeAmounts.push_back(thisFeeAmounts);
        feeMidAccrueds.push_back(thisFeeMidAccrueds);
        feeMidDiscounts.push_back(thisFeeMidDiscounts);
        feeDiscounts.push_back(thisFeeDiscounts);
        feeSurvivalProbs.push_back(thisFeeSurvivalProbs);
        feePeriodPDs.push_back(thisFeePeriodPDs);
        ++idx;
    }

    // if we are past the protection end date, the protection leg NPV is zero, otherwise we call into the
    // derived engine to compute this

    Real protection = 0.0;
    if (arguments_.protectionEnd > referenceDate_) {
        gridDates_ = buildDiscretisationGrid(referenceDate_, arguments_.protectionStart, arguments_.protectionEnd,
                                             arguments_.underlying, maxGapDays_, maxDiscretisationPoints_);
        protection = protectionLegNpv();
    }

    // compute the total NPV, we buy the protection if we pay the fee

    results_.value = (arguments_.protectionFeePayer ? 1.0 : -1.0) * (protection - fee);

    // set additional results

    std::vector<Real> gridPeriodPds;
    for (Size i = 0; i < gridDates_.size() - 1; ++i)
        gridPeriodPds.push_back(defaultCurve_->defaultProbability(gridDates_[i], gridDates_[i + 1]));

    results_.additionalResults["GridDates"] = gridDates_;
    results_.additionalResults["ProtectionLegNpv"] = (arguments_.protectionFeePayer ? 1.0 : -1.0) * protection;
    results_.additionalResults["FeeLegNpv"] = (arguments_.protectionFeePayer ? 1.0 : -1.0) * fee;
    results_.additionalResults["RecoveryRate"] = effectiveRecoveryRate_;
    results_.additionalResults["GridPeriodPDs"] = gridPeriodPds;
    results_.additionalResults["ParticipationRate"] = arguments_.participationRate;

    for (Size l = 0; l < arguments_.protectionFee.size(); ++l) {
        results_.additionalResults["FeeStartDates"] = feeStartDates[l];
        results_.additionalResults["FeeEndDates"] = feeEndDates[l];
        results_.additionalResults["FeePayDates"] = feePayDates[l];
        results_.additionalResults["FeeMidDates"] = feeMidDates[l];
        results_.additionalResults["FeeAmounts"] = feeAmounts[l];
        results_.additionalResults["FeeMidAccrueds"] = feeMidAccrueds[l];
        results_.additionalResults["FeeMidDiscounts"] = feeMidDiscounts[l];
        results_.additionalResults["FeeDiscounts"] = feeDiscounts[l];
        results_.additionalResults["FeeSurvivalProbs"] = feeSurvivalProbs[l];
        results_.additionalResults["FeePeriodPDs"] = feePeriodPDs[l];
        results_.additionalResults["FeeFXSpot"] = feeFXSpot;
        results_.additionalResults["FeeCurrency"] = arguments_.protectionFeeCcys[l];
    }
}

} // namespace data
} // namespace ore
