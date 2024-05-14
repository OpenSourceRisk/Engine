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

#include <ored/portfolio/rangebound.hpp>

#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

void RangeBound::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "RangeBound");
    if (auto n = XMLUtils::getChildNode(node, "RangeFrom"))
        from_ = parseReal(XMLUtils::getNodeValue(n));
    else
        from_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "RangeTo"))
        to_ = parseReal(XMLUtils::getNodeValue(n));
    else
        to_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "Leverage"))
        leverage_ = parseReal(XMLUtils::getNodeValue(n));
    else
        leverage_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "Strike"))
        strike_ = parseReal(XMLUtils::getNodeValue(n));
    else
        strike_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "StrikeAdjustment"))
        strikeAdjustment_ = parseReal(XMLUtils::getNodeValue(n));
    else
        strikeAdjustment_ = Null<Real>();
}

XMLNode* RangeBound::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("RangeBound");
    if (from_ != Null<Real>())
        XMLUtils::addChild(doc, node, "RangeFrom", from_);
    if (to_ != Null<Real>())
        XMLUtils::addChild(doc, node, "RangeTo", to_);
    if (leverage_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Leverage", leverage_);
    if (strike_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Strike", strike_);
    if (strikeAdjustment_ != Null<Real>())
        XMLUtils::addChild(doc, node, "StrikeAdjustment", strikeAdjustment_);
    return node;
}

bool operator==(const RangeBound& a, const RangeBound& b) {
    return a.from() == b.from() && a.to() == b.to() && a.leverage() == b.leverage() && a.strike() == b.strike() &&
           a.strikeAdjustment() == b.strikeAdjustment();
}

namespace {
std::string output(const Real d) {
    if (d == Null<Real>())
        return "na";
    else
        return std::to_string(d);
}
} // namespace

std::ostream& operator<<(std::ostream& out, const RangeBound& t) {
    return out << "[" << output(t.from()) << ", " << output(t.to()) << "] x " << output(t.leverage()) << " @ "
               << output(t.strike()) << " +- " << output(t.strikeAdjustment());
}

std::ostream& operator<<(std::ostream& out, const std::vector<RangeBound>& t) {
    std::ostringstream s;
    s << "[ ";
    for (Size i = 0; i < t.size(); ++i)
        s << t[i] << (i < t.size() - 1 ? ", " : "");
    s << " ]";
    return out << s.str();
}

} // namespace data
} // namespace ore
