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

/*! \file qle/indexes/equityindex.cpp
\brief equity index class for holding equity fixing histories and forwarding.
\ingroup indexes
*/

#include <qle/indexes/commodityindex.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

CommodityIndex::CommodityIndex(const std::string& underlyingName, const Date& expiryDate, const Currency& currency,
                               const Calendar& fixingCalendar, const Handle<QuantExt::PriceTermStructure>& curve,
                               const std::string& separator)
    : underlyingName_(underlyingName), expiryDate_(expiryDate), currency_(currency), fixingCalendar_(fixingCalendar),
      curve_(curve) {
    if (expiryDate_ == Date()) {
        // spot price index
        name_ = underlyingName_;
        isFuturesIndex_ = false;
    } else {
        // futures price index
        std::ostringstream o;
        o << underlyingName_ << separator << QuantLib::io::iso_date(expiryDate_);
        name_ = o.str();
        isFuturesIndex_ = true;
    }
    registerWith(curve_);
    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(name()));
}

Real CommodityIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid");
    QL_REQUIRE(expiryDate_ == Date() || fixingDate <= expiryDate_, "fixing past expiry requested");
    Date today = Settings::instance().evaluationDate();

    if (fixingDate > today || (fixingDate == today && forecastTodaysFixing))
        return forecastFixing(fixingDate);

    Real result = Null<Decimal>();

    if (fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
        // must have been fixed
        // do not catch exceptions
        result = pastFixing(fixingDate);
        QL_REQUIRE(result != Null<Real>(), "Missing " << name() << " fixing for " << fixingDate);
    } else {
        try {
            // might have been fixed
            result = pastFixing(fixingDate);
        } catch (Error&) {
            ; // fall through and forecast
        }
        if (result == Null<Real>())
            return forecastFixing(fixingDate);
    }

    return result;
}

Real CommodityIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}

Real CommodityIndex::forecastFixing(const Date& fixingDate) const {
    if (isFuturesIndex_)
        return curve_->price(expiryDate_);
    else
        return curve_->price(fixingDate);
}

} // namespace QuantExt
