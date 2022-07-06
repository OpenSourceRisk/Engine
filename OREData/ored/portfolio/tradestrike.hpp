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
#include <boost/serialization/variant.hpp>
#include <boost/optional.hpp>

namespace ore {
namespace data {

class StrikeYield {
public:
    StrikeYield() {}
    StrikeYield(const QuantLib::Rate& yield,
                QuantLib::Compounding compounding = QuantLib::Compounding::SimpleThenCompounded)
        : yield_(yield), compounding_(compounding) {}

    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc); 

    const QuantLib::Rate& yield() const { return yield_; }
    const QuantLib::Compounding& compounding() const { return compounding_; }

private:
    QuantLib::Rate yield_;
    QuantLib::Compounding compounding_ = QuantLib::Compounding::SimpleThenCompounded;
};

class StrikePrice : public TradeMonetary {
public:
    StrikePrice() {}
    StrikePrice(QuantLib::Real value, const std::string& currency = std::string()) : TradeMonetary(value, currency) {}

    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
};

class TradeStrike {
public:
    TradeStrike(){};

    TradeStrike(QuantLib::Real value, const std::string& currency) : strike_(StrikePrice(value, currency)){};
    TradeStrike(const StrikePrice& strike) : strike_(strike){};
    TradeStrike(const StrikeYield& yield) : strike_(yield){};

    const bool isStrikePrice() const;
    QuantLib::Real value();
    const boost::variant<StrikePrice, StrikeYield> strike() const { return *strike_; }

    void fromXML(XMLNode* node, const bool allowYieldStrike = false);
    XMLNode* toXML(XMLDocument& doc);

private:
    boost::optional<boost::variant<StrikePrice, StrikeYield>> strike_;
    bool onlyStrike_ = false;
    bool noStrikePriceNode_ = false;
};

} // namespace data
} // namespace ore
