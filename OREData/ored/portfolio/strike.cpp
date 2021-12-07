/*
 Copyright (C) 2019, 2021 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/portfolio/strike.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/export.hpp>

using namespace QuantLib;
using ore::data::XMLUtils;
using std::ostream;
using std::ostringstream;
using std::string;
using std::vector;

namespace ore {
namespace data {

TradeStrike::TradeStrike() {}

TradeStrike::TradeStrike(Real value, std::string currency) : value_(value), currency_(currency) {}

void TradeStrike::fromXML(XMLNode* node) { 
	currency_ = XMLUtils::getChildValue(node, "Currency", true);
    value_ = XMLUtils::getChildValueAsDouble(node, "Value", true);
}

XMLNode* TradeStrike::toXML(XMLDocument& doc) { 
	XMLNode* strikeNode = doc.allocNode("Strike");
    XMLUtils::addChild(doc, strikeNode, "Value", value_);
    XMLUtils::addChild(doc, strikeNode, "Currency", currency_);
    return strikeNode;
}

std::string TradeStrike::currency() const { return currency_; }
Real TradeStrike::value() const { 
    if (checkMinorCurrency(currency_)) 
        return convertMinorToMajorCurrency(currency_, value_);
    else
        return value_;
    return value_; 
}

} // namespace data
} // namespace ore
