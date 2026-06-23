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
#include <qle/utilities/intradaypower.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/settings.hpp>

namespace QuantExt {
std::string bucketName(const std::string& name, int start, int end, bool isDstHour) {
    std::ostringstream o;
    if (start == 0 && end == 24 * 3600)
        return name;
    o << name << "-" << start << "-" << end;
    if (isDstHour)
        o << "-DST";
    return o.str();
}

IntradayPowerIndex::IntradayPowerIndex(const std::string& underlyingName, const QuantLib::Date& deliveryDate,
                                       const Calendar& fixingCalendar,
                                       const Handle<QuantExt::IntradayPowerPriceTermStructure>& priceCurve,
                                       const QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadProfile>& loadProfile)
    : underlyingName_(underlyingName), deliveryDate_(deliveryDate), fixingCalendar_(fixingCalendar),
      intradayCurve_(priceCurve) {
    std::ostringstream o;
    o << "POWER-" << underlyingName << "-" << QuantLib::io::iso_date(deliveryDate_);
    name_ = o.str();
    registerWith(intradayCurve_);
    registerWith(Settings::instance().evaluationDate());
    registerWith(notifier());

    if (loadProfile != nullptr) {
        loadProfile_ = QuantLib::ext::make_shared<IntradayPowerLoadProfileWithMWh>();
        loadProfile_->reserve(loadProfile->size());
        auto dstAdjustment =
            intradayCurve_.empty()
                ? QuantExt::IntradayPowerDSTAdjustment::NoAdjustment
                : QuantExt::dayTimeSavingsAdjustment(deliveryDate_, intradayCurve_->intradayShape()->daylightSavingsLocation());
        for (const auto& load : *loadProfile) {
            loadProfile_->push_back(dstAdjustedTotalLoad(load, dstAdjustment));
            totalLoad_ += loadProfile_->back().totalMWh;
            std::string name = bucketName(name_, load.startTime, load.endTime, load.isDSTextraHour);
            QL_DEPRECATED_DISABLE_WARNING
            registerWith(IndexManager::instance().notifier(name));
            QL_DEPRECATED_ENABLE_WARNING
        }
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
    QL_REQUIRE(deliveryEnd <= QuantExt::SECONDS_PER_DAY, "deliveryEnd must be <= 24h in seconds, got " << deliveryEnd);
    QL_REQUIRE(!isDstHour || (deliveryStart >= QuantExt::TWO_AM_IN_SECONDS && deliveryEnd <= QuantExt::THREE_AM_IN_SECONDS),
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
        return forecastBucketFixing(fixingDate, start, end, isDstHour);
    }
    return intradayCurve_->price(fixingDate, loadProfile_);
}

Real IntradayPowerIndex::pastIntradayFixing(const Date& fixingDate, int start, int end, bool isDstHour) const {
    QL_REQUIRE(start >= 0, "start must be >= 0, got " << start);
    QL_REQUIRE(end > start, "end must be > start, got " << end << " <= " << start);
    QL_REQUIRE(end <= QuantExt::SECONDS_PER_DAY, "end must be <= 24h in seconds, got " << end);
    QL_REQUIRE(!isDstHour || (start >= QuantExt::TWO_AM_IN_SECONDS && end <= QuantExt::THREE_AM_IN_SECONDS),
               "DST hour must be between 2am and 3am, got " << start << "-" << end);

    std::string bucket = bucketName(name_, start, end, isDstHour);

    QL_DEPRECATED_DISABLE_WARNING
    const auto& history = IndexManager::instance().getHistory(bucket);
    QL_DEPRECATED_ENABLE_WARNING

    Real histFixing = history[fixingDate];
    return histFixing;
}

Real IntradayPowerIndex::forecastBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour) const {
    QL_REQUIRE(!intradayCurve_.empty(), "Intraday curve not provided for forecast fixing");
    QL_REQUIRE(start >= 0, "start must be >= 0, got " << start);
    QL_REQUIRE(end > start, "end must be > start, got " << end << " <= " << start);
    QL_REQUIRE(end <= QuantExt::SECONDS_PER_DAY, "end must be <= 24h in seconds, got " << end);
    QL_REQUIRE(!isDstHour || (start >= QuantExt::TWO_AM_IN_SECONDS && end <= QuantExt::THREE_AM_IN_SECONDS),
               "DST hour must be between 2am and 3am, got " << start << "-" << end);
    return intradayCurve_->price(fixingDate, start, end, isDstHour, true);
}

Real IntradayPowerIndex::pastBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour,
                                      bool enforceTodaysFixing) const {
    auto fixing = (start == 0 && end == QuantExt::SECONDS_PER_DAY) ? Index::pastFixing(fixingDate)
                                                   : pastIntradayFixing(fixingDate, start, end, isDstHour);
    if (fixing == Null<Real>()) {
        QL_REQUIRE(!enforceTodaysFixing, "Missing " << name() << " fixing for " << fixingDate << " and time bucket "
                                                    << start << "-" << end);
        // if todays fixing is not available for this time slot, we fall back to forcast it
        fixing = forecastBucketFixing(fixingDate, start, end, isDstHour);
    }
    return fixing;
}


Real IntradayPowerIndex::pastFixing(const Date& fixingDate) const {
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(fixingDate <= today, "Intraday power index " << name() << ": past fixing requested for future date "
                                                            << io::iso_date(fixingDate) << ". Eval date is "
                                                            << io::iso_date(today));
    QL_REQUIRE(isValidFixingDate(fixingDate),
               "Intraday power index " << name() << ": fixing date " << io::iso_date(fixingDate) << " is not valid");
    QL_REQUIRE(fixingDate <= deliveryDate_ || deliveryDate_ == Date(),
               "Intraday power index " << name() << ": past fixing requested for fixing date ("
                                       << io::iso_date(fixingDate) << ") that is past the delivery date ("
                                       << io::iso_date(deliveryDate_) << "). Eval date is " << io::iso_date(today));
    
    bool enforceTodaysFixing = fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings();
    // No Load profile, assume constant load during the day and fetch the price for the whole day
    if (loadProfile_ == nullptr){
        auto fixing = Index::pastFixing(fixingDate);
        QL_REQUIRE(fixing != Null<Real>() || !enforceTodaysFixing,
                   "Missing " << name() << " fixing for " << fixingDate);
        return fixing == Null<Real>() ? forecastFixing(fixingDate) : fixing;
    }
    // Assume right now, that the prices can be observed at the same granularity as the load profile,
    // future improvement, define a granularity and use it to fetch the price for each time bucket
    auto amount = 0.0;
    for (const auto& [start, end, load, mwh, isDstHour] : *loadProfile_) {
        if (load == 0.0)
            continue;
        amount += mwh * pastBucketFixing(fixingDate, start, end, isDstHour, enforceTodaysFixing);
    }
    return (totalLoad_ > 0.0) ? amount / totalLoad_ : 0.0;
}

Real IntradayPowerIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate),
               "Intraday power index " << name() << ": fixing date " << io::iso_date(fixingDate) << " is not valid");
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(deliveryDate_ == Date() || fixingDate <= deliveryDate_,
               "Intraday power index " << name() << ": fixing requested on fixing date (" << io::iso_date(fixingDate)
                                       << ") that is past the delivery date (" << io::iso_date(deliveryDate_)
                                       << "). Eval date is " << today);

    if (loadProfile_ != nullptr) {
        // Do day fixings
    }

    if (fixingDate > today || (fixingDate == today && forecastTodaysFixing))
        return forecastFixing(deliveryDate_);

    // Handle past fixing with a load profile
    return pastFixing(fixingDate);

}

const std::vector<std::string> IntradayPowerIndex::intraDayIndexNames() const {
    std::set<std::string> names;
    if (loadProfile_ != nullptr) {
        for (const auto& [start, end, load, mwh, isDstHour] : *loadProfile_) {
            if (load > 0.0)
                names.insert(bucketName(name_, start, end, isDstHour));
        }
    } else {
        names.insert(name_);
    }
    return std::vector<std::string>(names.begin(), names.end());
}

QuantLib::ext::shared_ptr<IntradayPowerIndex>
IntradayPowerIndex::clone(const Date& deliveryDate, ext::shared_ptr<IntradayPowerLoadProfile> loadProfile) const {
    return QuantLib::ext::make_shared<IntradayPowerIndex>(underlyingName_, deliveryDate, fixingCalendar_,
                                                          intradayCurve_, loadProfile);
}

} // namespace QuantExt
