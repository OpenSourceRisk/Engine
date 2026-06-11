/*
 Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file qle/indexes/commodityintradaypowerindex.cpp
    \brief commodity power index class, with intraday load profile adjustment

    \ingroup indexes
*/

#include <qle/indexes/commodityintradaypowerindex.hpp>

#include <ql/indexes/indexmanager.hpp>
#include <ql/settings.hpp>

namespace QuantExt {

CommodityIntradayPowerIndex::CommodityIntradayPowerIndex(
    const std::string& underlyingName, const QuantLib::Date& expiryDate, const Calendar& fixingCalendar,
    const Handle<QuantExt::IntradayPriceTermStructure>& priceCurve, const QuantLib::Date& optionExpiryDate,
    const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& loadProfile)
    : CommodityIndex(underlyingName, expiryDate, fixingCalendar, true,
                     Handle<QuantExt::PriceTermStructure>(
                         QuantLib::ext::dynamic_pointer_cast<QuantExt::PriceTermStructure>(priceCurve.currentLink())),
                     optionExpiryDate),
      intradayCurve_(priceCurve), loadProfile_(loadProfile) {}

Real CommodityIntradayPowerIndex::forecastFixing(const Date& fixingDate) const {
    return !intradayCurve_.empty() ? intradayCurve_->price(fixingDate, loadProfile_)
                                   : CommodityIndex::forecastFixing(fixingDate);
}

Real CommodityIntradayPowerIndex::forecastFixing(const Time& fixingTime) const {
    return !intradayCurve_.empty() ? intradayCurve_->price(fixingTime, loadProfile_)
                                   : CommodityIndex::forecastFixing(fixingTime);
}

Real CommodityIntradayPowerIndex::intradayBucketFixing(const Date& fixingDate, int start, int end,
                                                       bool isDstHour) const {
    QL_REQUIRE(start >= 0, "start must be >= 0, got " << start);
    QL_REQUIRE(end > start, "end must be > start, got " << end << " <= " << start);
    QL_REQUIRE(end <= 24 * 3600, "end must be <= 24h in seconds, got " << end);
    QL_REQUIRE(!isDstHour || (start >= 2 * 3600 && end <= 3 * 3600),
               "DST hour must be between 2am and 3am, got " << start << "-" << end);

    std::string bucketName = name() + "-" + std::to_string(start) + "-" + std::to_string(end);
    if (isDstHour)
        bucketName += "-DST";

    QL_DEPRECATED_DISABLE_WARNING
    const auto& history = IndexManager::instance().getHistory(bucketName);
    QL_DEPRECATED_ENABLE_WARNING

    const Date today = Settings::instance().evaluationDate();
    Real histFixing = history[fixingDate];

    if (fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
        QL_REQUIRE(histFixing != Null<Real>(), "Missing " << bucketName << " fixing for " << fixingDate);
        return histFixing;
    }

    if (fixingDate == today && histFixing == Null<Real>() && !intradayCurve_.empty()) {
        LoadFactors load;
        LoadFactors loadDst;
        if (isDstHour)
            loadDst.emplace_back(start, end, 1.0);
        else
            load.emplace_back(start, end, 1.0);
        auto lp = QuantLib::ext::make_shared<IntradayLoadProfile>(load, loadDst);
        return intradayCurve_->price(fixingDate, lp);
    }

    // Fallback to use day average price if intraday price not available
    if (histFixing == Null<Real>()) {
        histFixing = CommodityIndex::pastFixing(fixingDate);
    }

    QL_REQUIRE(histFixing != Null<Real>(),
               "Missing " << bucketName << " fixing for " << fixingDate
                          << " and no intraday curve provided or day average price fixing available");
    return histFixing;
}

Real CommodityIntradayPowerIndex::pastFixing(const Date& fixingDate) const {
    if (loadProfile_ == nullptr || (loadProfile_->loadProfile().empty() && loadProfile_->loadProfileDST().empty()))
        return CommodityIndex::pastFixing(fixingDate);
    else {
        // Assume right now, that the prices can be observed at the same granularity as the load profile,
        // future improvement, define a granularity and use it to fetch the price for each time bucket
        auto amount = 0.0;
        auto totalLoad = 0.0;
        for (const auto& [start, end, load] : loadProfile_->loadProfile()) {
            totalLoad += load * (end - start) / 3600.0;;
            if (start == 0 && end == 24 * 3600) {
                amount += load * (end - start) / 3600.0 * CommodityIndex::pastFixing(fixingDate);
            } else {
                amount += load * (end - start) / 3600.0 * intradayBucketFixing(fixingDate, start, end, false);
            }
        }
        for (const auto& [start, end, load] : loadProfile_->loadProfileDST()) {
            totalLoad += load * (end - start) / 3600.0;
            amount += load * (end - start) / 3600.0 * intradayBucketFixing(fixingDate, start, end, true) ;
        }
        return totalLoad > 0.0 ? amount / totalLoad : CommodityIndex::pastFixing(fixingDate);
    }
}

QuantLib::ext::shared_ptr<CommodityIndex>
CommodityIntradayPowerIndex::clone(const QuantLib::Date& expiryDate, const Date& optionExpiryDate,
                                   const QuantLib::ext::optional<QuantLib::Handle<PriceTermStructure>>& ts) const {
    const auto& pts = ts ? *ts : priceCurve();
    const auto& ed = expiryDate == Date() ? this->expiryDate() : expiryDate;
    const auto& oed = optionExpiryDate == Date() ? this->optionExpiryDate() : optionExpiryDate;
    auto intradayPts = QuantLib::ext::dynamic_pointer_cast<IntradayPriceTermStructure>(pts.currentLink());
    QL_REQUIRE(intradayPts, "CommodityIntradayPowerIndex::clone requires an IntradayPriceTermStructure");
    return QuantLib::ext::make_shared<CommodityIntradayPowerIndex>(underlyingName(), ed, fixingCalendar(),
                                                                    Handle<IntradayPriceTermStructure>(intradayPts),
                                                                    oed, loadProfile_);
}

} // namespace QuantExt
