/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
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
    void build(const boost::shared_ptr<ore::data::EngineFactory>&) override;

    //! Set the original trade
    void setUnderlyingTradeType(const std::string& underlyingTradeType_);

    //! Get the original trade
    const std::string& underlyingTradeType() const;

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::string underlyingTradeType_;
};

}
}
