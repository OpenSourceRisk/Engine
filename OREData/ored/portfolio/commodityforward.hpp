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

/*! \file ored/portfolio/commodityforward.hpp
    \brief Commodity forward representation
    
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Commodity forward contract
//! \ingroup tradedata
class CommodityForward : public Trade {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityForward();
    //! Detailed constructor
    CommodityForward(const Envelope& envelope, 
        const std::string& position, 
        const std::string& commodityName,
        const std::string& currency, 
        QuantLib::Real quantity, 
        const std::string& maturityDate, 
        QuantLib::Real strike);
    //@}

    //! \name Inspectors
    //@{
    std::string position() { return position_; }
    std::string commodityName() { return commodityName_; }
    std::string currency() { return currency_; }
    QuantLib::Real quantity() { return quantity_; }
    std::string maturityDate() { return maturityDate_; }
    QuantLib::Real strike() { return strike_; }
    //@}

    //! \name Trade interface
    //@{
    void build(const boost::shared_ptr<EngineFactory>&);
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    std::string position_;
    std::string commodityName_;
    std::string currency_;
    QuantLib::Real quantity_;
    std::string maturityDate_;
    QuantLib::Real strike_;
};
}
}
