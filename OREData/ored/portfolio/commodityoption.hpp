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

/*! \file ored/portfolio/commodityoption.hpp
    \brief Commodity option representation
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/vanillaoption.hpp>

namespace ore {
namespace data {

//! Commodity option trade representation
/*! \ingroup tradedata
 */
class CommodityOption : public VanillaOptionTrade {
public:
    //! Default constructor
    CommodityOption();

    //! Detailed constructor
    CommodityOption(const Envelope& env, const OptionData& optionData, const std::string& commodityName,
        const std::string& currency, QuantLib::Real strike, QuantLib::Real quantity);

    //! Build underlying instrument and link pricing engine
    void build(const boost::shared_ptr<EngineFactory>& engineFactory) override;

    //! Add underlying Commodity names
    std::map<AssetClass, std::set<std::string>> underlyingIndices() const override;

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Trade
    //@{
    bool hasCashflows() const override { return false; }
    //@}
};

}
}
