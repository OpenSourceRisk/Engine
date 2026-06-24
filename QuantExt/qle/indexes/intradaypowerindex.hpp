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

/*! \file qle/indexes/commodityintradaypowerindex.hpp
    \brief commodity power index class, with intraday load profile adjustment

    \ingroup indexes
*/

#pragma once

#include <qle/indexes/commodityindex.hpp>
#include <qle/termstructures/intradaypowerloadtermstructure.hpp>
#include <qle/termstructures/intradaypowerpricetermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Intraday PowerIndex
/*! This index can represent intraday power prices


    \ingroup indexes
*/
class IntradayPowerIndex : public Index {
public:
    /*! spot quote is interpreted as of today */
    IntradayPowerIndex(const std::string& underlyingName, const QuantLib::Date& deliveryDate,
                       const Calendar& fixingCalendar,
                       const Handle<QuantExt::IntradayPowerPriceTermStructure>& priceCurve =
                           Handle<QuantExt::IntradayPowerPriceTermStructure>(),
                       const QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadProfile>& loadProfile = nullptr);

    //! Constructor used for a single time bucket
    IntradayPowerIndex(const std::string& underlyingName, const QuantLib::Date& deliveryDate, int deliveryStart,
                       int deliveryEnd, bool isDstHour, const Calendar& fixingCalendar,
                       const Handle<QuantExt::IntradayPowerPriceTermStructure>& priceCurve =
                           Handle<QuantExt::IntradayPowerPriceTermStructure>());

    std::string name() const override { return name_; }
    Calendar fixingCalendar() const override { return fixingCalendar_; }
    bool isValidFixingDate(const Date& fixingDate) const override { return fixingCalendar_.isBusinessDay(fixingDate); }

    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;

    const Handle<QuantExt::IntradayPowerPriceTermStructure>& priceCurve() const { return intradayCurve_; }

    const QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadProfile>& loadProfile() const { return loadProfile_; }

    const QuantLib::Date& deliveryDate() const { return deliveryDate_; }

    const std::vector<std::string> intraDayIndexNames() const;

    QuantLib::ext::shared_ptr<IntradayPowerIndex> clone(const QuantLib::Date& deliveryDate, ext::shared_ptr<QuantExt::IntradayPowerLoadProfile> loadProfile) const;

    Real pastFixing(const Date& fixingDate) const override;


    Real totalLoadMWh() const { return totalLoad_; }
private:
    Real forecastFixing(const Date& fixingDate) const;

    Real forecastBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour) const;

    //! Compute the fixing for a single intraday time bucket, falling back to a forecast if no past fixing is available.
    Real pastBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour, bool enforceTodaysFixing) const;

    std::string underlyingName_;
    std::string name_;
    QuantLib::Date deliveryDate_;
    std::optional<std::tuple<int, int, bool>> deliveryTime_ = std::nullopt;
    Calendar fixingCalendar_;

    Real pastIntradayFixing(const Date& fixingDate, int start, int end, bool isDstHour) const;
    Handle<QuantExt::IntradayPowerPriceTermStructure> intradayCurve_;
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadProfile> loadProfile_;
    QuantLib::Real totalLoad_ = 0.0;
};

} // namespace QuantExt
