/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ql/errors.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <ored/configuration/yieldcurveconfig.hpp>
#include <ored/utilities/log.hpp>

using QuantLib::Visitor;
using QuantLib::Size;
using ore::data::XMLUtils;
using boost::iequals;

namespace ore {
namespace data {

YieldCurveSegment::Type parseYieldCurveSegement(const string& s) {
    if (iequals(s, "Zero"))
        return YieldCurveSegment::Type::Zero;
    else if (iequals(s, "Zero Spread"))
        return YieldCurveSegment::Type::ZeroSpread;
    else if (iequals(s, "Discount"))
        return YieldCurveSegment::Type::Discount;
    else if (iequals(s, "Deposit"))
        return YieldCurveSegment::Type::Deposit;
    else if (iequals(s, "FRA"))
        return YieldCurveSegment::Type::FRA;
    else if (iequals(s, "Future"))
        return YieldCurveSegment::Type::Future;
    else if (iequals(s, "OIS"))
        return YieldCurveSegment::Type::OIS;
    else if (iequals(s, "Swap"))
        return YieldCurveSegment::Type::Swap;
    else if (iequals(s, "Average OIS"))
        return YieldCurveSegment::Type::AverageOIS;
    else if (iequals(s, "Tenor Basis Swap"))
        return YieldCurveSegment::Type::TenorBasis;
    else if (iequals(s, "Tenor Basis Two Swaps"))
        return YieldCurveSegment::Type::TenorBasisTwo;
    else if (iequals(s, "FX Forward"))
        return YieldCurveSegment::Type::FXForward;
    else if (iequals(s, "Cross Currency Basis Swap"))
        return YieldCurveSegment::Type::CrossCcyBasis;
    else
        QL_FAIL("Yield curve segment type " << s << " not recognized");
}

class SegmentIDGetter : public AcyclicVisitor,
                        public Visitor<YieldCurveSegment>,
                        public Visitor<SimpleYieldCurveSegment>,
                        public Visitor<AverageOISYieldCurveSegment>,
                        public Visitor<TenorBasisYieldCurveSegment>,
                        public Visitor<CrossCcyYieldCurveSegment>,
                        public Visitor<ZeroSpreadedYieldCurveSegment> {

public:
    SegmentIDGetter(const string& curveID, set<string>& requiredYieldCurveIDs)
        : curveID_(curveID), requiredYieldCurveIDs_(requiredYieldCurveIDs) {}

    void visit(YieldCurveSegment&);
    void visit(SimpleYieldCurveSegment& s);
    void visit(AverageOISYieldCurveSegment& s);
    void visit(TenorBasisYieldCurveSegment& s);
    void visit(CrossCcyYieldCurveSegment& s);
    void visit(ZeroSpreadedYieldCurveSegment& s);

private:
    string curveID_;
    set<string>& requiredYieldCurveIDs_;
};

void SegmentIDGetter::visit(YieldCurveSegment&) {
    // Do nothing
}

void SegmentIDGetter::visit(SimpleYieldCurveSegment& s) {
    string aCurveID = s.projectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
}

void SegmentIDGetter::visit(AverageOISYieldCurveSegment& s) {
    string aCurveID = s.projectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
}

void SegmentIDGetter::visit(TenorBasisYieldCurveSegment& s) {
    string aCurveID = s.shortProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
    aCurveID = s.longProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
}

void SegmentIDGetter::visit(CrossCcyYieldCurveSegment& s) {
    string aCurveID = s.foreignDiscountCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
    aCurveID = s.domesticProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
    aCurveID = s.foreignProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
}

void SegmentIDGetter::visit(ZeroSpreadedYieldCurveSegment& s) {
    string aCurveID = s.referenceCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredYieldCurveIDs_.insert(aCurveID);
    }
}

// YieldCurveConfig
YieldCurveConfig::YieldCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                   const string& discountCurveID,
                                   const vector<boost::shared_ptr<YieldCurveSegment>>& curveSegments,
                                   const string& interpolationVariable, const string& interpolationMethod,
                                   const string& zeroDayCounter, bool extrapolation, Real tolerance)
    : CurveConfig(curveID, curveDescription), currency_(currency), discountCurveID_(discountCurveID),
      curveSegments_(curveSegments), interpolationVariable_(interpolationVariable),
      interpolationMethod_(interpolationMethod), zeroDayCounter_(zeroDayCounter), extrapolation_(extrapolation),
      tolerance_(tolerance) {
    populateRequiredYieldCurveIDs();
}

void YieldCurveConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "YieldCurve");

    // Read in the mandatory nodes.
    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    discountCurveID_ = XMLUtils::getChildValue(node, "DiscountCurve", true);

    // Read in the segments.
    XMLNode* segmentsNode = XMLUtils::getChildNode(node, "Segments");
    if (segmentsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(segmentsNode); child; child = XMLUtils::getNextSibling(child)) {

            boost::shared_ptr<YieldCurveSegment> segment;
            string childName = XMLUtils::getNodeName(child);

            if (childName == "Direct") {
                segment.reset(new DirectYieldCurveSegment());
            } else if (childName == "Simple") {
                segment.reset(new SimpleYieldCurveSegment());
            } else if (childName == "AverageOIS") {
                segment.reset(new AverageOISYieldCurveSegment());
            } else if (childName == "TenorBasis") {
                segment.reset(new TenorBasisYieldCurveSegment());
            } else if (childName == "CrossCurrency") {
                segment.reset(new CrossCcyYieldCurveSegment());
            } else if (childName == "ZeroSpread") {
                segment.reset(new ZeroSpreadedYieldCurveSegment());
            } else {
                QL_FAIL("Yield curve segment node name not recognized.");
            }

            if (segment) {
                try {
                    segment->fromXML(child);
                } catch (std::exception& ex) {
                    ALOG("Exception parsing yield curve segment XML Node, name = " << childName << " and curveID = "
                                                                                   << curveID_ << " : " << ex.what());
                }
            } else {
                LOG("Unable to build yield curve segment for name = " << childName << " and curveID = " << curveID_);
            }
            curveSegments_.push_back(segment);
        }
    } else {
        QL_FAIL("No Segments node in XML doc for yield curve ID = " << curveID_);
    }

    // Read in the optional nodes.

    // Empty strings if not there (or if there and empty).
    interpolationVariable_ = XMLUtils::getChildValue(node, "InterpolationVariable", false);
    interpolationMethod_ = XMLUtils::getChildValue(node, "InterpolationMethod", false);
    zeroDayCounter_ = XMLUtils::getChildValue(node, "YieldCurveDayCounter", false);

    // Add hardcoded defaults for now.
    if (interpolationVariable_.empty()) {
        interpolationVariable_ = "Discount";
    }
    if (interpolationMethod_.empty()) {
        interpolationMethod_ = interpolationVariable_ == "Zero" ? "Linear" : "LogLinear";
    }
    if (zeroDayCounter_.empty()) {
        zeroDayCounter_ = "A365";
    }
    XMLNode* nodeToTest = XMLUtils::getChildNode(node, "Extrapolation");
    if (nodeToTest) {
        extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation", false);
    } else {
        extrapolation_ = true;
    }
    nodeToTest = XMLUtils::getChildNode(node, "Tolerance");
    if (nodeToTest) {
        tolerance_ = XMLUtils::getChildValueAsDouble(node, "Tolerance", false);
    } else {
        tolerance_ = 1.0e-12;
    }

    populateRequiredYieldCurveIDs();
}

XMLNode* YieldCurveConfig::toXML(XMLDocument& doc) {
    // Allocate a node.
    XMLNode* node = doc.allocNode("YieldCurve");

    // Add the mandatory members.
    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChild(doc, node, "DiscountCurve", discountCurveID_);

    // Add the segments node.
    XMLNode* segmentsNode = doc.allocNode("Segments");
    XMLUtils::appendNode(node, segmentsNode);
    for (Size i = 0; i < curveSegments_.size(); ++i) {
        XMLUtils::appendNode(segmentsNode, curveSegments_[i]->toXML(doc));
    }

    // Add the defaultable elements.
    XMLUtils::addChild(doc, node, "InterpolationVariable", interpolationVariable_);
    XMLUtils::addChild(doc, node, "InterpolationMethod", interpolationMethod_);
    XMLUtils::addChild(doc, node, "YieldCurveDayCounter", zeroDayCounter_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    XMLUtils::addChild(doc, node, "Tolerance", tolerance_);

    return node;
}

void YieldCurveConfig::populateRequiredYieldCurveIDs() {

    if (!requiredYieldCurveIDs_.empty()) {
        requiredYieldCurveIDs_.clear();
    }

    if (curveID_ != discountCurveID_ && !discountCurveID_.empty()) {
        requiredYieldCurveIDs_.insert(discountCurveID_);
    }

    SegmentIDGetter segmentIDGetter(curveID_, requiredYieldCurveIDs_);
    for (Size i = 0; i < curveSegments_.size(); i++) {
        curveSegments_[i]->accept(segmentIDGetter);
    }
}

YieldCurveSegment::YieldCurveSegment(const string& typeID, const string& conventionsID)
    : type_(parseYieldCurveSegement(typeID)), typeID_(typeID), conventionsID_(conventionsID) {}

void YieldCurveSegment::fromXML(XMLNode* node) {
    typeID_ = XMLUtils::getChildValue(node, "Type", true);
    type_ = parseYieldCurveSegement(typeID_);
    conventionsID_ = XMLUtils::getChildValue(node, "Conventions", false);
}

XMLNode* YieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Segment");
    XMLUtils::addChild(doc, node, "Type", typeID_);
    XMLUtils::addChild(doc, node, "Conventions", conventionsID_);
    return node;
}

void YieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<YieldCurveSegment>* v1 = dynamic_cast<Visitor<YieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        QL_FAIL("Not a YieldCurveSegment visitor.");
}

DirectYieldCurveSegment::DirectYieldCurveSegment(const string& typeID, const string& conventionsID,
                                                 const vector<string>& quotes)
    : YieldCurveSegment(typeID, conventionsID), quotes_(quotes) {}

void DirectYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Direct");
    YieldCurveSegment::fromXML(node);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
}

XMLNode* DirectYieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "Direct");
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    return node;
}

void DirectYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<DirectYieldCurveSegment>* v1 = dynamic_cast<Visitor<DirectYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

SimpleYieldCurveSegment::SimpleYieldCurveSegment(const string& typeID, const string& conventionsID,
                                                 const vector<string>& quotes, const string& projectionCurveID)
    : YieldCurveSegment(typeID, conventionsID), quotes_(quotes), projectionCurveID_(projectionCurveID) {}

void SimpleYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Simple");
    YieldCurveSegment::fromXML(node);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    projectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurve", false);
}

XMLNode* SimpleYieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "Simple");
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    if (!projectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurve", projectionCurveID_);
    return node;
}

void SimpleYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<SimpleYieldCurveSegment>* v1 = dynamic_cast<Visitor<SimpleYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

AverageOISYieldCurveSegment::AverageOISYieldCurveSegment(const string& typeID, const string& conventionsID,
                                                         const vector<pair<string, string>>& quotes,
                                                         const string& projectionCurveID)
    : YieldCurveSegment(typeID, conventionsID), quotes_(quotes), projectionCurveID_(projectionCurveID) {}

void AverageOISYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AverageOIS");
    YieldCurveSegment::fromXML(node);
    projectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurve", false);

    // Read the Quotes node.
    XMLNode* quotesNode = XMLUtils::getChildNode(node, "Quotes");
    if (quotesNode) {
        for (XMLNode* child = XMLUtils::getChildNode(quotesNode, "CompositeQuote"); child;
             child = XMLUtils::getNextSibling(child)) {
            string rateQuote = XMLUtils::getChildValue(child, "RateQuote", true);
            string spreadQuote = XMLUtils::getChildValue(child, "SpreadQuote", true);
            quotes_.push_back(make_pair(rateQuote, spreadQuote));
        }
    } else {
        QL_FAIL("No Quotes in segment. Remove segment or add quotes.");
    }
}

XMLNode* AverageOISYieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "AverageOIS");
    if (!projectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurve", projectionCurveID_);

    // Add the Quotes node.
    XMLNode* quotesNode = doc.allocNode("Quotes");
    XMLUtils::appendNode(node, quotesNode);
    for (Size i = 0; i < quotes_.size(); ++i) {
        XMLNode* compositeQuoteNode = doc.allocNode("CompositeQuote");
        XMLUtils::addChild(doc, compositeQuoteNode, "RateQuote", quotes_[i].first);
        XMLUtils::addChild(doc, compositeQuoteNode, "SpreadQuote", quotes_[i].second);
        XMLUtils::appendNode(quotesNode, compositeQuoteNode);
    }

    return node;
}

void AverageOISYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<AverageOISYieldCurveSegment>* v1 = dynamic_cast<Visitor<AverageOISYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

TenorBasisYieldCurveSegment::TenorBasisYieldCurveSegment(const string& typeID, const string& conventionsID,
                                                         const vector<string>& quotes,
                                                         const string& shortProjectionCurveID,
                                                         const string& longProjectionCurveID)
    : YieldCurveSegment(typeID, conventionsID), quotes_(quotes), shortProjectionCurveID_(shortProjectionCurveID),
      longProjectionCurveID_(longProjectionCurveID) {}

void TenorBasisYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "TenorBasis");
    YieldCurveSegment::fromXML(node);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    shortProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveShort", false);
    longProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveLong", false);
}

XMLNode* TenorBasisYieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "TenorBasis");
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    if (!shortProjectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurveShort", shortProjectionCurveID_);
    if (!longProjectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurveLong", longProjectionCurveID_);
    return node;
}

void TenorBasisYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<TenorBasisYieldCurveSegment>* v1 = dynamic_cast<Visitor<TenorBasisYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

CrossCcyYieldCurveSegment::CrossCcyYieldCurveSegment(const string& type, const string& conventionsID,
                                                     const vector<string>& quotes, const string& spotRateID,
                                                     const string& foreignDiscountCurveID,
                                                     const string& domesticProjectionCurveID,
                                                     const string& foreignProjectionCurveID)
    : YieldCurveSegment(type, conventionsID), quotes_(quotes), spotRateID_(spotRateID),
      foreignDiscountCurveID_(foreignDiscountCurveID), domesticProjectionCurveID_(domesticProjectionCurveID),
      foreignProjectionCurveID_(foreignProjectionCurveID) {}

void CrossCcyYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CrossCurrency");
    YieldCurveSegment::fromXML(node);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    spotRateID_ = XMLUtils::getChildValue(node, "SpotRate", true);
    foreignDiscountCurveID_ = XMLUtils::getChildValue(node, "DiscountCurve", true);
    domesticProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveDomestic", false);
    foreignProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveForeign", false);
}

XMLNode* CrossCcyYieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "CrossCurrency");
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    XMLUtils::addChild(doc, node, "SpotRate", spotRateID_);
    XMLUtils::addChild(doc, node, "DiscountCurve", foreignDiscountCurveID_);
    if (!domesticProjectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurveDomestic", domesticProjectionCurveID_);
    if (!foreignProjectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurveForeign", foreignProjectionCurveID_);
    return node;
}

void CrossCcyYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<CrossCcyYieldCurveSegment>* v1 = dynamic_cast<Visitor<CrossCcyYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

ZeroSpreadedYieldCurveSegment::ZeroSpreadedYieldCurveSegment(const string& typeID, const string& conventionsID,
                                                             const vector<string>& quotes,
                                                             const string& referenceCurveID)
    : YieldCurveSegment(typeID, conventionsID), quotes_(quotes), referenceCurveID_(referenceCurveID) {}

void ZeroSpreadedYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ZeroSpread");
    YieldCurveSegment::fromXML(node);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    referenceCurveID_ = XMLUtils::getChildValue(node, "ReferenceCurve", false);
}

XMLNode* ZeroSpreadedYieldCurveSegment::toXML(XMLDocument& doc) {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "ZeroSpread");
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    XMLUtils::addChild(doc, node, "ReferenceCurve", referenceCurveID_);
    return node;
}

void ZeroSpreadedYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<ZeroSpreadedYieldCurveSegment>* v1 = dynamic_cast<Visitor<ZeroSpreadedYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}
} // namespace data
} // namespace ore
