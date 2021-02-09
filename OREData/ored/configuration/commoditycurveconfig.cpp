/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/configuration/commoditycurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

using std::string;
using std::vector;

namespace ore {
namespace data {

PriceSegment::PriceSegment() : empty_(true), type_(Type::Future) {}

PriceSegment::PriceSegment(const string& type, const string& conventionsId, const vector<string>& quotes,
    const boost::optional<unsigned short>& priority)
    : strType_(type), conventionsId_(conventionsId), quotes_(quotes), priority_(priority), empty_(false),
      type_(parsePriceSegmentType(strType_)) {}

PriceSegment::Type PriceSegment::type() const {
    return type_;
}

const string& PriceSegment::conventionsId() const {
    return conventionsId_;
}

const vector<string>& PriceSegment::quotes() const {
    return quotes_;
}

const boost::optional<unsigned short>& PriceSegment::priority() const {
    return priority_;
}

bool PriceSegment::empty() const {
    return empty_;
}

void PriceSegment::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "PriceSegment");
    strType_ = XMLUtils::getChildValue(node, "Type", true);
    conventionsId_ = XMLUtils::getChildValue(node, "Conventions", true);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);

    if (XMLNode* n = XMLUtils::getChildNode(node, "Priority")) {
        priority_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    empty_ = false;
    type_ = parsePriceSegmentType(strType_);
}

XMLNode* PriceSegment::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("PriceSegment");
    XMLUtils::addChild(doc, node, "Type", strType_);
    if (priority_) {
        XMLUtils::addChild(doc, node, "Priority", *priority_);
    }
    XMLUtils::addChild(doc, node, "Conventions", conventionsId_);
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);

    return node;
}

CommodityCurveConfig::CommodityCurveConfig(const string& curveId, const string& curveDescription,
                                           const string& currency, const vector<string>& quotes,
                                           const string& commoditySpotQuote, const string& dayCountId,
                                           const string& interpolationMethod, bool extrapolation,
                                           const string& conventionsId)
    : CurveConfig(curveId, curveDescription), type_(Type::Direct), fwdQuotes_(quotes), currency_(currency),
      commoditySpotQuoteId_(commoditySpotQuote), dayCountId_(dayCountId), interpolationMethod_(interpolationMethod),
      extrapolation_(extrapolation), conventionsId_(conventionsId) {

    quotes_ = quotes;
    if (!commoditySpotQuote.empty()) {
        quotes_.insert(quotes_.begin(), commoditySpotQuote);
    }
}

CommodityCurveConfig::CommodityCurveConfig(const string& curveId, const string& curveDescription,
                                           const string& currency, const string& basePriceCurveId,
                                           const string& baseYieldCurveId, const string& yieldCurveId,
                                           bool extrapolation)
    : CurveConfig(curveId, curveDescription), type_(Type::CrossCurrency), currency_(currency),
      basePriceCurveId_(basePriceCurveId), baseYieldCurveId_(baseYieldCurveId), yieldCurveId_(yieldCurveId),
      extrapolation_(extrapolation), addBasis_(true), monthOffset_(0) {
    populateRequiredCurveIds();
}

CommodityCurveConfig::CommodityCurveConfig(const string& curveId, const string& curveDescription,
                                           const string& currency, const string& basePriceCurveId,
                                           const string& baseConventionsId, const vector<string>& basisQuotes,
                                           const string& basisConventionsId, const string& dayCountId,
                                           const string& interpolationMethod, bool extrapolation, bool addBasis,
                                           QuantLib::Natural monthOffset)
    : CurveConfig(curveId, curveDescription), type_(Type::Basis), fwdQuotes_(basisQuotes), currency_(currency),
      dayCountId_(dayCountId), interpolationMethod_(interpolationMethod), basePriceCurveId_(basePriceCurveId),
      extrapolation_(extrapolation), conventionsId_(basisConventionsId), baseConventionsId_(baseConventionsId),
      addBasis_(addBasis), monthOffset_(monthOffset) {
    populateRequiredCurveIds();
}

CommodityCurveConfig::CommodityCurveConfig(const string& curveId, const string& curveDescription,
    const string& currency, const vector<PriceSegment>& priceSegments, const string& dayCountId,
    const string& interpolationMethod, bool extrapolation, const boost::optional<BootstrapConfig>& bootstrapConfig)
    : CurveConfig(curveId, curveDescription), type_(Type::Piecewise), currency_(currency),
      dayCountId_(dayCountId), interpolationMethod_(interpolationMethod), extrapolation_(extrapolation),
      addBasis_(false), monthOffset_(0), bootstrapConfig_(bootstrapConfig) {
    processSegments(priceSegments);
}

void CommodityCurveConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    if (XMLNode* n = XMLUtils::getChildNode(node, "BasisConfiguration")) {

        type_ = Type::Basis;
        basePriceCurveId_ = XMLUtils::getChildValue(n, "BasePriceCurve", true);
        baseConventionsId_ = XMLUtils::getChildValue(n, "BasePriceConventions", true);
        quotes_ = fwdQuotes_ = XMLUtils::getChildrenValues(n, "BasisQuotes", "Quote");
        conventionsId_ = XMLUtils::getChildValue(n, "BasisConventions", true);
        dayCountId_ = XMLUtils::getChildValue(n, "DayCounter", false);
        interpolationMethod_ = XMLUtils::getChildValue(n, "InterpolationMethod", false);
        addBasis_ = XMLUtils::getChildValueAsBool(n, "AddBasis", false);
        monthOffset_ = XMLUtils::getChildValueAsInt(n, "MonthOffset", false);

    } else if (XMLNode* n = XMLUtils::getChildNode(node, "BasePriceCurve")) {

        type_ = Type::CrossCurrency;
        basePriceCurveId_ = XMLUtils::getNodeValue(n);
        baseYieldCurveId_ = XMLUtils::getChildValue(node, "BaseYieldCurve", true);
        yieldCurveId_ = XMLUtils::getChildValue(node, "YieldCurve", true);

    } else if (XMLNode* n = XMLUtils::getChildNode(node, "PriceSegments")) {

        vector<PriceSegment> priceSegments;
        type_ = Type::Piecewise;
        for (XMLNode* c = XMLUtils::getChildNode(n); c; c = XMLUtils::getNextSibling(c)) {
            PriceSegment ps;
            ps.fromXML(c);
            priceSegments.push_back(ps);
        }
        processSegments(priceSegments);

        dayCountId_ = XMLUtils::getChildValue(n, "DayCounter", false);
        interpolationMethod_ = XMLUtils::getChildValue(node, "InterpolationMethod", false);

        if (XMLNode* bcn = XMLUtils::getChildNode(node, "BootstrapConfig")) {
            bootstrapConfig_ = BootstrapConfig();
            bootstrapConfig_->fromXML(bcn);
        }

    } else {

        type_ = Type::Direct;
        dayCountId_ = XMLUtils::getChildValue(node, "DayCounter", false);
        commoditySpotQuoteId_ = XMLUtils::getChildValue(node, "SpotQuote", false);
        fwdQuotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote");
        quotes_ = fwdQuotes_;
        if (commoditySpotQuoteId_ != "")
            quotes_.insert(quotes_.begin(), commoditySpotQuoteId_);

        interpolationMethod_ = XMLUtils::getChildValue(node, "InterpolationMethod", false);
        conventionsId_ = XMLUtils::getChildValue(node, "Conventions", false);
    }

    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation");

    populateRequiredCurveIds();
}

XMLNode* CommodityCurveConfig::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CommodityCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    if (type_ == Type::Basis) {

        XMLNode* basisNode = XMLUtils::addChild(doc, node, "BasisConfiguration");

        XMLUtils::addChild(doc, basisNode, "BasePriceCurve", basePriceCurveId_);
        XMLUtils::addChild(doc, basisNode, "BasePriceConventions", baseConventionsId_);
        XMLUtils::addChildren(doc, basisNode, "BasisQuotes", "Quote", fwdQuotes_);
        XMLUtils::addChild(doc, basisNode, "BasisConventions", conventionsId_);
        XMLUtils::addChild(doc, basisNode, "DayCounter", dayCountId_);
        XMLUtils::addChild(doc, basisNode, "InterpolationMethod", interpolationMethod_);
        XMLUtils::addChild(doc, basisNode, "AddBasis", addBasis_);
        XMLUtils::addChild(doc, basisNode, "MonthOffset", static_cast<int>(monthOffset_));

    } else if (type_ == Type::CrossCurrency) {

        XMLUtils::addChild(doc, node, "BasePriceCurve", basePriceCurveId_);
        XMLUtils::addChild(doc, node, "BaseYieldCurve", baseYieldCurveId_);
        XMLUtils::addChild(doc, node, "YieldCurve", yieldCurveId_);

    } else if (type_ == Type::Piecewise) {

        // Add the price segment nodes.
        XMLNode* segmentsNode = doc.allocNode("PriceSegments");
        for (auto& kv : priceSegments_) {
            XMLUtils::appendNode(segmentsNode, kv.second.toXML(doc));
        }
        XMLUtils::appendNode(node, segmentsNode);

        XMLUtils::addChild(doc, node, "DayCounter", dayCountId_);
        XMLUtils::addChild(doc, node, "InterpolationMethod", interpolationMethod_);

    } else {

        if (!commoditySpotQuoteId_.empty())
            XMLUtils::addChild(doc, node, "SpotQuote", commoditySpotQuoteId_);
        XMLUtils::addChildren(doc, node, "Quotes", "Quote", fwdQuotes_);
        XMLUtils::addChild(doc, node, "DayCounter", dayCountId_);
        XMLUtils::addChild(doc, node, "InterpolationMethod", interpolationMethod_);
        XMLUtils::addChild(doc, node, "Conventions", conventionsId_);
    }

    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);

    if (bootstrapConfig_) {
        XMLUtils::appendNode(node, bootstrapConfig_->toXML(doc));
    }

    return node;
}

void CommodityCurveConfig::populateRequiredCurveIds() {
    if (!baseYieldCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(baseYieldCurveId());
    if (!yieldCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(yieldCurveId());
    if (!basePriceCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Commodity].insert(basePriceCurveId());
}

void CommodityCurveConfig::processSegments(std::vector<PriceSegment> priceSegments) {

    QL_REQUIRE(!priceSegments.empty(), "Need at least one price segment for a Piecewise commodity curve.");

    // Populate the quotes with each segment's quotes. Remove any price segments that have a valid priority value and 
    // add to the priceSegments_ map. What remains in priceSegments are price segments without a priority.
    auto it = priceSegments.begin();
    while (it != priceSegments.end()) {

        // Quotes
        fwdQuotes_.insert(fwdQuotes_.end(), it->quotes().begin(), it->quotes().end());

        // Price segments
        if (it->priority()) {
            unsigned short p = *it->priority();
            QL_REQUIRE(priceSegments_.count(p) == 0, "CommodityCurveConfig: already configured a price segment " <<
                "with priority " << p << " for commodity curve configuration " << curveID() << ".");
            priceSegments_[p] = *it;
            it = priceSegments.erase(it);
        } else {
            it++;
        }

    }

    // Get the current largest priority.
    unsigned short largestPriority = 0;
    if (!priceSegments_.empty()) {
        largestPriority = priceSegments_.rbegin()->first;
    }

    // Very unlikely but check that the priorities entered will not cause an overflow of short int.
    try {
        using namespace boost::safe_numerics;
        safe<unsigned short> test = largestPriority + priceSegments.size();
    } catch (const std::exception&) {
        QL_FAIL("Largest price segment priority (" << largestPriority << ") and number of segments without a " <<
            "priority (" << priceSegments.size() << ") combine to give a value too large for unsigned short.");
    }

    // Now add the price segments without a priority to the end of the map.
    for (const auto& ps : priceSegments) {
        priceSegments_[++largestPriority] = ps;
    }

    quotes_ = fwdQuotes_;
}

} // namespace data
} // namespace ore
