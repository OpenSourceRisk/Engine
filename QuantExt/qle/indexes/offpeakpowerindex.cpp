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

#include <qle/indexes/offpeakpowerindex.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Calendar;
using QuantLib::NullCalendar;
using std::string;

namespace QuantExt {

OffPeakPowerIndex::OffPeakPowerIndex(const string& underlyingName,
    const Date& expiryDate,
    const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& offPeakIndex,
    const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& peakIndex,
    Real offPeakHours,
    const Calendar& peakCalendar,
    const Handle<PriceTermStructure>& priceCurve)
    : CommodityFuturesIndex(underlyingName, expiryDate, NullCalendar(), true, priceCurve),
      offPeakIndex_(offPeakIndex), peakIndex_(peakIndex), offPeakHours_(offPeakHours),
      peakCalendar_(peakCalendar) {
    string msgPrefix = "Constructing " + underlyingName + ": ";
    QL_REQUIRE(0.0 < offPeakHours_ && offPeakHours_ < 24.0, msgPrefix << "off-peak hours must be in (0, 24.0)");
    QL_REQUIRE(expiryDate_ == offPeakIndex_->expiryDate(), msgPrefix << "the expiry date (" <<
        io::iso_date(expiryDate_) << ") should equal the off-peak index expiry date (" <<
        io::iso_date(offPeakIndex_->expiryDate()) << ").");
    QL_REQUIRE(expiryDate_ == peakIndex_->expiryDate(), msgPrefix << "the expiry date (" <<
        io::iso_date(expiryDate_) << ") should equal the peak index expiry date (" <<
        io::iso_date(peakIndex_->expiryDate()) << ").");
    QL_REQUIRE(offPeakIndex_, msgPrefix << "the off-peak index should not be null.");
    QL_REQUIRE(peakIndex_, msgPrefix << "the peak index should not be null.");
}

const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& OffPeakPowerIndex::offPeakIndex() const {
    return offPeakIndex_;
}

const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& OffPeakPowerIndex::peakIndex() const {
    return peakIndex_;
}

Real OffPeakPowerIndex::offPeakHours() const {
    return offPeakHours_;
}

const Calendar& OffPeakPowerIndex::peakCalendar() const {
    return peakCalendar_;
}

QuantLib::ext::shared_ptr<CommodityIndex> OffPeakPowerIndex::clone(const Date& expiry,
    const boost::optional<Handle<PriceTermStructure>>& ts) const {
    const auto& pts = ts ? *ts : priceCurve();
    const auto& ed = expiry == Date() ? expiryDate() : expiry;
    auto offPeakIndex = QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(offPeakIndex_->clone(ed));
    auto peakIndex = QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(peakIndex_->clone(ed));
    return QuantLib::ext::make_shared<OffPeakPowerIndex>(underlyingName(), ed, offPeakIndex,
        peakIndex, offPeakHours_, peakCalendar_, pts);
}

Real OffPeakPowerIndex::pastFixing(const Date& fixingDate) const {
    if (peakCalendar_.isBusinessDay(fixingDate))
        return offPeakIndex_->fixing(fixingDate);
    else
        return (offPeakHours_ * offPeakIndex_->fixing(fixingDate) +
            (24.0 - offPeakHours_) * peakIndex_->fixing(fixingDate)) / 24.0;
}

}
