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

#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>

#include <qle/models/yoyinflationmodeltermstructure.hpp>

using QuantLib::Array;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;
using QuantLib::Time;
using QuantLib::InterpolatedDiscountCurve;
using QuantLib::Linear;
using QuantLib::PiecewiseYoYInflationCurve;
using QuantLib::YearOnYearInflationSwapHelper;

namespace QuantExt {
// we set baseRate to zero because inflationTermStructure gives us a zero inf curve,
// zeroinflationtermstructure doesnt have a base rate anymore but the inflation curve base class
// the base class throws at null (missing base rate)
// seems incomplete refactoring in QL, since yoy termstructure still has base rate.
YoYInflationModelTermStructure::YoYInflationModelTermStructure(
    const QuantLib::Handle<CrossAssetModel>& model, Size index,
    const std::optional<QuantLib::DayCounter>& simulationDayCounter)
    : YoYInflationTermStructure(inflationTermStructure(*model, index)->baseDate(), 0.0,
                                inflationTermStructure(*model, index)->frequency(),
                                inflationTermStructure(*model, index)->dayCounter()),
      model_(model), index_(index), simulationDayCounter_(simulationDayCounter),
      referenceDate_(inflationTermStructure(*model_, index_)->referenceDate()), relativeTime_(0.0) {
    registerWith(model_);
    update();
}

void YoYInflationModelTermStructure::update() { notifyObservers(); }

Date YoYInflationModelTermStructure::maxDate() const {
    // we don't care. Let the underlying classes throw exceptions if applicable
    return Date::maxDate();
}

Time YoYInflationModelTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

const Date& YoYInflationModelTermStructure::referenceDate() const { return referenceDate_; }

void YoYInflationModelTermStructure::referenceDate(const Date& d) {
    referenceDate_ = d;
    // we use the simulation day counter, otherwise both times could be from different times
    relativeTime_ = simulationDayCounter_.value_or(dayCounter())
                        .yearFraction(inflationTermStructure(*model_, index_)->referenceDate(), referenceDate_);
    update();
}

Date YoYInflationModelTermStructure::baseDate() const {
    // The inflation models are continous time, we need to keep the initial lag here constant
    return referenceDate_ - simulationLagDays();
}

void YoYInflationModelTermStructure::state(const Array& s) {
    state_ = s;
    checkState();
    notifyObservers();
}

void YoYInflationModelTermStructure::move(const Date& d, const Array& s) {
    state(s);
    referenceDate(d);
}

Real YoYInflationModelTermStructure::yoyRate(const Date& d, const Period& obsLag, bool forceLinearInterpolation,
                                             bool extrapolate) const {
    return yoyRates({d}, {obsLag}).at(d);
}

Real YoYInflationModelTermStructure::yoyRateImpl(Time t) const {
    QL_FAIL("YoYInflationModelTermStructure::yoyRateImpl cannot be called.");
}

std::map<QuantLib::Date, QuantLib::Real> YoYInflationModelTermStructure::modelParRatesToSwapletRates(
    const std::vector<QuantLib::Date>& dates, const QuantLib::Period& obsLag,
    const std::map<QuantLib::Date, QuantLib::Real>& parRates,
    const std::map<QuantLib::Date, QuantLib::Real>& discounts,
    const QuantLib::Size irIndex,
    const QuantLib::ext::shared_ptr<ZeroInflationIndex>& infIndex) const {
    QL_REQUIRE(!dates.empty(),
               "YoYInflationModelTermStructure::modelParRatesToSwapletRates: empty dates vector provided.");
    std::map<QuantLib::Date, QuantLib::Real> swapletRates;
    QuantLib::ext::shared_ptr<YoYInflationIndex> index =
        QuantLib::ext::make_shared<YoYInflationIndexWrapper>(infIndex);
    
    // Will need a discount term structure in the bootstrap below so create it here from the discounts map.
    std::vector<Date> dfDates;
    std::vector<Real> dfValues;

    if (discounts.count(referenceDate_) == 0) {
        dfDates.push_back(referenceDate_);
        dfValues.push_back(1.0);
    }

    for (const auto& kv : discounts) {
        dfDates.push_back(kv.first);
        dfValues.push_back(kv.second);
    }
    auto discountDayCounter = model_->irlgm1f(irIndex)->termStructure()->dayCounter();

    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<InterpolatedDiscountCurve<LogLinear>>(
        dfDates, dfValues, discountDayCounter, LogLinear()));

    // Create the YoY swap helpers from the YoY swap rates calculated above.
    // Using the curve's day counter as the helper's day counter for now.
    using YoYHelper = BootstrapHelper<YoYInflationTermStructure>;
    std::vector<QuantLib::ext::shared_ptr<YoYHelper>> helpers;
    for (size_t i = 0; i < dates.size(); ++i) {
        QuantLib::Date maturity = dates[i];
        auto it = parRates.find(dates[i]);
        QL_REQUIRE(it != parRates.end(), "YoYInflationModelTermStructure::yoySwaptletRates: par rate for maturity "
                                             << maturity
                                             << " not found in parRates map. Internal error. Contact developer.");
        Real parRate = it->second;
        Handle<Quote> yyiisQuote(QuantLib::ext::make_shared<SimpleQuote>(parRate));
        helpers.push_back(QuantLib::ext::make_shared<YearOnYearInflationSwapHelper>(
            yyiisQuote, obsLag, referenceDate_, maturity, calendar(), Unadjusted, dayCounter(), index,
            QuantLib::CPI::Flat, yts));
    }

    // Create a YoY curve from the helpers
    // Use Linear here in line with what is in scenariosimmarket and todaysmarket but should probably be more generic.
    auto baseRate = helpers.front()->quote()->value();
    auto yoyCurve = QuantLib::ext::make_shared<PiecewiseYoYInflationCurve<Linear>>(
        referenceDate_, baseDate(), baseRate, frequency(), dayCounter(), helpers);
    // Read the necessary YoY swaplet rates from the bootstrapped YoY inflation curve
    for (size_t i = 0; i < dates.size(); ++i) {
        auto fixingDate = inflationPeriod(dates[i] - obsLag, frequency()).first;
        swapletRates[dates[i]] = yoyCurve->yoyRate(fixingDate);
    }
    return swapletRates;
}
} // namespace QuantExt
