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

#include <qle/pricingengines/numericlgmbgsflexiswapengine.hpp>

#include <ql/cashflows/coupon.hpp>

namespace QuantExt {

NumericLgmBgsFlexiSwapEngine::NumericLgmBgsFlexiSwapEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                           const Real sy, const Size ny, const Real sx, const Size nx,
                                                           const Handle<Quote>& minCpr, const Handle<Quote>& maxCpr,
                                                           const Handle<YieldTermStructure>& discountCurve,
                                                           const Method method, const Real singleSwaptionThreshold)
    : NumericLgmFlexiSwapEngineBase(model, sy, ny, sx, nx, discountCurve, method, singleSwaptionThreshold),
      minCpr_(minCpr), maxCpr_(maxCpr) {
    registerWith(this->model());
    registerWith(discountCurve_);
    registerWith(minCpr_);
    registerWith(maxCpr_);
} // NumericLgmFlexiSwapEngine::NumericLgmFlexiSwapEngine

namespace {
Real getNotional(const std::vector<Real> nominals, const std::vector<Date> dates, const Date& d) {
    QL_REQUIRE(nominals.size() + 1 == dates.size(), "getNominal(): nominals size (" << nominals.size()
                                                                                    << ") + 1 must be dates size ("
                                                                                    << dates.size() << ")");
    if (d < dates.front() || d >= dates.back())
        return 0.0;
    Size l = std::upper_bound(dates.begin(), dates.end(), d) - dates.begin();
    return nominals[l - 1];
}
} // namespace

void NumericLgmBgsFlexiSwapEngine::calculate() const {
    Date today = Settings::instance().evaluationDate();
    // compute the lower and upper notionals bounds, in terms of the tranche notional schedule
    Real currentLowerNotionalAfterPrepayment = 0.0, currentUpperNotionalAfterPrepayment = 0.0,
         lastAggregatePrincipal = 0.0;
    std::vector<Real> tmpLowerNotionalBound, tmpUpperNotionalBound;
    Real effectiveMinCpr = minCpr_->value() / static_cast<Real>(arguments_.trancheNominalFrequency);
    Real effectiveMaxCpr = maxCpr_->value() / static_cast<Real>(arguments_.trancheNominalFrequency);
    for (Size i = 0; i < arguments_.trancheNominalDates.size() - 1; ++i) {
        Real aggregatePrincipal = 0.0;
        for (Size j = 0; j < arguments_.trancheNominals.size(); ++j)
            aggregatePrincipal += arguments_.trancheNominals[j][i];
        if (i == 0) {
            // in the first period we do not have a prepayment
            currentUpperNotionalAfterPrepayment = aggregatePrincipal;
            currentLowerNotionalAfterPrepayment = aggregatePrincipal;
        }
        if (i > 0) {
            // ratio of zero-cpr notionals
            Real amortisationRate =
                QuantLib::close_enough(aggregatePrincipal, 0.0) ? 0.0 : (aggregatePrincipal / lastAggregatePrincipal);
            // we only prepay if the start date of the nominal is in the future
            currentUpperNotionalAfterPrepayment =
                currentUpperNotionalAfterPrepayment *
                std::max(amortisationRate - (arguments_.trancheNominalDates[i] > today ? effectiveMinCpr : 0.0), 0.0);
            currentLowerNotionalAfterPrepayment =
                currentLowerNotionalAfterPrepayment *
                std::max(amortisationRate - (arguments_.trancheNominalDates[i] > today ? effectiveMaxCpr : 0.0), 0.0);
        }
        // now that we have the current notional of the period we can determine the swap notional
        // for this, we subtract the notionals of all tranches which are less senior...
        Real subordinatedPrincipal = 0;
        for (Size k = arguments_.trancheNominals.size() - 1; k > arguments_.referencedTranche; --k) {
            subordinatedPrincipal += arguments_.trancheNominals[k][i];
        }
        // and then cap the result at the referenced tranche's volume and floor it at zero
        tmpLowerNotionalBound.push_back(
            std::min(std::max(currentLowerNotionalAfterPrepayment - subordinatedPrincipal, 0.0),
                     arguments_.trancheNominals[arguments_.referencedTranche][i]));
        tmpUpperNotionalBound.push_back(
            std::min(std::max(currentUpperNotionalAfterPrepayment - subordinatedPrincipal, 0.0),
                     arguments_.trancheNominals[arguments_.referencedTranche][i]));
        // update the aggregate principal for the next period
        lastAggregatePrincipal = aggregatePrincipal;
    }

    // convert the bounds to notional vectors for the fixed and floating schedule
    std::vector<Real> lowerNotionalFixedBound, upperNotionalFixedBound, upperNotionalFloatingBound;
    for (Size i = 0; i < arguments_.fixedResetDates.size(); ++i) {
        lowerNotionalFixedBound.push_back(
            getNotional(tmpLowerNotionalBound, arguments_.trancheNominalDates, arguments_.fixedResetDates[i]));
        upperNotionalFixedBound.push_back(
            getNotional(tmpUpperNotionalBound, arguments_.trancheNominalDates, arguments_.fixedResetDates[i]));
    }
    // derive floating nominal schedule from fixed to ensure they match
    Size ratio =
        arguments_.floatingResetDates.size() / arguments_.fixedResetDates.size(); // we know there is no remainder
    upperNotionalFloatingBound.resize(upperNotionalFixedBound.size() * ratio);
    Size i = 0; // remove in C++14 and instead write [i=0, &fixedNotional] in the capture below
    std::generate(upperNotionalFloatingBound.begin(), upperNotionalFloatingBound.end(),
                  [i, &upperNotionalFixedBound, ratio]() mutable { return upperNotionalFixedBound[i++ / ratio]; });

    // recalculate the fixed and floating coupons belonging to the upper Notional
    std::vector<Real> upperFixedCoupons, upperFloatingCoupons;
    for (Size i = 0; i < arguments_.fixedLeg.size(); ++i) {
        auto cp = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(arguments_.fixedLeg[i]);
        upperFixedCoupons.push_back(cp->accrualPeriod() * cp->rate() * upperNotionalFixedBound[i]);
    }
    for (Size i = 0; i < arguments_.floatingLeg.size(); ++i) {
        auto cp = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(arguments_.floatingLeg[i]);
        try {
            upperFloatingCoupons.push_back(cp->accrualPeriod() * cp->rate() * upperNotionalFloatingBound[i]);
        } catch (...) {
            upperFloatingCoupons.push_back(Null<Real>());
        }
    }

    // determine the option position, the holder is the payer of the structured (i.e. fixed) leg
    QuantLib::Position::Type flexiOptionPosition =
        arguments_.type == VanillaSwap::Payer ? QuantLib::Position::Long : QuantLib::Position::Short;

    // set arguments in base engine
    type = arguments_.type;
    fixedNominal = upperNotionalFixedBound;       // computed above
    floatingNominal = upperNotionalFloatingBound; // computed above
    fixedResetDates = arguments_.fixedResetDates;
    fixedPayDates = arguments_.fixedPayDates;
    floatingAccrualTimes = arguments_.floatingAccrualTimes;
    floatingResetDates = arguments_.floatingResetDates;
    floatingFixingDates = arguments_.floatingFixingDates;
    floatingPayDates = arguments_.floatingPayDates;
    fixedCoupons = upperFixedCoupons;
    fixedRate = arguments_.fixedRate;
    floatingGearings = arguments_.floatingGearings;
    floatingSpreads = arguments_.floatingSpreads;
    cappedRate = arguments_.cappedRate;
    flooredRate = arguments_.flooredRate;
    floatingCoupons = upperFloatingCoupons;
    iborIndex = arguments_.iborIndex;
    lowerNotionalBound = lowerNotionalFixedBound; // computed above
    optionPosition = flexiOptionPosition;         // computed above
    notionalCanBeDecreased =
        std::vector<bool>(fixedNominal.size(), true); // each period is eligable for notional decrease

    // calculate and set results
    auto result = NumericLgmFlexiSwapEngineBase::calculate();
    results_.value = result.first;
    results_.additionalResults = getAdditionalResultsMap(model()->getCalibrationInfo());
} // NumericLgmBgsFlexiSwapEngine::calculate

} // namespace QuantExt
