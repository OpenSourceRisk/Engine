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
#include <ql/tuple.hpp>
#include <boost/variant.hpp>

namespace ore {
namespace data {

class TradeStrike {
public:
    enum class Type {
        Price,
        Yield
    };

    typedef TradeMonetary StrikePrice;
    struct StrikeYield {
        StrikeYield()
            : yield(QuantLib::Null<QuantLib::Real>()), compounding(QuantLib::Compounding::SimpleThenCompounded) {}
        StrikeYield(const QuantLib::Real& value, QuantLib::Compounding compounding = QuantLib::Compounding::SimpleThenCompounded)
            : yield(value), compounding(compounding) {}
        QuantLib::Rate yield;
        QuantLib::Compounding compounding;
    };

    TradeStrike() : type_(Type::Price) {};
    TradeStrike(Type type, const QuantLib::Real& value);
    TradeStrike(const QuantLib::Real& value, const std::string& currency);
    TradeStrike(const QuantLib::Real& value, QuantLib::Compounding compounding);

    QuantLib::Real value() const;
    Type type() const { return type_; };
    std::string currency();
    const QuantLib::Compounding& compounding();

    StrikePrice& strikePrice() const { return boost::get<StrikePrice>(strike_); }
    StrikeYield& strikeYield() const { return boost::get<StrikeYield>(strike_); }

    void setValue(const QuantLib::Real& value);
    void setCurrency(const std::string& currency);

    void fromXML(XMLNode* node, const bool isRequired = true, const bool allowYieldStrike = false);
    XMLNode* toXML(XMLDocument& doc) const;

    const bool empty() const;

private:
    mutable boost::variant<StrikeYield, StrikePrice> strike_;
    Type type_;
    bool onlyStrike_ = false;
    bool noStrikePriceNode_ = false;

};

} // namespace data
} // namespace ore
