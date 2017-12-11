/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

namespace QuantExt {

ZeroInflationIndexWrapper::ZeroInflationIndexWrapper(const boost::shared_ptr<ZeroInflationIndex> source,
                                                     const CPI::InterpolationType interpolation)
    : ZeroInflationIndex(source->familyName(), source->region(), source->revised(), source->interpolated(),
                         source->frequency(), source->availabilityLag(), source->currency(),
                         source->zeroInflationTermStructure()),
      source_(source), interpolation_(interpolation) {}

Rate ZeroInflationIndexWrapper::fixing(const Date& fixingDate, bool /*forecastTodaysFixing*/) const {

    // duplicated logic from CPICashFlow::amount()

    // what interpolation do we use? Index / flat / linear
    if (interpolation_ == CPI::AsIndex) {
        return source_->fixing(fixingDate);
    } else {
        std::pair<Date, Date> dd = inflationPeriod(fixingDate, frequency());
        Real indexStart = source_->fixing(dd.first);
        if (interpolation_ == CPI::Linear) {
            Real indexEnd = source_->fixing(dd.second + Period(1, Days));
            // linear interpolation
            return indexStart + (indexEnd - indexStart) * (fixingDate - dd.first) /
                                    ((dd.second + Period(1, Days)) -
                                     dd.first); // can't get to next period's value within current period
        } else {
            // no interpolation, i.e. flat = constant, so use start-of-period value
            return indexStart;
        }
    }
}

YoYInflationIndexWrapper::YoYInflationIndexWrapper(const boost::shared_ptr<ZeroInflationIndex> zeroIndex,
                                                   const bool interpolated, const Handle<YoYInflationTermStructure>& ts)
    : YoYInflationIndex(zeroIndex->familyName(), zeroIndex->region(), zeroIndex->revised(), interpolated, true,
                        zeroIndex->frequency(), zeroIndex->availabilityLag(), zeroIndex->currency(), ts),
      zeroIndex_(zeroIndex) {
    registerWith(zeroIndex_);
}

Rate YoYInflationIndexWrapper::fixing(const Date& fixingDate, bool /*forecastTodaysFixing*/) const {

    // duplicated logic from YoYInflationIndex, this would not be necessary, if forecastFixing
    // was defined virtual in InflationIndex
    Date today = Settings::instance().evaluationDate();
    Date todayMinusLag = today - availabilityLag_;
    std::pair<Date, Date> lim = inflationPeriod(todayMinusLag, frequency_);
    Date lastFix = lim.first - 1;

    Date flatMustForecastOn = lastFix + 1;
    Date interpMustForecastOn = lastFix + 1 - Period(frequency_);

    if (interpolated() && fixingDate >= interpMustForecastOn) {
        return forecastFixing(fixingDate);
    }

    if (!interpolated() && fixingDate >= flatMustForecastOn) {
        return forecastFixing(fixingDate);
    }

    // historical fixing
    return YoYInflationIndex::fixing(fixingDate);
}

Real YoYInflationIndexWrapper::forecastFixing(const Date& fixingDate) const {
    if (!yoyInflationTermStructure().empty())
        return YoYInflationIndex::fixing(fixingDate);
    Real f1 = zeroIndex_->fixing(fixingDate);
    Real f0 = zeroIndex_->fixing(fixingDate - 1 * Years); // FIXME convention ?
    return (f1 - f0) / f0;
}

void YoYInflationCouponPricer2::initialize(const InflationCoupon& coupon) {
    // duplicated logic from YoYInflationCouponPricer
    coupon_ = dynamic_cast<const YoYInflationCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "year-on-year inflation coupon needed");
    gearing_ = coupon_->gearing();
    spread_ = coupon_->spread();
    paymentDate_ = coupon_->date();

    // this is different from QuantLib::YoYInflationCouponPricer
    rateCurve_ = nominalTs_;

    // past or future fixing is managed in YoYInflationIndex::fixing()
    // use yield curve from index (which sets discount)

    discount_ = 1.0;
    if (paymentDate_ > rateCurve_->referenceDate())
        discount_ = rateCurve_->discount(paymentDate_);

    spreadLegValue_ = spread_ * coupon_->accrualPeriod() * discount_;
}

} // namespace QuantExt
