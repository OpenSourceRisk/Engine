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

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <ored/utilities/log.hpp>
#include <ored/portfolio/tlockdata.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void TreasuryLockData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "TreasuryLockData");
    QL_REQUIRE(node, "No TreasuryLockData Node");
    payer_ = XMLUtils::getChildValueAsBool(node, "Payer", true);
    originalBondData_.fromXML(XMLUtils::getChildNode(node, "BondData"));
    bondData_ = originalBondData_;
    referenceRate_ = XMLUtils::getChildValueAsDouble(node, "ReferenceRate", true);
    dayCounter_ = XMLUtils::getChildValue(node, "DayCounter", false);
    terminationDate_ = XMLUtils::getChildValue(node, "TerminationDate", true);
    paymentGap_ = XMLUtils::getChildValueAsInt(node, "PaymentGap", false, 0);
    paymentCalendar_ = XMLUtils::getChildValue(node, "PaymentCalendar", true);
    empty_ = false;
}

XMLNode* TreasuryLockData::toXML(XMLDocument& doc) const {
    XMLNode* tlockNode = doc.allocNode("TreasuryLockData");
    XMLUtils::addChild(doc, tlockNode, "Payer", payer_);
    XMLUtils::appendNode(tlockNode, originalBondData_.toXML(doc));
    XMLUtils::addChild(doc, tlockNode, "ReferenceRate", referenceRate_);
    if (!dayCounter_.empty())
        XMLUtils::addChild(doc, tlockNode, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, tlockNode, "TerminationDate", terminationDate_);
    XMLUtils::addChild(doc, tlockNode, "PaymentGap", paymentGap_);
    XMLUtils::addChild(doc, tlockNode, "PaymentCalendar", paymentCalendar_);
    return tlockNode;
}

} // namespace data
} // namespace ore
