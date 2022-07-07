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

#pragma once


#include <ored/portfolio/trademonetary.hpp>
#include <ql/interestrate.hpp>

namespace ore {
namespace data {

class StrikeBase {
public:
    StrikeBase() {}
    virtual ~StrikeBase() {}
    virtual QuantLib::Real strikeValue() const = 0;
    virtual XMLNode* toXML(XMLDocument& doc) = 0;
};

class StrikeYield : public StrikeBase {
public:
    StrikeYield() {}
    StrikeYield(const QuantLib::Rate& yield,
                QuantLib::Compounding compounding = QuantLib::Compounding::SimpleThenCompounded)
        : yield_(yield), compounding_(compounding) {}

    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc) override; 

    const QuantLib::Rate& yield() const { return yield_; }
    const QuantLib::Compounding& compounding() const { return compounding_; }

    QuantLib::Real strikeValue() const override { return yield_; }

private:
    QuantLib::Rate yield_;
    QuantLib::Compounding compounding_ = QuantLib::Compounding::SimpleThenCompounded;
};

class StrikePrice : public StrikeBase, public TradeMonetary {
public:
    StrikePrice() {}
    StrikePrice(QuantLib::Real value, const std::string& currency = std::string()) : TradeMonetary(value, currency) {}

    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc) override;

    QuantLib::Real strikeValue() const override { return value(); }
};

class TradeStrike {
public:
    TradeStrike(){};

    TradeStrike(QuantLib::Real value, const std::string& currency = string()) : 
        strike_(boost::make_shared<StrikePrice>(value, currency)) {};
    TradeStrike(const boost::shared_ptr<StrikeBase>& strike) : strike_(strike) {};

    const bool isStrikePrice() const;
    QuantLib::Real value() const;
    boost::shared_ptr<StrikeBase> strike() const { return strike_; }

    void fromXML(XMLNode* node, const bool allowYieldStrike = false);
    XMLNode* toXML(XMLDocument& doc);

private:
    boost::shared_ptr<StrikeBase> strike_;
    bool onlyStrike_ = false;
    bool noStrikePriceNode_ = false;
};

} // namespace data
} // namespace ore
