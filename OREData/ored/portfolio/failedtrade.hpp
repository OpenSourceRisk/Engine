/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/failedtrade.hpp
    \brief Skeleton trade generated when trade loading/building fails
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

/*! Serializable skeleton trade to represent trades that failed loading or building
    \ingroup tradedata
*/
class FailedTrade : public ore::data::Trade {
public:
    FailedTrade();

    FailedTrade(const ore::data::Envelope& env);

    //! Trade Interface's build.
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    //! Set the original trade
    void setUnderlyingTradeType(const std::string& underlyingTradeType_);

    //! Get the original trade
    const std::string& underlyingTradeType() const;

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string underlyingTradeType_;
};

}
}
