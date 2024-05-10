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

/*! \file ored/portfolio/bondoption.hpp
\brief bond option data model and serialization
\ingroup tradedata
*/

#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/portfolio/referencedata.hpp>

namespace ore {
namespace data {

//! Serializable Bond Option
/*!
\ingroup tradedata
*/
class BondOption : public Trade {
public:
    //! Default constructor
    BondOption() : Trade("BondOption"), redemption_(0.0), knocksOut_(false) {}

    //! Constructor taking trade data
    BondOption(Envelope env, const BondData& bondData, const OptionData& optionData, TradeStrike strike, bool knocksOut)
        : Trade("BondOption", env), bondData_(bondData), optionData_(optionData), strike_(strike),
          knocksOut_(knocksOut) {}

    // Build QuantLib/QuantExt instrument, link pricing engine
    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const BondData& bondData() const { return bondData_; }
    const OptionData& optionData() const { return optionData_; }
    TradeStrike strike() const { return strike_; }
    double redemption() const { return redemption_; }
    string priceType() const { return priceType_; }

private:
    BondData originalBondData_, bondData_;
    OptionData optionData_;

    TradeStrike strike_;
    double redemption_;
    string priceType_;
    string currency_;
    bool knocksOut_;

    QuantLib::ext::shared_ptr<ore::data::Bond> underlying_;
};
} // namespace data
} // namespace ore
