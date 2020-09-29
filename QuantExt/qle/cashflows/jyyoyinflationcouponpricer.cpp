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

#include <qle/cashflows/jyyoyinflationcouponpricer.hpp>
#include <qle/models/crossassetanalytics.hpp>

using QuantLib::Option;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Size;

namespace QuantExt {

JyYoYInflationCouponPricer::JyYoYInflationCouponPricer(const boost::shared_ptr<CrossAssetModel>& model, Size index)
    : model_(model), index_(index) {

    Size irIdx = model_->ccyIndex(model_->infjy(index_)->currency());
    nominalTermStructure_ = model_->irlgm1f(irIdx)->termStructure();

    registerWith(model_);
    registerWith(nominalTermStructure_);
}

Real JyYoYInflationCouponPricer::optionletRate(Option::Type optionType, Real effStrike) const {
    QL_FAIL("JyYoYInflationCouponPricer::optionletRate: not implemented.");
}

Rate JyYoYInflationCouponPricer::adjustedFixing(Rate) const {
    
    // We only need to use the Jarrow Yildrim model if both I(T) and I(S) are not yet known (i.e. published).
    // If only I(S) is known, we have a ZCIIS. If both are known, we just use the published values. In either case, 
    // we can just ask the inflation index for its fixing.

    // Fixing date associated with numerator inflation index value i.e. I(T). Incorporates the observation lag.
    // It is essentially: coupon_end_date - contract_observation_lag.
    Date numFixingDate = coupon_->fixingDate();

    // Fixing date associated with denominator inflation index value i.e. I(S).
    Date denFixingDate = numFixingDate - 1 * Years;

    // If everything has been published in order to determine I(S), return model independent value read off curve.
    // Logic to determine last available fixing and where forecasting is needed copied from YoYInflationIndex::fixing.
    Date today = Settings::instance().evaluationDate();
    auto freq = coupon_->index()->frequency();
    auto ip = inflationPeriod(today - coupon_->index()->availabilityLag(), freq);
    bool isInterp = coupon_->index()->interpolated();
    if ((!isInterp && denFixingDate < ip.first) || (isInterp && denFixingDate < ip.first - Period(freq))) {
        return coupon_->indexFixing();
    }

    // Use the JY model to calculate the adjusted fixing.
    using namespace CrossAssetAnalytics;
    auto irIdx = model_->ccyIndex(model_->infjy(index_)->currency());
    auto irTs = model_->irlgm1f(irIdx)->termStructure();
    auto zts = model_->infjy(index_)->realRate()->termStructure();

    auto T = zts->timeFromReference(numFixingDate);
    auto S = zts->timeFromReference(denFixingDate);

    // Calculate growthRatio: \frac{P_r(0,T)}{P_n(0,T)} / \frac{P_r(0,S)}{P_n(0,S)}
    auto growthRatio = inflationGrowth(zts, T) / inflationGrowth(zts, S);
    
    // Calculate exponent of the convexity adjustment i.e. c.
    auto rrParam = model_->infjy(index_)->realRate();
    auto H_r_S = rrParam->H(S);
    auto H_r_T = rrParam->H(T);
    auto H_n_S = model_->irlgm1f(irIdx)->H(S);

    auto c = H_r_S * rrParam->zeta(S);
    c -= H_n_S * integral(model_.get(), P(rzy(irIdx, index_, 0), az(irIdx), ay(index_)), 0.0, S);
    c += integral(model_.get(), LC(0.0, -1.0, P(ay(index_), ay(index_), Hy(index_)),
        1.0, P(rzy(irIdx, index_, 0), az(irIdx), ay(index_), Hz(irIdx)),
        -1.0, P(ryy(index_, index_, 0, 1), ay(index_), sy(index_))), 0.0, S);
    c *= (H_r_S - H_r_T);

    return growthRatio * exp(c) - 1;
}

}
