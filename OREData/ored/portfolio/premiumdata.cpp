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

#include <ored/portfolio/premiumdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

QuantLib::Date PremiumData::latestPremiumDate() const {
    QuantLib::Date latestPaymentDate = Date::minDate();

    for (const PremiumDatum& d : premiumData_)
        latestPaymentDate = std::max(latestPaymentDate, d.payDate);

    return latestPaymentDate;
}

void PremiumData::fromXML(XMLNode* node) {

    // support deprecated variant, where data is given in single fields under the root node

    auto depr_amount_node = XMLUtils::getChildNode(node, "PremiumAmount");
    auto depr_ccy_node = XMLUtils::getChildNode(node, "PremiumCurrency");
    auto depr_date_node = XMLUtils::getChildNode(node, "PremiumPayDate");

    if (depr_amount_node != nullptr) {
        std::string amountStr = XMLUtils::getNodeValue(depr_amount_node);
        if (!amountStr.empty()) {
            double amount = parseReal(amountStr);
            if (!close_enough(amount, 0.0)) {
                QL_REQUIRE(depr_ccy_node, "PremiumAmount (" << amount << ") given, but no PremiumCurrency");
                QL_REQUIRE(depr_date_node, "PremiumAmount (" << amount << ") given, but no PremiumPayDate");
                std::string ccyStr = XMLUtils::getNodeValue(depr_ccy_node);
                std::string dateStr = XMLUtils::getNodeValue(depr_date_node);
                QL_REQUIRE(!ccyStr.empty(), "PremiumAmount (" << amount << ") given, but no PremiumCurrency");
                QL_REQUIRE(!dateStr.empty(), "PremiumAmount (" << amount << ") given, but no PremiumPayDate");
                premiumData_.emplace_back(amount, ccyStr, parseDate(dateStr));
            }
        }
    }

    // standard variant, data is given in Premium nodes under Premiums root node

    if (auto p = XMLUtils::getChildNode(node, "Premiums")) {
        QL_REQUIRE(premiumData_.empty(), "Single PremiumAmount and Premiums node are not allowed simultaneously. Move "
                                         "the single premium to the Premiums node instead.");
        for (auto n : XMLUtils::getChildrenNodes(p, "Premium")) {
            PremiumDatum d;
            d.amount = XMLUtils::getChildValueAsDouble(n, "Amount", true);
            d.ccy = XMLUtils::getChildValue(n, "Currency", true);
            d.payDate = parseDate(XMLUtils::getChildValue(n, "PayDate", true));
            premiumData_.push_back(d);
        }
    }
}

XMLNode* PremiumData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Premiums");
    for (auto const& d : premiumData_) {
        XMLNode* p = XMLUtils::addChild(doc, node, "Premium");
        XMLUtils::addChild(doc, p, "Amount", d.amount);
        XMLUtils::addChild(doc, p, "Currency", d.ccy);
        XMLUtils::addChild(doc, p, "PayDate", ore::data::to_string(d.payDate));
    }
    return node;
}

} // namespace data
} // namespace ore
