/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/commodityforward.hpp
    \brief Commodity forward representation

    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Commodity forward contract
//! \ingroup tradedata
class CommodityForward : public Trade {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityForward();

    //! Detailed constructor with explicit future expiry date.
    CommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike);

    //! Detailed constructor with explicit future expiry date.
    CommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike, const QuantLib::Date& futureExpiryDate,
                     const boost::optional<bool>& physicallySettled = true,
                     const Date& paymentDate = Date());

    //! Detailed constructor with explicit future expiry offset and calendar.
    CommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike, const QuantLib::Period& futureExpiryOffset,
                     const QuantLib::Calendar& offsetCalendar,
                     const boost::optional<bool>& physicallySettled = true,
                     const Date& paymentDate = Date());
    //@}

    //! \name Inspectors
    //@{
    std::string position() { return position_; }
    std::string commodityName() { return commodityName_; }
    std::string currency() { return currency_; }
    QuantLib::Real quantity() { return quantity_; }
    std::string maturityDate() { return maturityDate_; }
    QuantLib::Real strike() { return strike_; }
    const boost::optional<bool>& isFuturePrice() const { return isFuturePrice_; }
    const QuantLib::Date& futureExpiryDate() const { return futureExpiryDate_; }
    const QuantLib::Period& futureExpiryOffset() const { return futureExpiryOffset_; }
    const QuantLib::Calendar& offsetCalendar() const { return offsetCalendar_; }
    const boost::optional<bool>& physicallySettled() const { return physicallySettled_; }
    const QuantLib::Date& paymentDate() const { return paymentDate_; }
    //@}

    //! \name Trade interface
    //@{
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;

    //! Add underlying Commodity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::string position_;
    std::string commodityName_;
    std::string currency_;
    QuantLib::Real quantity_;
    std::string maturityDate_;
    QuantLib::Real strike_;

    /*! Indicates if the forward underlying is a commodity future settlement price, \c true, or a spot price \c false.
        If not explicitly set, it is assumed to be \c false.
    */
    boost::optional<bool> isFuturePrice_;

    /*! An explicit expiry date for the underlying future contract. This can be used if the trade references a
        future contract settlement price and the forward's maturity date does not match the future contract expiry 
        date.
    */
    QuantLib::Date futureExpiryDate_;

    //! Future expiry offset and calendar
    QuantLib::Period futureExpiryOffset_;
    QuantLib::Calendar offsetCalendar_;

    boost::optional<bool> physicallySettled_;
    QuantLib::Date paymentDate_;

    //! NDF currency, index and fixing date
    QuantLib::Date fixingDate_;
    std::string fxIndex_;
    std::string payCcy_;

};
} // namespace data
} // namespace ore
