/*
 Copyright (C) 2017, 2023 Quaternion Risk Management Ltd
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

/*! \file portfolio/forwardvolatilityagreement.hpp
    \brief ForwardVolatilityAgreement data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable ForwardVolatilityAgreement
/*!
  \ingroup tradedata
*/
class ForwardVolatilityAgreement : public ScriptedTrade {
public:
    explicit ForwardVolatilityAgreement(const std::string& tradeType = "ForwardVolatilityAgreement") : ScriptedTrade(tradeType) {}
    ForwardVolatilityAgreement(const Envelope& env, const std::string& fvaDate, const std::string& optionExpiry,
                               const std::string& premiumDate, const QuantLib::ext::shared_ptr<Underlying>& underlying,
                               const std::string& longShort, const std::string& underlyingStrike,
                               const std::string& impliedVolStrike, const std::string& quantity,
                               const std::string& payCcy, const std::string& settlementDate,
                               const std::string& payoffType = "Straddle",
                               const std::string& dayCountFraction = "",
                               const std::string& dividendYield = "",
                               const std::string& fixedRate = "")
        : ScriptedTrade("ForwardVolatilityAgreement", env), fvaDate_(fvaDate), optionExpiry_(optionExpiry),
          premiumDate_(premiumDate), underlying_(underlying), longShort_(longShort),
          underlyingStrike_(underlyingStrike), impliedVolStrike_(impliedVolStrike), quantity_(quantity),
          payCcy_(payCcy), settlementDate_(settlementDate), payoffType_(payoffType),
          dayCountFraction_(dayCountFraction), dividendYield_(dividendYield), fixedRate_(fixedRate),
          underlyingStrikeProvided_(underlyingStrike != "0") {}

    void build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    
private:
    void initIndices();
    std::string fvaDate_;
    std::string optionExpiry_;
    std::string premiumDate_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
    std::string longShort_;
    std::string underlyingStrike_;
    std::string impliedVolStrike_;
    std::string quantity_;
    std::string payCcy_;
    std::string settlementDate_;
    std::string payoffType_;
    std::string dayCountFraction_;
    std::string dividendYield_ = "0";
    std::string fixedRate_ = "0";
    bool underlyingStrikeProvided_ = false;
};

class EquityForwardVolatilityAgreement : public ForwardVolatilityAgreement {
public:
    EquityForwardVolatilityAgreement() : ForwardVolatilityAgreement("EquityForwardVolatilityAgreement") {}
};

class FxForwardVolatilityAgreement : public ForwardVolatilityAgreement {
public:
    FxForwardVolatilityAgreement() : ForwardVolatilityAgreement("FxForwardVolatilityAgreement") {}
};

class CommodityForwardVolatilityAgreement : public ForwardVolatilityAgreement {
public:
    CommodityForwardVolatilityAgreement() : ForwardVolatilityAgreement("CommodityForwardVolatilityAgreement") {}
};

} // namespace data
} // namespace ore
