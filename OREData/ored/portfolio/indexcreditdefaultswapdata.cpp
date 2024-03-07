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
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/utilities/time.hpp>

using namespace QuantLib;
using namespace QuantExt;
using ore::data::LegData;
using ore::data::parseBool;
using ore::data::parseDate;
using ore::data::parseReal;
using ore::data::to_string;
using ore::data::XMLDocument;
using ore::data::XMLUtils;
using ore::data::XMLNode;
using std::string;

namespace ore {
namespace data {

IndexCreditDefaultSwapData::IndexCreditDefaultSwapData() {}

IndexCreditDefaultSwapData::IndexCreditDefaultSwapData(const string& creditCurveId,
    const BasketData& basket,
    const LegData& leg,
    const bool settlesAccrual,
    const PPT protectionPaymentTime,
    const Date& protectionStart,
    const Date& upfrontDate,
    const Real upfrontFee,
    const Date& tradeDate,
						       const string& cashSettlementDays,
						       const bool rebatesAccrual)
    : CreditDefaultSwapData("", creditCurveId, leg, settlesAccrual, protectionPaymentTime, protectionStart,
			    upfrontDate, upfrontFee, Null<Real>(), "", tradeDate, cashSettlementDays, rebatesAccrual),
      basket_(basket) {}

void IndexCreditDefaultSwapData::fromXML(XMLNode* node) {

    CreditDefaultSwapData::fromXML(node);

    if (auto basketNode = XMLUtils::getChildNode(node, "BasketData"))
        basket_.fromXML(basketNode);

    indexStartDateHint_ = parseDate(XMLUtils::getChildValue(node, "IndexStartDateHint", false));
}

XMLNode* IndexCreditDefaultSwapData::toXML(XMLDocument& doc) const {

    XMLNode* node = CreditDefaultSwapData::toXML(doc);
    XMLUtils::appendNode(node, basket_.toXML(doc));
    if(indexStartDateHint_ != Date()) {
        XMLUtils::addChild(doc, node, "IndexStartDateHint", ore::data::to_string(indexStartDateHint_));
    }

    return node;
}

void IndexCreditDefaultSwapData::check(XMLNode* node) const {
    XMLUtils::checkNode(node, "IndexCreditDefaultSwapData");
}

XMLNode* IndexCreditDefaultSwapData::alloc(XMLDocument& doc) const {
    return doc.allocNode("IndexCreditDefaultSwapData");
}

std::string IndexCreditDefaultSwapData::creditCurveIdWithTerm() const {
    auto p = ore::data::splitCurveIdWithTenor(creditCurveId());
    if (p.second != 0 * Days)
        return creditCurveId();
    QuantLib::Schedule s = makeSchedule(leg().schedule());
    if (s.dates().empty())
        return p.first;
    QuantLib::Period t = QuantExt::implyIndexTerm(
        indexStartDateHint_ == Date() ? s.dates().front() : indexStartDateHint_, s.dates().back());
    if (t != 0 * Days)
        return p.first + "_" + ore::data::to_string(t);
    return p.first;
}

} // namespace data
} // namespace ore
