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

#include <boost/lexical_cast.hpp>
#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void CreditDefaultSwapData::fromXML(XMLNode* node) {
    XMLUtils::cehckNode(node, "CreditDefaultSwapData");
    issuerId_ = XMLUtils::getChildValue(cdsNode, "IssuerId", true);
    creditCurveId_ = XMLUtils::getChildValue(cdsNode, "CreditCurveId", true);
    settlesAccrual_ = XMLUtils::getChildValueAsBool(cdsNode, "SettlesAccrual", false);       // default = Y
    paysAtDefaultTime_ = XMLUtils::getChildValueAsBool(cdsNode, "PaysAtDefaultTime", false); // default = Y
    XMLNode* tmp = XMLUtils::getChildNode(cdsNode, "ProtectionStart");
    if (tmp)
        protectionStart_ = parseDate(XMLUtils::getNodeValue(tmp)); // null date if empty
    else
        protectionStart_ = Date();
    tmp = XMLUtils::getChildNode(cdsNode, "UpfrontDate");
    if (tmp)
        upfrontDate_ = parseDate(XMLUtils::getNodeValue(tmp)); // null date if empty
    else
        upfrontDate_ = Date();
    if (upfrontDate_ != Date())
        upfrontFee_ = boost::lexical_cast<Real>(XMLUtils::getChildValue(cdsNode, "UpfrontFee")); // throw if empty
    else
        upfrontFee_ = Null<Real>();
    leg_.fromXML(XMLUtils::getChildNode(cdsNode, "LegData"));
}

XMLNode* CreditDefaultSwapData::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* cdsNode = doc.allocNode("CreditDefaultSwapData");
    XMLUtils::appendNode(node, cdsNode);
    XMLUtils::addChild(doc, cdsNode, "IssuerId", issuerId_);
    XMLUtils::addChild(doc, cdsNode, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, cdsNode, "SettlesAccrual", settlesAccrual_);
    XMLUtils::addChild(doc, cdsNode, "PaysAtDefaultTime", paysAtDefaultTime_);
    if (protectionStart_ != Date()) {
        std::ostringstream tmp;
        tmp << QuantLib::io::iso_date(protectionStart_);
        XMLUtils::addChild(doc, cdsNode, "ProtectionStart", tmp.str());
    }
    if (upfrontDate_ != Date()) {
        std::ostringstream tmp;
        tmp << QuantLib::io::iso_date(upfrontDate_);
        XMLUtils::addChild(doc, cdsNode, "UpfrontDate", tmp.str());
    }
    if (upfrontFee_ != Null<Real>())
        XMLUtils::addChild(doc, cdsNode, "UpfrontFee", upfrontFee_);
    XMLUtils::appendNode(cdsNode, leg_.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
