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

/*! \file qle/indexes/commodityindex.hpp
    \brief commodity index class for holding commodity spot and futures price histories and forwarding.
    \ingroup indexes
*/

#ifndef quantext_commodityindex_hpp
#define quantext_commodityindex_hpp

#include <ql/currency.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <qle/indexes/eqfxindexbase.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Index
/*! This index can represent both spot and futures prices.
    In the latter case the constructor needs to be called with the futures expiry date.
    If the expiry date is set to Date(), the index is interpreted as spot index.

    If it is spot index, the index name() is set to the underlying name passed to the constructor
    prefixed by "COMM-".

    If it is a futures index and \c keepDays_ is \c false, we set the name() to 
    "COMM-" + underlyingName + "-" + "yyyy-mm", where "yyyy" is the expiry date's year and "mm" is the expiry date's 
    month. The index forecast for fixing Date yields the price curve's forecast to the futures expiry instead which 
    is beyond the fixing date. If \c keepDays_ is \c true, the date suffix in the name is "yyyy-mm-dd" i.e. we keep 
    the full date. This is useful for commodities whose expiry cycle is less than one month e.g. daily.

    \ingroup indexes
*/
class CommodityIndex : public EqFxIndexBase {
public:
    /*! spot quote is interpreted as of today */
    CommodityIndex(const std::string& underlyingName, const QuantLib::Date& expiryDate, const Calendar& fixingCalendar,
                   const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());
    CommodityIndex(const std::string& underlyingName, const QuantLib::Date& expiryDate, const Calendar& fixingCalendar,
        bool keepDays, const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());
    //! \name Index interface
    //@{
    std::string name() const override { return name_; }
    Calendar fixingCalendar() const override { return fixingCalendar_; }
    bool isValidFixingDate(const Date& fixingDate) const override { return fixingCalendar_.isBusinessDay(fixingDate); }
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
    //@}
    //! \name Observer interface
    //@{
    void update() override { notifyObservers(); }
    //@}
    //! \name Inspectors
    //@{
    std::string underlyingName() const { return underlyingName_; }
    const Handle<QuantExt::PriceTermStructure>& priceCurve() const { return curve_; }
    bool isFuturesIndex() const { return isFuturesIndex_; }
    const QuantLib::Date& expiryDate() const { return expiryDate_; }
    bool keepDays() const { return keepDays_; }
    //@}
    //! \name Fixing calculations
    //@{
    virtual Real forecastFixing(const Date& fixingDate) const;
    virtual Real forecastFixing(const Time& fixingTime) const override;
    virtual Real pastFixing(const Date& fixingDate) const override;
    // @}

    /*! Returns a copy of itself with a potentially different expiry date and pricing curve.

        If the \c expiryDate is an empty \c Date, it is ignored and the CommodityIndex instance's expiry 
        date is used. If the \c ts is uninitialised, it is ignored and the CommodityIndex instance's price 
        curve is used.
    */
    virtual QuantLib::ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
        const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const = 0;

protected:
    std::string underlyingName_;
    Date expiryDate_;
    Calendar fixingCalendar_;
    Handle<QuantExt::PriceTermStructure> curve_;
    std::string name_;
    bool isFuturesIndex_;
    // Belongs in CommodityFuturesIndex but have put everything else in base class.
    bool keepDays_;

    // Shared initialisation
    void init();
};

class CommoditySpotIndex : public CommodityIndex {
public:
    /*! spot quote is interpreted as of today */
    CommoditySpotIndex(const std::string& underlyingName, const Calendar& fixingCalendar,
                       const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>())
        : CommodityIndex(underlyingName, Date(), fixingCalendar, priceCurve) {
        QL_REQUIRE(expiryDate_ == Date(), "empty expiry date expected in CommoditySpotIndex");
    }

    //! Implement the base clone. The \c expiryDate is ignored for a CommoditySpotIndex.
    QuantLib::ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
        const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const override;
};

class CommodityFuturesIndex : public CommodityIndex {
public:
    CommodityFuturesIndex(
        const std::string& underlyingName, const Date& expiryDate, const Calendar& fixingCalendar,
        const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>())
        : CommodityIndex(underlyingName, expiryDate, fixingCalendar, priceCurve) {
        QL_REQUIRE(expiryDate_ != Date(), "non-empty expiry date expected CommodityFuturesIndex");
    }

    CommodityFuturesIndex(
        const std::string& underlyingName, const Date& expiryDate, const Calendar& fixingCalendar, bool keepDays,
        const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>())
        : CommodityIndex(underlyingName, expiryDate, fixingCalendar, keepDays, priceCurve) {
        QL_REQUIRE(expiryDate_ != Date(), "non-empty expiry date expected CommodityFuturesIndex");
    }

    //! Implement the base clone.
    QuantLib::ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
        const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const override;
};

} // namespace QuantExt

#endif
