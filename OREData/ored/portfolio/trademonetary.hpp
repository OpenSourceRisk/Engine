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

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>

using ore::data::XMLNode;
using ore::data::XMLSerializable;

namespace ore {
namespace data {

class TradeMonetary {

public:
    TradeMonetary() {};

    TradeMonetary(const QuantLib::Real& value, std::string currency = std::string())
        : value_(value), currency_(currency) {
        valueString_ = to_string(value);
    };
    TradeMonetary(const std::string& valueString) : valueString_(valueString) { 
        value_ = parseReal(valueString_);
    };

    void fromXMLNode(XMLNode* node);
    void toXMLNode(XMLDocument& doc, XMLNode* node);

    bool empty() const { return value_ == QuantLib::Null<QuantLib::Real>(); };
    QuantLib::Real value() const;
    const string& valueString() const { return valueString_; }
    std::string currency() const;
    void setCurrency(const std::string& currency) { currency_ = currency; }
    void setValue(const QuantLib::Real& value) { value_ = value; }

protected:
    QuantLib::Real value_ = QuantLib::Null<QuantLib::Real>();
    //! store a string version of the value, this ensures toXML values matches fromXML input
    std::string valueString_;
    std::string currency_;
};

} // namespace data
} // namespace ore
