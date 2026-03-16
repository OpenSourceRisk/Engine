/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/indexes/interpolatediborindex.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

InterpolatedIborIndex::InterpolatedIborIndex(const QuantLib::ext::shared_ptr<IborIndex>& shortIndex,
                                             const QuantLib::ext::shared_ptr<IborIndex>& longIndex,
                                             const Size calendarDays, const QuantLib::Rounding& rounding,
                                             const Handle<YieldTermStructure> overwriteEstimationCurve,
                                             const bool parCouponMode)
    : InterestRateIndex(shortIndex->familyName(),
                        shortIndex->tenor() == longIndex->tenor() ? shortIndex->tenor() : calendarDays * Days,
                        shortIndex->fixingDays(), shortIndex->currency(), shortIndex->fixingCalendar(),
                        shortIndex->dayCounter()),
      shortIndex_(shortIndex), longIndex_(longIndex), calendarDays_(calendarDays), rounding_(rounding),
      overwriteEstimationCurve_(overwriteEstimationCurve), parCouponMode_(parCouponMode) {

    QL_REQUIRE(shortIndex_, "InterpolatedIborIndex(): shortIndex is null");
    QL_REQUIRE(longIndex_, "InterpolatedIborIndex(): longIndex is null");
    QL_REQUIRE(shortIndex_->familyName() == longIndex_->familyName(), "InterpolatedIborIndex(): family name mismatch");
    QL_REQUIRE(shortIndex_->fixingDays() == longIndex_->fixingDays(), "InterpolatedIborIndex(): fixing days mismatch");
    QL_REQUIRE(shortIndex_->currency() == longIndex_->currency(), "InterpolatedIborIndex(): currency mismatch");
    QL_REQUIRE(shortIndex_->fixingCalendar() == longIndex_->fixingCalendar(),
               "InterpolatedIborIndex(): calendar mismatch");
    QL_REQUIRE(shortIndex_->dayCounter() == longIndex_->dayCounter(), "InterpoaltedIborIndex(): daycounter mismatch");
    QL_REQUIRE(shortIndex_->tenor() <= longIndex_->tenor(), "InterpolatedIborIndex(): short index tenor ("
                                                                << shortIndex_->tenor()
                                                                << ") must be shorter or equal than long index tenor ("
                                                                << longIndex_->tenor() << ")");
    noInterpolation_ = shortIndex_->tenor() == longIndex_->tenor();

    registerWith(shortIndex);
    registerWith(longIndex);
    registerWith(overwriteEstimationCurve);
    registerWith(Settings::instance().evaluationDate());

    // overwrite name (if index is effectively interpolated)
    if (!noInterpolation_) {
        std::ostringstream out;
        out << familyName() << io::short_period(calendarDays * Days) << " (Interpolated "
            << io::short_period(shortIndex->tenor()) << "/" << io::short_period(longIndex->tenor()) << ") "
            << dayCounter().name();
        name_ = out.str();
    }
}

Real InterpolatedIborIndex::shortWeight(const Date& fixingDate) const {
    if (noInterpolation_)
        return 1.0;
    Date s1 = shortIndex_->valueDate(fixingDate);
    Date e1 = shortIndex_->maturityDate(s1);
    Date s2 = longIndex_->valueDate(fixingDate);
    Date e2 = longIndex_->maturityDate(s2);
    Size l1 = e1 - s1;
    Size l2 = e2 - s2;
    if (calendarDays_ < l1)
        return 1.0;
    else if (calendarDays_ > l2)
        return 0.0;
    else
        return static_cast<Real>(l2 - calendarDays_) / static_cast<Real>(l2 - l1);
}

Real InterpolatedIborIndex::longWeight(const Date& fixingDate) const { return 1.0 - shortWeight(fixingDate); }

Real InterpolatedIborIndex::forecastFixing(const Date& fixingDate) const {
    Real res;
    if (noInterpolation_) {
        if (parCouponMode_) {
            Date valueDate = shortIndex_->valueDate(fixingDate);
            Handle<YieldTermStructure> yts =
                overwriteEstimationCurve_.empty() ? shortIndex_->forwardingTermStructure() : overwriteEstimationCurve_;
            res = (yts->discount(valueDate) / yts->discount(valueDate + calendarDays_) - 1.0) /
                  shortIndex_->dayCounter().yearFraction(valueDate, valueDate + calendarDays_);
        } else {
            res = overwriteEstimationCurve_.empty()
                      ? shortIndex_->fixing(fixingDate, false)
                      : shortIndex_->clone(overwriteEstimationCurve_)->fixing(fixingDate, false);
        }
    } else {
        Real w = shortWeight(fixingDate);
        // handle the case where one of the indices has a historic fixing on the evaluation date
        if (overwriteEstimationCurve_.empty())
            res = shortIndex_->fixing(fixingDate, false) * w + longIndex_->fixing(fixingDate, false) * (1.0 - w);
        else
            res = shortIndex_->clone(overwriteEstimationCurve_)->fixing(fixingDate, false) * w +
                  longIndex_->clone(overwriteEstimationCurve_)->fixing(fixingDate, false) * (1.0 - w);
    }
    // we don't apply rounding to fixings in the future
    if (fixingDate > Settings::instance().evaluationDate())
        return res;
    else
        return rounding_(res);
}

Real InterpolatedIborIndex::pastFixing(const Date& fixingDate) const {
    Real sf = shortIndex_->pastFixing(fixingDate);
    Real lf = longIndex_->pastFixing(fixingDate);
    if (sf == Null<Real>() || lf == Null<Real>())
        return Null<Real>();
    Real w = shortWeight(fixingDate);
    return rounding_(sf * w + lf * (1.0 - w));
}

Date InterpolatedIborIndex::maturityDate(const Date& valueDate) const {
    if (noInterpolation_)
        return shortIndex_->maturityDate(valueDate);
    // no adjustment, take the plain calendar days, maturity date might be a holiday!
    return valueDate + calendarDays_ * Days;
}

QuantLib::ext::shared_ptr<InterpolatedIborIndex>
InterpolatedIborIndex::clone(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& shortForwardCurve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& longForwardCurve) const {
    return QuantLib::ext::make_shared<InterpolatedIborIndex>(
        shortIndex()->clone(shortForwardCurve), longIndex()->clone(longForwardCurve),
        calendarDays(), rounding(), Handle<YieldTermStructure>(), parCouponMode());
}

QuantLib::ext::shared_ptr<InterpolatedIborIndex>
InterpolatedIborIndex::clone(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& overwriteForwardCurve) const {
    return QuantLib::ext::make_shared<InterpolatedIborIndex>(
        shortIndex(), longIndex(), calendarDays(), rounding(),
        overwriteForwardCurve, parCouponMode());
}

} // namespace QuantExt
