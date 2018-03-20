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

/*! \file portfolio/commodityoption.hpp
    \brief Commodity option representation
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Commodity option trade representation
/*! \ingroup tradedata
*/
class CommodityOption : public Trade {
public:
    //! Default constructor
    CommodityOption() : Trade("CommodityOption"), strike_(0.0), quantity_(0.0) {}

    //! Constructor
    CommodityOption(const Envelope& env,
        const OptionData& optionData,
        const std::string& commodityName,
        const std::string& currency,
        QuantLib::Real strike,
        QuantLib::Real quantity);

    //! Build underlying instrument and link pricing engine
    void build(const boost::shared_ptr<EngineFactory>& engineFactory);

    //! \name Inspectors
    //@{
    const OptionData& option() const { return optionData_; }
    const std::string& commodityName() const { return commodityName_; }
    const std::string& currency() const { return currency_; }
    QuantLib::Real strike() const { return strike_; }
    QuantLib::Real quantity() const { return quantity_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    OptionData optionData_;
    std::string commodityName_;
    std::string currency_;
    QuantLib::Real strike_;
    QuantLib::Real quantity_;
};

}
}
