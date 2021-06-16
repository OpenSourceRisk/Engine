/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file portfolio/fxasianoption.hpp
    \brief Fx Asian Option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/asianoption.hpp>

namespace ore {
namespace data {

//! Serializable Fx Asian Option
/*!
  \ingroup tradedata
*/
class FxAsianOption : public AsianOptionTrade {
public:
    //! Default constructor
    FxAsianOption() : AsianOptionTrade(AssetClass::FX) { tradeType_ = "FxAsianOption"; }
    //! Constructor
    FxAsianOption(Envelope& env, OptionData option, OptionAsianData asianData, ScheduleData scheduleData,
                  string boughtCurrency, double boughtAmount, string soldCurrency, double soldAmount,
                  const std::string& fxIndex)
        : AsianOptionTrade(env, AssetClass::FX, option, asianData, scheduleData, boughtCurrency, soldCurrency,
                           soldAmount / boughtAmount, boughtAmount),
          fxIndex_(fxIndex) {
        tradeType_ = "FxAsianOption";
    }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const string& boughtCurrency() const { return assetName_; }
    double boughtAmount() const { return quantity_; }
    const string& soldCurrency() const { return currency_; }
    double soldAmount() const { return strike_ * quantity_; }
    const std::string& fxIndex() const { return fxIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    //! Needed for past fixings
    std::string fxIndex_;
};

} // namespace data
} // namespace ore
