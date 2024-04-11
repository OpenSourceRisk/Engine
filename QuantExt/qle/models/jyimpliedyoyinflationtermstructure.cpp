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

#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/models/jyimpliedyoyinflationtermstructure.hpp>
#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>
#include <qle/utilities/inflation.hpp>

using QuantLib::Date;
using QuantLib::InterpolatedDiscountCurve;
using QuantLib::Linear;
using QuantLib::MakeSchedule;
using QuantLib::PiecewiseYoYInflationCurve;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Size;
using QuantLib::Time;
using QuantLib::YearOnYearInflationSwapHelper;
using std::exp;
using std::map;
using std::vector;

namespace QuantExt {

JyImpliedYoYInflationTermStructure::JyImpliedYoYInflationTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                                                       Size index, bool indexIsInterpolated)
    : YoYInflationModelTermStructure(model, index, indexIsInterpolated) {}

map<Date, Real> JyImpliedYoYInflationTermStructure::yoyRates(const vector<Date>& dts, const Period& obsLag) const {

    // First step is to calculate the YoY swap rate for each maturity date in dts and store in yyiisRates.
    map<Date, Real> yoySwaplets;
    map<Date, Real> discounts;
    map<Date, Real> yyiisRates;
    auto irIdx = model_->ccyIndex(model_->infjy(index_)->currency());

    // Will need a YoY index below in the helpers.
    QuantLib::ext::shared_ptr<YoYInflationIndex> index =
        QuantLib::ext::make_shared<YoYInflationIndexWrapper>(model_->infjy(index_)->inflationIndex(), indexIsInterpolated());

    for (const auto& maturity : dts) {

        // Schedule for the YoY swap with maturity date equal to `maturity`
        Schedule schedule = MakeSchedule()
                                .from(referenceDate_)
                                .to(maturity)
                                .withTenor(1 * Years)
                                .withConvention(Unadjusted)
                                .withCalendar(calendar())
                                .backwards();

        // Store the value of the model implied YoY leg and the value of the fixed leg annuity.
        Real yoyLegValue = 0.0;
        Real fixedLegAnnuity = 0.0;

        for (Size i = 1; i < schedule.dates().size(); ++i) {

            // Start and end of the current YoY swaplet period.
            const auto& start = schedule.dates()[i - 1];
            const auto& end = schedule.dates()[i];

            // If we have already calculated the YoY swaplet price for this period, use it.
            // We should always have a discount factor in this case also.
            auto it = yoySwaplets.find(end);
            if (it != yoySwaplets.end()) {
                yoyLegValue += it->second;
                fixedLegAnnuity += discounts.at(end);
                continue;
            }

            // Need to calculate the YoY swaplet value over the period [start, end]
            Real swaplet;
            auto T = relativeTime_ + dayCounter().yearFraction(referenceDate_, end);
            auto discount = model_->discountBond(irIdx, relativeTime_, T, state_[2]);
            if (i == 1) {
                // The first YoY swaplet is a zero coupon swaplet because I_{start} is known.
                auto growth =
                    inflationGrowth(model_, index_, relativeTime_, T, state_[2], state_[0], indexIsInterpolated_);
                swaplet = discount * (growth - 1.0);
            } else {
                auto S = relativeTime_ + dayCounter().yearFraction(referenceDate_, start);
                swaplet = yoySwaplet(S, T);
            }

            // Cache the swaplet value and the discount factor related to this swaplet end date.
            yoySwaplets[end] = swaplet;
            discounts[end] = discount;

            // Update the YoY leg value and the fixed leg annuity
            yoyLegValue += swaplet;
            fixedLegAnnuity += discount;
        }

        // The model implied YoY inflation swap rate
        yyiisRates[maturity] = yoyLegValue / fixedLegAnnuity;
    }

    QL_REQUIRE(!yyiisRates.empty(), "JyImpliedYoYInflationTermStructure: yoyRates did not create any YoY swap rates.");

    // Will need a discount term structure in the bootstrap below so create it here from the discounts map.
    vector<Date> dfDates;
    vector<Real> dfValues;

    if (discounts.count(referenceDate_) == 0) {
        dfDates.push_back(referenceDate_);
        dfValues.push_back(1.0);
    }

    for (const auto& kv : discounts) {
        dfDates.push_back(kv.first);
        dfValues.push_back(kv.second);
    }

    auto irTs = model_->irlgm1f(irIdx)->termStructure();
    Handle<YieldTermStructure> yts(
        QuantLib::ext::make_shared<InterpolatedDiscountCurve<LogLinear>>(dfDates, dfValues, irTs->dayCounter(), LogLinear()));

    // Create the YoY swap helpers from the YoY swap rates calculated above.
    // Using the curve's day counter as the helper's day counter for now.
    using YoYHelper = BootstrapHelper<YoYInflationTermStructure>;
    vector<QuantLib::ext::shared_ptr<YoYHelper>> helpers;
    for (const auto& kv : yyiisRates) {
        Handle<Quote> yyiisQuote(QuantLib::ext::make_shared<SimpleQuote>(kv.second));
        helpers.push_back(QuantLib::ext::make_shared<YearOnYearInflationSwapHelper>(
            yyiisQuote, observationLag(), kv.first, calendar(), Unadjusted, dayCounter(), index, yts));
    }

    // Create a YoY curve from the helpers
    // Use Linear here in line with what is in scenariosimmarket and todaysmarket but should probably be more generic.
    auto lag = obsLag == -1 * Days ? observationLag() : obsLag;
    auto baseRate = helpers.front()->quote()->value();
    auto yoyCurve = QuantLib::ext::make_shared<PiecewiseYoYInflationCurve<Linear>>(
        referenceDate_, calendar(), dayCounter(), lag, frequency(), indexIsInterpolated(), baseRate, helpers, 1e-12);

    // Read the necessary YoY rates from the bootstrapped YoY inflation curve
    map<Date, Real> result;
    for (const auto& maturity : dts) {
        result[maturity] = yoyCurve->yoyRate(maturity);
    }

    return result;
}

Real JyImpliedYoYInflationTermStructure::yoySwaplet(Time S, Time T) const {

    // The JY implied swap rate at time t for the period from S to T is:
    // N \tau(S, T) \left\{ P_n(t,S) \frac{P_r(t,T)}{P_r(t,S)} e^{C(t,S,T)} - P_n(t,T) \right\}
    // where N is the nominal, \tau(S, T) is the daycount which we assume equals 1 here.
    // e^{C(t,S,T)} is the correction term which we deal with below.

    // Get P_n(t,S) and P_n(t,T).
    auto irIdx = model_->ccyIndex(model_->infjy(index_)->currency());
    auto irTs = model_->irlgm1f(irIdx)->termStructure();
    auto p_n_t_S = model_->discountBond(irIdx, relativeTime_, S, state_[2]);
    auto p_n_t_T = model_->discountBond(irIdx, relativeTime_, T, state_[2]);

    // Get the rrRatio := P_r(t,T)}{P_r(t,S)}.
    auto rrParam = model_->infjy(index_)->realRate();
    auto H_r_S = rrParam->H(S);
    auto H_r_T = rrParam->H(T);
    auto zeta_r_t = rrParam->zeta(relativeTime_);
    auto rrRatio = exp(-(H_r_T - H_r_S) * state_[0] - 0.5 * (H_r_T * H_r_T - H_r_S * H_r_S) * zeta_r_t);

    const auto& zts = model_->infjy(index_)->realRate()->termStructure();
    rrRatio *= (irTs->discount(T) * inflationGrowth(zts, T, indexIsInterpolated_)) /
               (irTs->discount(S) * inflationGrowth(zts, S, indexIsInterpolated_));

    // Calculate the correction term C(t,S,T)
    using CrossAssetAnalytics::ay;
    using CrossAssetAnalytics::az;
    using CrossAssetAnalytics::Hy;
    using CrossAssetAnalytics::Hz;
    using CrossAssetAnalytics::integral;
    using CrossAssetAnalytics::LC;
    using CrossAssetAnalytics::P;
    using CrossAssetAnalytics::ryy;
    using CrossAssetAnalytics::rzy;
    using CrossAssetAnalytics::sy;

    auto H_n_S = model_->irlgm1f(irIdx)->H(S);
    auto zeta_r_S = rrParam->zeta(S);

    auto c = H_r_S * (zeta_r_S - zeta_r_t);
    c -= H_n_S * integral(*model_, P(rzy(irIdx, index_, 0), az(irIdx), ay(index_)), relativeTime_, S);
    c += integral(*model_,
                  LC(0.0, -1.0, P(ay(index_), ay(index_), Hy(index_)), 1.0,
                     P(rzy(irIdx, index_, 0), az(irIdx), ay(index_), Hz(irIdx)), -1.0,
                     P(ryy(index_, index_, 0, 1), ay(index_), sy(index_))),
                  relativeTime_, S);
    c *= (H_r_S - H_r_T);

    return p_n_t_S * rrRatio * exp(c) - p_n_t_T;
}

void JyImpliedYoYInflationTermStructure::checkState() const {
    // For JY YoY, expect the state to be three variables i.e. z_I and c_I and z_{ir}.
    QL_REQUIRE(state_.size() == 3, "JyImpliedYoYInflationTermStructure: expected state to have "
                                       << "three elements but got " << state_.size());
}

} // namespace QuantExt
