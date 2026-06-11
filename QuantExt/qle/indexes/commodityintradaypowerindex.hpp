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
class CommodityIntradayPowerIndex : public CommodityIndex {
public:
    /*! spot quote is interpreted as of today */
    CommodityIntradayPowerIndex(
        const std::string& underlyingName, const QuantLib::Date& expiryDate, const Calendar& fixingCalendar,
        const Handle<QuantExt::IntradayPriceTermStructure>& priceCurve = Handle<QuantExt::IntradayPriceTermStructure>(),
        const QuantLib::Date& optionExpiryDate = QuantLib::Date(),
        const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& loadProfile = nullptr);

    Real forecastFixing(const Date& fixingDate) const override;

    Real forecastFixing(const Time& fixingTime) const override;

    Real pastFixing(const Date& fixingDate) const override;

    QuantLib::ext::shared_ptr<CommodityIndex>
    clone(const QuantLib::Date& expiryDate = QuantLib::Date(), const Date& optionExpiryDate = QuantLib::Date(),
          const QuantLib::ext::optional<QuantLib::Handle<PriceTermStructure>>& ts =
              QuantLib::ext::nullopt) const override;

protected:
    Handle<QuantExt::IntradayPriceTermStructure> intradayCurve_;
    QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile> loadProfile_;
    // Shared initialisation

private:
    Real intradayBucketFixing(const Date& fixingDate, int start, int end, bool isDstHour) const;
};

} // namespace QuantExt
