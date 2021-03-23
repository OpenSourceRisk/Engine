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

#include <ored/configuration/arbitragecheckconfig.hpp>

#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

ArbitrageCheckConfig::ArbitrageCheckConfig() {

    // set default values

    tenors_ = {2 * Weeks, 1 * Months, 2 * Months, 3 * Months, 6 * Months, 9 * Months, 1 * Years,
               2 * Years, 3 * Years,  4 * Years,  5 * Years,  6 * Years,  7 * Years,  8 * Years,
               9 * Years, 10 * Years, 12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years};

    moneyness_ = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3,
                  1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 3.0, 4.0, 5.0, 7.0, 10.0};
}

void ArbitrageCheckConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ArbitrageCheck");
    if (auto tmp = XMLUtils::getChildNode(node, "Tenors")) {
        tenors_ = parseListOfValues<Period>(XMLUtils::getNodeValue(tmp), &parsePeriod);
        defaultTenors_ = false;
    }
    if (auto tmp = XMLUtils::getChildNode(node, "Moneyness")) {
        moneyness_ = parseListOfValues<Real>(XMLUtils::getNodeValue(tmp)), &parseReal);
        defaultMoneyness_ = false;
    }
}

XMLNode* ArbitrageCheckConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ArbitrageCheck");
    if (!defaultTenors_)
        XMLUtils::addGenericChildAsList(doc, node, "Tenors", tenors_);
    if (!defaultMoneyness_)
        XMLUtils::addGenericChildAsList(doc, node, "Moneyness", moneyness_);
    return node;
}

} // namespace data
} // namespace ore
