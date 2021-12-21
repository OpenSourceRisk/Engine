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

/*! \file ored/portfolio/fxbarrieroption.hpp
    \brief FX Barrier Option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/barrieroption.hpp>

namespace ore {
namespace data {

//! Serializable FX Barrier Option
/*!
  \ingroup tradedata
*/
class FxBarrierOption : public BarrierOptionTrade {
public:
    //! Default constructor
    FxBarrierOption() : BarrierOptionTrade(AssetClass::FX) { tradeType_ = "FxBarrierOption"; }
    //! Constructor
    FxBarrierOption(Envelope& env, OptionData option, OptionBarrierData barrier, string boughtCurrency,
                    double boughtAmount, string soldCurrency, double soldAmount, const string& fxIndex = "")
        : BarrierOptionTrade(env, AssetClass::FX, option, barrier, boughtCurrency, soldCurrency,
                             soldAmount / boughtAmount, boughtAmount),
          fxIndex_(fxIndex) {
        tradeType_ = "FxBarrierOption";
    }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const string& boughtCurrency() const { return assetName_; }
    double boughtAmount() const { return quantity_; }
    const string& soldCurrency() const { return currency_; }
    double soldAmount() const { return strike_ * quantity_; }
    const string& fxIndex() const { return fxIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    //! If the option has automatic exercise, need an FX index for settlement.
    string fxIndex_;
};
} // namespace data
} // namespace ore
