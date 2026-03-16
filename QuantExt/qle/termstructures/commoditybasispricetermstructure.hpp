/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/commoditybasispricetermstructure.hpp
    \brief An interface for a commodity price curve created from a base price curve and a collection of basis quotes
    \ingroup termstructures
*/

#pragma once

#include <map>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

class CommodityBasisPriceTermStructure : public PriceTermStructure {
public:
    CommodityBasisPriceTermStructure(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                                     const QuantLib::DayCounter& dc,
                                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec,
                                     const QuantLib::ext::shared_ptr<CommodityIndex>& baseIndex,
                                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                     QuantLib::Size monthOffset = 0, bool averagingBaseCashflow = false,
                                     bool priceAsHistoricalFixing = true)
        : PriceTermStructure(referenceDate, cal, dc), basisFec_(basisFec),
          baseIndex_(baseIndex), baseFec_(baseFec), addBasis_(addBasis), monthOffset_(monthOffset),
          averagingBaseCashflow_(averagingBaseCashflow), priceAsHistoricalFixing_(priceAsHistoricalFixing) {
        registerWith(baseIndex_);
    }

    CommodityBasisPriceTermStructure(const QuantLib::Date& referenceDate,
                                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec,
                                     const QuantLib::ext::shared_ptr<CommodityIndex>& baseIndex,
                                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                     QuantLib::Size monthOffset = 0, bool averagingBaseCashflow = false,
                                     bool priceAsHistoricalFixing = true)
        : PriceTermStructure(referenceDate, QuantLib::NullCalendar(), baseIndex->priceCurve()->dayCounter()), basisFec_(basisFec),
          baseIndex_(baseIndex), baseFec_(baseFec), addBasis_(addBasis), monthOffset_(monthOffset),
          averagingBaseCashflow_(averagingBaseCashflow), priceAsHistoricalFixing_(priceAsHistoricalFixing) {
        registerWith(baseIndex_);
    }


    //! Inspectors
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFutureExpiryCalculator() const { return basisFec_; }
    const QuantLib::ext::shared_ptr<CommodityIndex>& baseIndex() const { return baseIndex_; }
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFutureExpiryCalculator() const { return baseFec_; }
    bool addBasis() const { return addBasis_; }
    bool averagingBaseCashflow() const { return averagingBaseCashflow_; }
    bool priceAsHistoricalFixing() const { return priceAsHistoricalFixing_; }
    QuantLib::Size monthOffset() const { return monthOffset_; }

protected:
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> basisFec_;
    QuantLib::ext::shared_ptr<CommodityIndex> baseIndex_;
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> baseFec_;
    bool addBasis_;
    QuantLib::Size monthOffset_;
    bool averagingBaseCashflow_;
    bool priceAsHistoricalFixing_;
};
} // namespace QuantExt