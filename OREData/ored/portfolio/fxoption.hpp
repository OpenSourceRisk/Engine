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

#include <ored/portfolio/oneassetoption.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable FX Option
/*!
  \ingroup tradedata
*/
class FxOption : public OneAssetOption {
public:
    //! Default constructor
    FxOption() : OneAssetOption(AssetClass::FX) { tradeType_ = "FxOption"; }
    //! Constructor
    FxOption(Envelope& env, OptionData option, string boughtCurrency, double boughtAmount, string soldCurrency,
             double soldAmount)
        : OneAssetOption(env, AssetClass::FX, option, boughtCurrency, soldCurrency, soldAmount / boughtAmount, boughtAmount)
          { tradeType_ = "FxOption"; }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const string& boughtCurrency() const { return assetName_; }
    double boughtAmount() const { return quantity_; }
    const string& soldCurrency() const { return currency_; }
    double soldAmount() const { return strike_ * quantity_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
};
} // namespace data
} // namespace ore
