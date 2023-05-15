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
                                     const boost::shared_ptr<FutureExpiryCalculator>& basisFec,
                                     const QuantLib::Handle<PriceTermStructure>& basePts,
                                     const boost::shared_ptr<CommodityIndex>& baseIndex,
                                     const boost::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                     QuantLib::Size monthOffset = 0, bool baseIsAveraging = false)
        : PriceTermStructure(referenceDate, cal, dc), basisFec_(basisFec), basePts_(basePts),
          baseIndex_(baseIndex), baseFec_(baseFec), addBasis_(addBasis), monthOffset_(monthOffset),
          baseIsAveraging_(baseIsAveraging) {
        registerWith(basePts_);
        registerWith(baseIndex_);
    }

    CommodityBasisPriceTermStructure(const QuantLib::Date& referenceDate,
                                     const boost::shared_ptr<FutureExpiryCalculator>& basisFec,
                                     const QuantLib::Handle<PriceTermStructure>& basePts,
                                     const boost::shared_ptr<CommodityIndex>& baseIndex,
                                     const boost::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                     QuantLib::Size monthOffset = 0, bool baseIsAveraging = false)
        : PriceTermStructure(referenceDate, QuantLib::NullCalendar(), basePts->dayCounter()), basisFec_(basisFec),
          basePts_(basePts), baseIndex_(baseIndex), baseFec_(baseFec), addBasis_(addBasis),
          monthOffset_(monthOffset), baseIsAveraging_(baseIsAveraging) {
        registerWith(basePts_);
        registerWith(baseIndex_);
    }


    //! Inspectors
    const boost::shared_ptr<FutureExpiryCalculator>& basisFutureExpiryCalculator() const { return basisFec_; }
    const boost::shared_ptr<CommodityIndex>& baseIndex() const { return baseIndex_; }
    const boost::shared_ptr<FutureExpiryCalculator>& baseFutureExpiryCalculator() const { return baseFec_; }
    bool addBasis() const { return addBasis_; }
    bool baseIsAveraging() const { return baseIsAveraging_; }
    QuantLib::Size monthOffset() const { return monthOffset_; }

protected:
    boost::shared_ptr<FutureExpiryCalculator> basisFec_;

    QuantLib::Handle<PriceTermStructure> basePts_;
    boost::shared_ptr<CommodityIndex> baseIndex_;
    boost::shared_ptr<FutureExpiryCalculator> baseFec_;

    bool addBasis_;
    QuantLib::Size monthOffset_;
    bool baseIsAveraging_;
};


class FlatCommodityBasisFutureCurve : public CommodityBasisPriceTermStructure, public QuantLib::LazyObject {
public:
    //! \name Constructors
    //@{
    //! Curve constructed from dates and quotes
    FlatCommodityBasisFutureCurve(const QuantLib::Date& referenceDate, const double price,
                                  const boost::shared_ptr<FutureExpiryCalculator>& basisFec,
                                  const boost::shared_ptr<CommodityIndex>& baseIndex,
                                  const QuantLib::Handle<PriceTermStructure>& basePts,
                                  const boost::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                  QuantLib::Size monthOffset = 0, bool baseIsAverging = false)
        : CommodityBasisPriceTermStructure(referenceDate, basisFec, basePts, baseIndex, baseFec, addBasis, monthOffset, baseIsAverging),
          price_(price) {}
    //@}

    //! \name Observer interface
    //@{
    void update() override { QuantLib::LazyObject::update(); }
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const override{};
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override { return QuantLib::Date::maxDate(); }
    QuantLib::Time maxTime() const override { return timeFromReference(maxDate()); }
    //@}

    //! \name PriceTermStructure interface
    //@{
    QuantLib::Time minTime() const override { return 0.0; }
    std::vector<QuantLib::Date> pillarDates() const override {
        return {referenceDate() + 1 * Days, referenceDate() + 1 * Years};
    }
    const QuantLib::Currency& currency() const override { return basePts_->currency(); }
    //@}

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Time>& times() const { return {1. / 365., 1.0}; }

    const std::vector<QuantLib::Real>& prices() const { return {price_, price_}; }
    //@}

protected:
    //! \name PriceTermStructure implementation
    //@{
    QuantLib::Real priceImpl(QuantLib::Time t) const override { return price_; }
    //@}

private:
    double price_;
};


} // namespace QuantExt