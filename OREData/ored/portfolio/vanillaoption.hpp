/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/vanillaoption.hpp
    \brief Vanilla Option data model
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable Vanilla Option
/*!
  \ingroup tradedata
*/
class VanillaOptionTrade : public Trade {
public:
    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const string& asset() const { return assetName_; }
    const string& currency() const { return currency_; }
    double strike() const { return strike_; }
    double quantity() const { return quantity_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
protected:
    VanillaOptionTrade(AssetClass assetClassUnderlying)
        : Trade("VanillaOption"), assetClassUnderlying_(assetClassUnderlying), strike_(0), quantity_(0) {}
    VanillaOptionTrade(const Envelope& env, AssetClass assetClassUnderlying, OptionData option, string assetName,
                       string currency, double strike, double quantity,
                       const boost::shared_ptr<QuantLib::Index>& index = nullptr, const std::string& indexName = "")
        : Trade("VanillaOption", env), assetClassUnderlying_(assetClassUnderlying), option_(option),
          assetName_(assetName), currency_(currency), strike_(strike), quantity_(quantity), index_(index),
          indexName_(indexName) {}

    AssetClass assetClassUnderlying_;
    OptionData option_;
    string assetName_;
    string currency_;
    double strike_;
    double quantity_;

    //! An index is needed if the option is to be automatically exercised on expiry.
    boost::shared_ptr<QuantLib::Index> index_;

    //! Hold the external index name if needed e.g. in the case of an FX index.
    std::string indexName_;

    //! Store the option expiry date.
    QuantLib::Date expiryDate_;
};
} // namespace data
} // namespace ore
