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

#include <qle/indexes/intradaypowerindex.hpp>

#include <ql/indexes/indexmanager.hpp>
#include <ql/settings.hpp>

namespace QuantExt {
std::string bucketName(const std::string& name, int start, int end, bool isDstHour) {
    std::ostringstream o;
    o << name << "-" << start << "-" << end;
    if (isDstHour)
        o << "-DST";
    return o.str();
}

IntradayPowerIndex::IntradayPowerIndex(const std::string& underlyingName, const QuantLib::Date& deliveryDate,
                                       const Calendar& fixingCalendar,
                                       const Handle<QuantExt::IntradayPowerPriceTermStructure>& priceCurve,
                                       const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& loadProfile)
    : deliveryDate_(deliveryDate), fixingCalendar_(fixingCalendar), intradayCurve_(priceCurve),
      loadProfile_(loadProfile) {
    std::ostringstream o;
    o << "POWER-" << underlyingName << "-" << QuantLib::io::iso_date(deliveryDate_);
    name_ = o.str();

    registerWith(intradayCurve_);
    registerWith(Settings::instance().evaluationDate());

    for (const auto& [start, end, load] : loadProfile_->loadProfile()) {
        std::string name = bucketName(name_, start, end, false);
        QL_DEPRECATED_DISABLE_WARNING
        IndexManager::instance().notifier(name);
        QL_DEPRECATED_ENABLE_WARNING
    }
    for (const auto& [start, end, load] : loadProfile_->loadProfileDST()) {
        std::string name = bucketName(name_, start, end, true);
        QL_DEPRECATED_DISABLE_WARNING
        IndexManager::instance().notifier(name);
        QL_DEPRECATED_ENABLE_WARNING
    }
}

IntradayPowerIndex::IntradayPowerIndex(const std::string& underlyingName, const QuantLib::Date& deliveryDate,
                                       int deliveryStart, int deliveryEnd, bool isDstHour,
                                       const Calendar& fixingCalendar,
                                       const Handle<QuantExt::IntradayPowerPriceTermStructure>& priceCurve)
    : deliveryDate_(deliveryDate), fixingCalendar_(fixingCalendar), intradayCurve_(priceCurve) {
    QL_REQUIRE(deliveryStart >= 0, "deliveryStart must be >= 0, got " << deliveryStart);
    QL_REQUIRE(deliveryEnd > deliveryStart,
               "deliveryEnd must be > deliveryStart, got " << deliveryEnd << " <= " << deliveryStart);
    QL_REQUIRE(deliveryEnd <= 24 * 3600, "deliveryEnd must be <= 24h in seconds, got " << deliveryEnd);
    QL_REQUIRE(!isDstHour || (deliveryStart >= 2 * 3600 && deliveryEnd <= 3 * 3600),
               "DST hour must be between 2am and 3am, got " << deliveryStart << "-" << deliveryEnd);
    std::ostringstream o;
    o << "POWER-" << underlyingName << "-" << QuantLib::io::iso_date(deliveryDate_);
    name_ = bucketName(o.str(), deliveryStart, deliveryEnd, isDstHour);
    deliveryTime_ = std::make_tuple(deliveryStart, deliveryEnd, isDstHour);
}

Real IntradayPowerIndex::forecastFixing(const Date& fixingDate) const {
    QL_REQUIRE(!intradayCurve_.empty(), "Intraday curve not provided for forecast fixing");
    if (deliveryTime_.has_value()) {
        auto [start, end, isDstHour] = *deliveryTime_;
        auto load =
            QuantLib::ext::make_shared<IntradayLoadProfile>(isDstHour ? LoadFactors{} : LoadFactors{{start, end, 1.0}},
                                                            isDstHour ? LoadFactors{{start, end, 1.0}} : LoadFactors{});
        return intradayCurve_->price(fixingDate, load);
    }
    return intradayCurve_->price(fixingDate, loadProfile_);
}

Real IntradayPowerIndex::intradayBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour) const {
    QL_REQUIRE(start >= 0, "start must be >= 0, got " << start);
    QL_REQUIRE(end > start, "end must be > start, got " << end << " <= " << start);
    QL_REQUIRE(end <= 24 * 3600, "end must be <= 24h in seconds, got " << end);
    QL_REQUIRE(!isDstHour || (start >= 2 * 3600 && end <= 3 * 3600),
               "DST hour must be between 2am and 3am, got " << start << "-" << end);

    std::string bucket = bucketName(name_, start, end, isDstHour);

    QL_DEPRECATED_DISABLE_WARNING
    const auto& history = IndexManager::instance().getHistory(bucket);
    QL_DEPRECATED_ENABLE_WARNING

    const Date today = Settings::instance().evaluationDate();
    Real histFixing = history[fixingDate];

    if (fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
        QL_REQUIRE(histFixing != Null<Real>(), "Missing " << bucket << " fixing for " << fixingDate);
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
        histFixing = Index::pastFixing(fixingDate);
    }

    QL_REQUIRE(histFixing != Null<Real>(),
               "Missing " << bucket << " fixing for " << fixingDate
                          << " and no intraday curve provided or day average price fixing available");
    return histFixing;
}

Real IntradayPowerIndex::pastFixing(const Date& fixingDate) const {

    if (loadProfile_ == nullptr || (loadProfile_->loadProfile().empty() && loadProfile_->loadProfileDST().empty())) {
        // No load profile provided, assume constant load and use the day average price as the fixing
        return Index::pastFixing(fixingDate);
    } else {
        // Assume right now, that the prices can be observed at the same granularity as the load profile,
        // future improvement, define a granularity and use it to fetch the price for each time bucket
        auto amount = 0.0;
        auto totalLoad = 0.0;
        for (const auto& [start, end, load] : loadProfile_->loadProfile()) {
            totalLoad += load * (end - start) / 3600.0;
            ;
            if (start == 0 && end == 24 * 3600) {
                // constant load special case, use the day average price fixing
                amount += load * (end - start) / 3600.0 * Index::pastFixing(fixingDate);
            } else {
                amount += load * (end - start) / 3600.0 * intradayBucketFixing(fixingDate, start, end, false);
            }
        }
        for (const auto& [start, end, load] : loadProfile_->loadProfileDST()) {
            totalLoad += load * (end - start) / 3600.0;
            amount += load * (end - start) / 3600.0 * intradayBucketFixing(fixingDate, start, end, true);
        }
        return totalLoad > 0.0 ? amount / totalLoad : Index::pastFixing(fixingDate);
    }
}

Real IntradayPowerIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate),
               "Intraday power index " << name() << ": fixing date " << io::iso_date(fixingDate) << " is not valid");
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(deliveryDate_ == Date() || fixingDate <= deliveryDate_,
               "Intraday power index " << name() << ": fixing requested on fixing date (" << io::iso_date(fixingDate)
                                       << ") that is past the delivery date (" << io::iso_date(deliveryDate_)
                                       << "). Eval date is " << today);

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

const std::vector<std::string> IntradayPowerIndex::intraDayIndexNames() const {
    std::vector<std::string> names;
    if (loadProfile_ != nullptr) {
        for (const auto& [start, end, load] : loadProfile_->loadProfile()) {
            names.push_back(bucketName(name_, start, end, false));
        }
        for (const auto& [start, end, load] : loadProfile_->loadProfileDST()) {
            names.push_back(bucketName(name_, start, end, true));
        }
    } else {
        names.push_back(name_);
    }
    return names;
}

} // namespace QuantExt
