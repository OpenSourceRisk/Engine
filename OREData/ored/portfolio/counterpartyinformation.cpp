/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/counterpartyinformation.hpp>
#include <ored/utilities/to_string.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>

using namespace ore::data;
using namespace QuantLib;

namespace ore {
namespace data {

// custom comparator for bimaps
struct string_cmp {
    bool operator()(const string& lhs, const string& rhs) const {
        return ((boost::to_lower_copy(lhs)) < (boost::to_lower_copy(rhs)));
    }
};

// Ease the notation below
template <typename T> using bm = boost::bimap<T, boost::bimaps::set_of<std::string, string_cmp>>;

const bm<CounterpartyCreditQuality> creditQualityMap =
    boost::assign::list_of<bm<CounterpartyCreditQuality>::value_type>(
        CounterpartyCreditQuality::HY, "HY")( // high yield
        CounterpartyCreditQuality::IG, "IG")( // investment grade
        CounterpartyCreditQuality::NR, "NR")  // not rated
        ;

std::ostream& operator<<(std::ostream& out, const CounterpartyCreditQuality& mt) {
    QL_REQUIRE(creditQualityMap.left.count(mt) > 0, "Margin type not a valid CounterpartyCreditQuality");
    return out << creditQualityMap.left.at(mt);
}

CounterpartyCreditQuality parseCounterpartyCreditQuality(const string& mt) {
    QL_REQUIRE(creditQualityMap.right.count(mt) > 0,
               "string " << mt << " does not correspond to a valid CounterpartyCreditQuality");
    return creditQualityMap.right.at(mt);
}


CounterpartyInformation::CounterpartyInformation(ore::data::XMLNode* node) {
    fromXML(node);
}

//! loads NettingSetDefinition object from XML
void CounterpartyInformation::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Counterparty"); 
    
    counterpartyId_ = XMLUtils::getChildValue(node, "CounterpartyId", true);
    bool isClearingCpDefault = false;
    if (XMLUtils::getChildNode(node, "ClearingCounterparty")) {
        isClearingCP_ = XMLUtils::getChildValueAsBool(node, "ClearingCounterparty", false, isClearingCpDefault);
    } else {
        isClearingCP_ = isClearingCpDefault;
    }
    std::string cq = XMLUtils::getChildValue(node, "CreditQuality", false);
    creditQuality_ = cq.empty() ? CounterpartyCreditQuality::NR : parseCounterpartyCreditQuality(cq);
    baCvaRiskWeight_ = XMLUtils::getChildValueAsDouble(node, "BaCvaRiskWeight", false, 0.0);
    saCcrRiskWeight_ = XMLUtils::getChildValueAsDouble(node, "SaCcrRiskWeight", false, 1.0);
    saCvaRiskBucket_ = XMLUtils::getChildValue(node, "SaCvaRiskBucket", false);

}

//! writes object to XML
XMLNode* CounterpartyInformation::toXML(XMLDocument& doc) const {
    // Allocate a node.
    XMLNode* node = doc.allocNode("Counterparty");
    // Add the mandatory members.
    XMLUtils::addChild(doc, node, "CounterpartyId", counterpartyId_);
    XMLUtils::addChild(doc, node, "ClearingCounterparty", isClearingCP_);
    if (!isClearingCP_)
        XMLUtils::addChild(doc, node, "CreditQuality", ore::data::to_string(creditQuality_));
    if (baCvaRiskWeight_ != Null<Real>())
        XMLUtils::addChild(doc, node, "BaCvaRiskWeight", baCvaRiskWeight_);
    if (saCcrRiskWeight_ != Null<Real>())
        XMLUtils::addChild(doc, node, "SaCcrRiskWeight", saCcrRiskWeight_);
    if (saCvaRiskBucket_ != "")
        XMLUtils::addChild(doc, node, "SaCvaRiskBucket", saCvaRiskBucket_);

    return node;
}

} // data
} // ore
