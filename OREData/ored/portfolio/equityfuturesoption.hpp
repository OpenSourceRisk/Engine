/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file portfolio/equityfuturesoption.hpp
    \brief EQ Futures Option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/vanillaoption.hpp>

namespace ore {
namespace data {
using std::string;
using namespace ore::data;

//! Serializable EQ Futures Option
/*!
  \ingroup tradedata
*/
class EquityFutureOption : public VanillaOptionTrade {
public:

    //! Default constructor
    EquityFutureOption() : VanillaOptionTrade(AssetClass::EQ) { tradeType_ = "EquityFutureOption"; }
    //! Constructor
    EquityFutureOption(Envelope& env, OptionData option, const string& currency, Real quantity, 
        const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying, TradeStrike strike, QuantLib::Date forwardDate,
        const QuantLib::ext::shared_ptr<QuantLib::Index>& index = nullptr, const std::string& indexName = "");

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const std::string& name() const { return underlying_->name(); }
    const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying() const { return underlying_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

private:
    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying_;    
        
};
} // namespace data
} // namespace ore
