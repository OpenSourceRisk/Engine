/*
  Copyright (C) 2020 Skandinaviska Enskilda Banken AB (publ)
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

 /*! \file portfolio/commodityasianoption.hpp
     \brief Commodity Asian Option data model and serialization
     \ingroup tradedata
 */

 #pragma once

 #include <ored/portfolio/asianoption.hpp>

 namespace ore {
 namespace data {

 //! Serializable Commodity Asian Option
 /*!
   \ingroup tradedata
 */
 class CommodityAsianOption : public AsianOptionTrade {
 public:
     //! Default constructor
     CommodityAsianOption() : AsianOptionTrade(AssetClass::COM) { tradeType_ = "CommodityAsianOption"; }

     //! Detailed constructor
     CommodityAsianOption(const Envelope& env, const OptionData& optionData, const OptionAsianData& asianData,
                          ScheduleData scheduleData, const std::string& commodityName, const std::string& currency,
                          QuantLib::Real strike, QuantLib::Real quantity,
                          const boost::optional<bool>& isFuturePrice = boost::none,
                          const QuantLib::Date& futureExpiryDate = QuantLib::Date())
         : AsianOptionTrade(env, AssetClass::COM, optionData, asianData, scheduleData, commodityName, currency, strike,
                            quantity),
           isFuturePrice_(isFuturePrice), futureExpiryDate_(futureExpiryDate) {
         tradeType_ = "CommodityAsianOption";
     }

     //! Build underlying instrument and link pricing engine
     void build(const boost::shared_ptr<EngineFactory>&) override;

     //! Add underlying Commodity names
     std::map<AssetClass, std::set<std::string>> underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

     //! \name Inspectors
     //@{
     const boost::optional<bool>& isFuturePrice() const { return isFuturePrice_; }
     const QuantLib::Date& futureExpiryDate() const { return futureExpiryDate_; }
     //@}

     //! \name Serialisation
     //@{
     virtual void fromXML(XMLNode* node) override;
     virtual XMLNode* toXML(XMLDocument& doc) override;
     //@}

 private:
     /*! Indicates if the option underlying is a commodity future settlement price, \c true, or a spot price \c false.
         If not explicitly set, it is assumed to be \c true.
     */
     boost::optional<bool> isFuturePrice_;

     /*! An explicit expiry date for the underlying future contract. This can be used if the option trade references a
         future contract settlement price and the option's expiry date does not match the future contract expiry date.
     */
     QuantLib::Date futureExpiryDate_;

     CommodityUnderlying commodityUnderlying_;
};

 } // namespace data
 } // namespace ore
