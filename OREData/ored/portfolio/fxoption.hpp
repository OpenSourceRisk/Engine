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

/*! \file portfolio/fxoption.hpp
    \brief FX Option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/vanillaoption.hpp>

#include <ored/portfolio/tradefactory.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable FX Option
/*!
  \ingroup tradedata
*/
class FxOption : public VanillaOptionTrade {
public:
    //! Default constructor
    FxOption() : VanillaOptionTrade(AssetClass::FX) { tradeType_ = "FxOption"; }
    //! Constructor
    FxOption(const Envelope& env, const OptionData& option, const string& boughtCurrency, double boughtAmount,
             const string& soldCurrency, double soldAmount, const std::string& fxIndex = "")
        : VanillaOptionTrade(env, AssetClass::FX, option, boughtCurrency, soldCurrency, boughtAmount,
                             TradeStrike(soldAmount / boughtAmount, soldCurrency)),
          fxIndex_(fxIndex) {
        tradeType_ = "FxOption";
    }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const string& boughtCurrency() const { return assetName_; }
    double boughtAmount() const { return quantity_; }
    const string& soldCurrency() const { return currency_; }
    double soldAmount() const { return strike_.value() * quantity_; }
    const std::string& fxIndex() const { return fxIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    //! If the option has automatic exercise, need an FX index for settlement.
    std::string fxIndex_;
};
} // namespace data
} // namespace ore
