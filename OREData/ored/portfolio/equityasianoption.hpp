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

/*! \file portfolio/equityasianoption.hpp
    \brief Equity Asian Option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/asianoption.hpp>

namespace ore {
namespace data {

//! Serializable Equity Asian Option
/*!
  \ingroup tradedata
*/
class EquityAsianOption : public AsianOptionTrade {
public:
    //! Default constructor
    EquityAsianOption() : AsianOptionTrade(AssetClass::EQ) { tradeType_ = "EquityAsianOption"; }
    //! Constructor
    EquityAsianOption(Envelope& env, OptionData option, OptionAsianData asianData, ScheduleData scheduleData,
                      EquityUnderlying equityUnderlying, string currency, double strike, double quantity)
        : AsianOptionTrade(env, AssetClass::EQ, option, asianData, scheduleData, equityUnderlying.name(), currency,
                           strike, quantity),
          equityUnderlying_(equityUnderlying) {
        tradeType_ = "EquityAsianOption";
    }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>> underlyingIndices() const override;

    //! \name Inspectors
    //@{
    const string& equityName() const { return equityUnderlying_.name(); }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    EquityUnderlying equityUnderlying_;
};

} // namespace data
} // namespace ore
