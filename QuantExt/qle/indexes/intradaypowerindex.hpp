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
#include <qle/termstructures/intradayloadingtermstructure.hpp>
#include <qle/termstructures/intradaypricetermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Intraday PowerIndex
/*! This index can represent intraday power prices


    \ingroup indexes
*/
class IntradayPowerIndex : public Index {
public:
    /*! spot quote is interpreted as of today */
    IntradayPowerIndex(
        const std::string& underlyingName, const QuantLib::Date& deliveryDate, const Calendar& fixingCalendar,
        const Handle<QuantExt::IntradayPriceTermStructure>& priceCurve = Handle<QuantExt::IntradayPriceTermStructure>(),
        const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& loadProfile = nullptr);

    std::string name() const override { return name_; }
    Calendar fixingCalendar() const override { return fixingCalendar_; }
    bool isValidFixingDate(const Date& fixingDate) const override { return fixingCalendar_.isBusinessDay(fixingDate); }

    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;


    Real forecastFixing(const Date& fixingDate) const;

    Real pastFixing(const Date& fixingDate) const override;

    const Handle<QuantExt::IntradayPriceTermStructure>& priceCurve() const { return intradayCurve_; }

    const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& loadProfile() const { return loadProfile_; }

    const QuantLib::Date& deliveryDate() const { return deliveryDate_; }

private:
    std::string name_;
    QuantLib::Date deliveryDate_;
    Calendar fixingCalendar_;
    Real intradayBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour) const;
    Handle<QuantExt::IntradayPriceTermStructure> intradayCurve_;
    QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile> loadProfile_;
};

} // namespace QuantExt
