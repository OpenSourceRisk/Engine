/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/basketdata.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void BasketData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BasketData");

    for (XMLNode* child = XMLUtils::getChildNode(node, "Name"); child; child = XMLUtils::getNextSibling(child)) {
        issuers_.push_back(XMLUtils::getChildValue(node, "IssuerId", true));
	qualifiers_.push_back(XMLUtils::getChildValue(node, "Qualifier", true));
        creditCurves_.push_back(XMLUtils::getChildValue(node, "CreditCurveId", true));
	notionals_.push_back(XMLUtils::getChildValueAsDouble(node, "Notional", true));
        currencies_.push_back(XMLUtils::getChildValue(node, "Currency", true));
    }
}

XMLNode* BasketData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("BasketData");
    QL_FAIL("not implemented yet");
    return node;
}

} // namespace data
} // namespace ore
