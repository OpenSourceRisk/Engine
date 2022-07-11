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

#include <ored/portfolio/trademonetary.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {


void TradeMonetary::fromXMLNode(XMLNode* node) { 
	currency_ = XMLUtils::getChildValue(node, "Currency", false);
    valueString_ = XMLUtils::getChildValue(node, "Value", true);
    value_ = parseReal(valueString_);
}

void TradeMonetary::toXMLNode(XMLDocument& doc, XMLNode* node) {
    XMLUtils::addChild(doc, node, "Value", valueString_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
}

std::string TradeMonetary::currency() const { 
    if (!currency_.empty())
        return parseCurrencyWithMinors(currency_).code();
    else
        return currency_;
}

QuantLib::Real TradeMonetary::value() const { 
    if (currency_.empty())
        return value_;
    else
        return convertMinorToMajorCurrency(currency_, value_); 
}

} // namespace data
} // namespace ore
