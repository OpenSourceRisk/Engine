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

#include <qle/pricingengines/analyticjyyoycapfloorengine.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <qle/cashflows/jyyoyinflationcouponpricer.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/pricingengines/analyticjycpicapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>

using QuantLib::blackFormula;
using QuantLib::CPICapFloor;
using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::NullCalendar;
using QuantLib::Option;
using QuantLib::Period;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Settings;
using QuantLib::SimpleCashFlow;
using QuantLib::Size;
using QuantLib::Unadjusted;
using QuantLib::YoYInflationCapFloor;
using QuantLib::YoYInflationIndex;
using QuantLib::ZeroInflationIndex;
using std::max;
using std::pow;
using std::sqrt;

namespace QuantExt {

AnalyticJyYoYCapFloorEngine::AnalyticJyYoYCapFloorEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index,
                                                         bool indexIsInterpolated)
    : model_(model), index_(index), indexIsInterpolated_(indexIsInterpolated) {}

void AnalyticJyYoYCapFloorEngine::calculate() const {

    QL_REQUIRE(model_, "AnalyticJyYoYCapFloorEngine requires a model.");

    Date today = Settings::instance().evaluationDate();

    auto type = arguments_.type;

    // Shorten types below
    const auto yoyCap = YoYInflationCapFloor::Cap;
    const auto yoyFlr = YoYInflationCapFloor::Floor;
    const auto yoyClr = YoYInflationCapFloor::Collar;

    // Floor indicator used below.
    Real indFlr = type == yoyFlr ? 1.0 : -1.0;

    Size irIdx = model_->ccyIndex(model_->infjy(index_)->currency());
    auto yts = model_->irlgm1f(irIdx)->termStructure();

    // For each YoY optionlet, there are four scenarios:
    // 1. The YoY optionlet payment has already occurred (depends on some settings) => skip it.
    // 2. The underlying YoY rate is known but has not been paid. Deterministic discounted value.
    // 3. The denominator in the underlying YoY rate is known but the numerator is not known. We have a CPI
    //    optionlet.
    // 4. Both the denominator and numerator in the underlying YoY rate are not yet known. We have a "true" YoY
    //    optionlet.
    // Both 3 and 4 are covered by the JY YoY optionlet formula from Chapter 13 of the book. In the case of 3, the
    // formula for the mean and variance collapses to that of a CPI optionlet.
    results_.value = 0.0;
    for (Size i = 0; i < arguments_.payDates.size(); ++i) {

        // Check for scenario 1.
        const auto& payDate = arguments_.payDates[i];
        SimpleCashFlow cf(0.0, payDate);
        if (cf.hasOccurred())
            continue;

        // Discount to payment
        auto df = yts->discount(payDate);

        // Current optionlet values for brevity.
        const auto& dt = arguments_.accrualTimes[i];
        const auto& n = arguments_.nominals[i];
        const auto& g = arguments_.gearings[i];
        auto c = 1.0 + arguments_.capRates[i];
        auto f = 1.0 + arguments_.floorRates[i];

        // Check for scenario 2.
        const auto& fixingDate = arguments_.fixingDates[i];
        if (fixingDate <= today) {

            Real payoff = 0.0;
            auto yoyFixing = arguments_.index->fixing(fixingDate);

            if (type == yoyCap || type == yoyClr) {
                payoff = max(yoyFixing - c, 0.0);
            }

            if (type == yoyFlr || type == yoyClr) {
                payoff += indFlr * max(f - yoyFixing, 0.0);
            }

            results_.value += n * g * dt * payoff * df;
            continue;
        }

        // If we get to here, we are in scenario 3 or 4.
        Date denFixingDate = fixingDate - 1 * Years;
        auto zts = model_->infjy(index_)->realRate()->termStructure();
        auto S = inflationTime(denFixingDate, *zts, indexIsInterpolated_);
        auto T = inflationTime(fixingDate, *zts, indexIsInterpolated_);

        Real mean = jyExpectedIndexRatio(model_, index_, S, T, indexIsInterpolated_);
        Real stdDev = sqrt(varianceLogRatio(S, T));

        Real payoff = 0.0;
        if (type == yoyCap || type == yoyClr) {
            payoff = blackFormula(Option::Call, c, mean, stdDev, df, 0.0);
        }
        if (type == yoyFlr || type == yoyClr) {
            payoff += indFlr * blackFormula(Option::Put, f, mean, stdDev, df, 0.0);
        }

        results_.value += n * g * dt * payoff;
    }
}

Real AnalyticJyYoYCapFloorEngine::varianceLogRatio(Time S, Time T) const {

    using namespace CrossAssetAnalytics;

    // Short variable names for formula below.
    auto i = model_->ccyIndex(model_->infjy(index_)->currency());
    auto j = index_;

    // H_n(S), H_n(T) and \zeta_n(S)
    Real H_n_S = Hz(i).eval(*model_, S);
    Real H_n_T = Hz(i).eval(*model_, T);
    Real z_n_S = zetaz(i).eval(*model_, S);

    // H_r(S), H_r(T) and \zeta_r(S)
    Real H_r_S = Hy(j).eval(*model_, S);
    Real H_r_T = Hy(j).eval(*model_, T);
    Real z_r_S = zetay(j).eval(*model_, S);

    // As per section 13 of book i.e. \nu = Var \left[ \ln \frac{I(T)}{I(S)} \right]
    Real var = integral(*model_, P(az(i), az(i), LC(H_n_T, -1.0, Hz(i)), LC(H_n_T, -1.0, Hz(i))), S, T);
    var += integral(*model_, P(ay(j), ay(j), LC(H_r_T, -1.0, Hy(j)), LC(H_r_T, -1.0, Hy(j))), S, T);
    var += integral(*model_, P(sy(j), sy(j)), S, T);
    var -= 2 * integral(*model_, P(rzy(i, j, 0), az(i), LC(H_n_T, -1.0, Hz(i)), ay(j), LC(H_r_T, -1.0, Hy(j))), S, T);
    var += 2 * integral(*model_, P(rzy(i, j, 1), az(i), LC(H_n_T, -1.0, Hz(i)), sy(j)), S, T);
    var -= 2 * integral(*model_, P(ryy(j, j, 0, 1), ay(j), LC(H_r_T, -1.0, Hy(j)), sy(j)), S, T);
    var += (H_n_T - H_n_S) * (H_n_T - H_n_S) * z_n_S;
    var += (H_r_T - H_r_S) * (H_r_T - H_r_S) * z_r_S;
    var -= 2 * (H_n_T - H_n_S) * (H_r_T - H_r_S) * integral(*model_, P(rzy(i, j, 0), az(i), ay(j)), 0.0, S);

    return var;
}

} // namespace QuantExt
