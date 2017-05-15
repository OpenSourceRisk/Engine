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

#include <ored/portfolio/indexcreditdefaultswapdata.hpp>

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void IndexCreditDefaultSwapData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IndexCreditDefaultSwapData");
    creditCurveId_ = XMLUtils::getChildValue(node, "CreditCurveId", true);
    settlesAccrual_ = XMLUtils::getChildValueAsBool(node, "SettlesAccrual", false);       // default = Y
    paysAtDefaultTime_ = XMLUtils::getChildValueAsBool(node, "PaysAtDefaultTime", false); // default = Y
    XMLNode* tmp = XMLUtils::getChildNode(node, "ProtectionStart");
    if (tmp)
        protectionStart_ = parseDate(XMLUtils::getNodeValue(tmp)); // null date if empty
    else
        protectionStart_ = Date();
    tmp = XMLUtils::getChildNode(node, "UpfrontDate");
    if (tmp)
        upfrontDate_ = parseDate(XMLUtils::getNodeValue(tmp)); // null date if empty
    else
        upfrontDate_ = Date();
    if (upfrontDate_ != Date())
        upfrontFee_ = boost::lexical_cast<Real>(XMLUtils::getChildValue(node, "UpfrontFee")); // throw if empty
    else
        upfrontFee_ = Null<Real>();
    leg_.fromXML(XMLUtils::getChildNode(node, "LegData"));
    basket_.fromXML(XMLUtils::getChildNode(node, "BasketData"));
}

XMLNode* IndexCreditDefaultSwapData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("IndexCreditDefaultSwapData");
    XMLUtils::appendNode(node, node);
    XMLUtils::addChild(doc, node, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, node, "SettlesAccrual", settlesAccrual_);
    XMLUtils::addChild(doc, node, "PaysAtDefaultTime", paysAtDefaultTime_);
    if (protectionStart_ != Date()) {
        std::ostringstream tmp;
        tmp << QuantLib::io::iso_date(protectionStart_);
        XMLUtils::addChild(doc, node, "ProtectionStart", tmp.str());
    }
    if (upfrontDate_ != Date()) {
        std::ostringstream tmp;
        tmp << QuantLib::io::iso_date(upfrontDate_);
        XMLUtils::addChild(doc, node, "UpfrontDate", tmp.str());
    }
    if (upfrontFee_ != Null<Real>())
        XMLUtils::addChild(doc, node, "UpfrontFee", upfrontFee_);
    XMLUtils::appendNode(node, leg_.toXML(doc));
    XMLUtils::appendNode(node, basket_.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
