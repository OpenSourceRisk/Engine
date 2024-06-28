/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2023 Oleg Kulkov
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

#include <ored/configuration/yieldcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string/predicate.hpp>

using boost::iequals;
using ore::data::XMLUtils;
using QuantLib::Size;
using QuantLib::Visitor;

namespace ore {
namespace data {

YieldCurveSegment::Type parseYieldCurveSegment(const string& s) {
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
    else if (iequals(s, "BMA Basis Swap"))
        return YieldCurveSegment::Type::BMABasis;
    else if (iequals(s, "FX Forward"))
        return YieldCurveSegment::Type::FXForward;
    else if (iequals(s, "Cross Currency Basis Swap"))
        return YieldCurveSegment::Type::CrossCcyBasis;
    else if (iequals(s, "Cross Currency Fix Float Swap"))
        return YieldCurveSegment::Type::CrossCcyFixFloat;
    else if (iequals(s, "Discount Ratio"))
        return YieldCurveSegment::Type::DiscountRatio;
    else if (iequals(s, "FittedBond"))
        return YieldCurveSegment::Type::FittedBond;
    else if (iequals(s, "Yield Plus Default"))
        return YieldCurveSegment::Type::YieldPlusDefault;
    else if (iequals(s, "Weighted Average"))
        return YieldCurveSegment::Type::WeightedAverage;
    else if (iequals(s, "Ibor Fallback"))
        return YieldCurveSegment::Type::IborFallback;
    else if (iequals(s, "Bond Yield Shifted"))
        return YieldCurveSegment::Type::BondYieldShifted;
    QL_FAIL("Yield curve segment type " << s << " not recognized");
}

class SegmentIDGetter : public AcyclicVisitor,
                        public Visitor<YieldCurveSegment>,
                        public Visitor<SimpleYieldCurveSegment>,
                        public Visitor<AverageOISYieldCurveSegment>,
                        public Visitor<TenorBasisYieldCurveSegment>,
                        public Visitor<CrossCcyYieldCurveSegment>,
                        public Visitor<ZeroSpreadedYieldCurveSegment>,
                        public Visitor<DiscountRatioYieldCurveSegment>,
                        public Visitor<FittedBondYieldCurveSegment>,
                        public Visitor<WeightedAverageYieldCurveSegment>,
                        public Visitor<YieldPlusDefaultYieldCurveSegment>,
                        public Visitor<BondYieldShiftedYieldCurveSegment>,
                        public Visitor<IborFallbackCurveSegment> {

public:
    SegmentIDGetter(const string& curveID, map<CurveSpec::CurveType, set<string>>& requiredCurveIds)
        : curveID_(curveID), requiredCurveIds_(requiredCurveIds) {}

    void visit(YieldCurveSegment&) override;
    void visit(SimpleYieldCurveSegment& s) override;
    void visit(AverageOISYieldCurveSegment& s) override;
    void visit(TenorBasisYieldCurveSegment& s) override;
    void visit(CrossCcyYieldCurveSegment& s) override;
    void visit(ZeroSpreadedYieldCurveSegment& s) override;
    void visit(DiscountRatioYieldCurveSegment& s) override;
    void visit(FittedBondYieldCurveSegment& s) override;
    void visit(WeightedAverageYieldCurveSegment& s) override;
    void visit(YieldPlusDefaultYieldCurveSegment& s) override;
    void visit(BondYieldShiftedYieldCurveSegment& s) override;
    void visit(IborFallbackCurveSegment& s) override;

private:
    string curveID_;
    map<CurveSpec::CurveType, set<string>>& requiredCurveIds_;
};

void SegmentIDGetter::visit(YieldCurveSegment&) {
    // Do nothing
}

void SegmentIDGetter::visit(SimpleYieldCurveSegment& s) {
    string aCurveID = s.projectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
}

void SegmentIDGetter::visit(AverageOISYieldCurveSegment& s) {
    string aCurveID = s.projectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
}

void SegmentIDGetter::visit(TenorBasisYieldCurveSegment& s) {
    string aCurveID = s.receiveProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
    aCurveID = s.payProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
}

void SegmentIDGetter::visit(CrossCcyYieldCurveSegment& s) {
    string aCurveID = s.foreignDiscountCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
    aCurveID = s.domesticProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
    aCurveID = s.foreignProjectionCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
}

void SegmentIDGetter::visit(ZeroSpreadedYieldCurveSegment& s) {
    string aCurveID = s.referenceCurveID();
    if (curveID_ != aCurveID && !aCurveID.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID);
    }
}

void SegmentIDGetter::visit(DiscountRatioYieldCurveSegment& s) {
    if (curveID_ != s.baseCurveId() && !s.baseCurveId().empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(s.baseCurveId());
    }
    if (curveID_ != s.numeratorCurveId() && !s.numeratorCurveId().empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(s.numeratorCurveId());
    }
    if (curveID_ != s.denominatorCurveId() && !s.denominatorCurveId().empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(s.denominatorCurveId());
    }
}

void SegmentIDGetter::visit(FittedBondYieldCurveSegment& s) {
    for (auto const& c : s.iborIndexCurves())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(c.second);
}

void SegmentIDGetter::visit(BondYieldShiftedYieldCurveSegment& s) {
    for (auto const& c : s.iborIndexCurves())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(c.second);
    requiredCurveIds_[CurveSpec::CurveType::Yield].insert(s.referenceCurveID());
}

void SegmentIDGetter::visit(WeightedAverageYieldCurveSegment& s) {
    string aCurveID1 = s.referenceCurveID1();
    string aCurveID2 = s.referenceCurveID2();
    requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID1);
    requiredCurveIds_[CurveSpec::CurveType::Yield].insert(aCurveID2);
}

void SegmentIDGetter::visit(YieldPlusDefaultYieldCurveSegment& s) {
    requiredCurveIds_[CurveSpec::CurveType::Yield].insert(s.referenceCurveID());
    for (auto const& i : s.defaultCurveIDs()) {
        requiredCurveIds_[CurveSpec::CurveType::Default].insert(parseCurveSpec(i)->curveConfigID());
    }
}

void SegmentIDGetter::visit(IborFallbackCurveSegment& s) {
    requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(s.rfrCurve())->curveConfigID());
}

// YieldCurveConfig
YieldCurveConfig::YieldCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                   const string& discountCurveID,
                                   const vector<QuantLib::ext::shared_ptr<YieldCurveSegment>>& curveSegments,
                                   const string& interpolationVariable, const string& interpolationMethod,
                                   const string& zeroDayCounter, bool extrapolation,
                                   const BootstrapConfig& bootstrapConfig, const Size mixedInterpolationCutoff)
    : CurveConfig(curveID, curveDescription), currency_(currency), discountCurveID_(discountCurveID),
      curveSegments_(curveSegments), interpolationVariable_(interpolationVariable),
      interpolationMethod_(interpolationMethod), zeroDayCounter_(zeroDayCounter), extrapolation_(extrapolation),
      bootstrapConfig_(bootstrapConfig), mixedInterpolationCutoff_(mixedInterpolationCutoff) {
    populateRequiredCurveIds();
}

const vector<string>& YieldCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        bool addedFxSpot = false;
        for (auto c : curveSegments_) {
            for (auto segmentQuote : c->quotes())
                quotes_.push_back(segmentQuote.first);

            // Check if the segment is a CrossCcyYieldCurveSegment and add the FX spot rate to the
            // set of quotes needed for the YieldCurveConfig if it has not already been added.
            if (auto xccySegment = QuantLib::ext::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(c)) {
                if (!addedFxSpot) {
                    quotes_.push_back(xccySegment->spotRateID());
                    // we add the inverted pair as well, because the original pair might get removed from the market
                    // data loader if both are present in the input market data
                    if (auto md = boost::dynamic_pointer_cast<FXSpotQuote>(
                            parseMarketDatum(Date(), xccySegment->spotRateID(), 1.0))) {
                        quotes_.push_back("FX/RATE/" + md->ccy() + "/" + md->unitCcy());
                    }
                }
            }
        }
    }
    return quotes_;
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

            QuantLib::ext::shared_ptr<YieldCurveSegment> segment;
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
            } else if (childName == "DiscountRatio") {
                segment.reset(new DiscountRatioYieldCurveSegment());
            } else if (childName == "FittedBond") {
                segment.reset(new FittedBondYieldCurveSegment());
            } else if (childName == "BondYieldShifted") {
                segment.reset(new BondYieldShiftedYieldCurveSegment());
            } else if (childName == "WeightedAverage") {
                segment.reset(new WeightedAverageYieldCurveSegment());
            } else if (childName == "YieldPlusDefault") {
                segment.reset(new YieldPlusDefaultYieldCurveSegment());
            } else if(childName == "IborFallback"){
                segment.reset(new IborFallbackCurveSegment());
            } else {
                QL_FAIL("Yield curve segment node name '" << childName << "' not recognized.");
            }

            if (segment) {
                try {
                    segment->fromXML(child);
                } catch (std::exception& ex) {
                    QL_FAIL("Exception parsing yield curve segment XML Node, name = "
                            << childName << " and curveID = " << curveID_ << " : " << ex.what());
                }
            } else {
                QL_FAIL("Unable to build yield curve segment for name = " << childName
                                                                          << " and curveID = " << curveID_);
            }
            curveSegments_.push_back(segment);
        }
    } else {
        QL_FAIL("No Segments node in XML doc for yield curve ID = " << curveID_);
    }

    // Read in the optional nodes.

    // Empty strings if not there (or if there and empty).
    interpolationVariable_ = XMLUtils::getChildValue(node, "InterpolationVariable", false, "Discount");
    interpolationMethod_ = XMLUtils::getChildValue(node, "InterpolationMethod", false, interpolationVariable_ == "Zero" ? "Linear" : "LogLinear");
    mixedInterpolationCutoff_ = XMLUtils::getChildValueAsInt(node, "MixedInterpolationCutoff", false, 1);
    zeroDayCounter_ = XMLUtils::getChildValue(node, "YieldCurveDayCounter", false, "A365");
    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation", false, true);

    // Optional bootstrap configuration
    if (XMLNode* n = XMLUtils::getChildNode(node, "BootstrapConfig")) {
        bootstrapConfig_.fromXML(n);
    }

    // Tolerance is deprecated in favour of Accuracy in BootstrapConfig. However, if it is
    // still provided, use it as the accuracy and global accuracy in the bootstrap.
    if (XMLUtils::getChildNode(node, "Tolerance")) {
        Real accuracy = XMLUtils::getChildValueAsDouble(node, "Tolerance", false);
        bootstrapConfig_ =
            BootstrapConfig(accuracy, accuracy, bootstrapConfig_.dontThrow(), bootstrapConfig_.maxAttempts(),
                            bootstrapConfig_.maxFactor(), bootstrapConfig_.minFactor());
    }

    populateRequiredCurveIds();
}

XMLNode* YieldCurveConfig::toXML(XMLDocument& doc) const {
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
    XMLUtils::addChild(doc, node, "MixedInterpolationCutoff", (int)mixedInterpolationCutoff_);
    XMLUtils::addChild(doc, node, "YieldCurveDayCounter", zeroDayCounter_);
    XMLUtils::addChild(doc, node, "Tolerance", bootstrapConfig_.accuracy());
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    XMLUtils::appendNode(node, bootstrapConfig_.toXML(doc));

    return node;
}

void YieldCurveConfig::populateRequiredCurveIds() {

    requiredCurveIds_.clear();

    if (curveID_ != discountCurveID_ && !discountCurveID_.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(discountCurveID_);
    }

    SegmentIDGetter segmentIDGetter(curveID_, requiredCurveIds_);
    for (Size i = 0; i < curveSegments_.size(); i++) {
        curveSegments_[i]->accept(segmentIDGetter);
    }
}

YieldCurveSegment::YieldCurveSegment(const string& typeID, const string& conventionsID,
                                     const vector<string>& quoteNames)
    : type_(parseYieldCurveSegment(typeID)), typeID_(typeID), conventionsID_(conventionsID) {
    for (auto q : quoteNames)
        quotes_.emplace_back(quote(q));
}

void YieldCurveSegment::fromXML(XMLNode* node) {
    typeID_ = XMLUtils::getChildValue(node, "Type", true);
    string name = XMLUtils::getNodeName(node);

	// Check if curve type is valid for the given segment name
    std::map<std::string, std::list<std::string>> validSegmentTypes = {
        {"Direct", {"Zero", "Discount"}},
        {"Simple", {"Deposit", "FRA", "Future", "OIS", "Swap", "BMA Basis Swap"}},
        {"AverageOIS", {"Average OIS"}},
        {"TenorBasis", {"Tenor Basis Swap", "Tenor Basis Two Swaps"}},
        {"CrossCurrency", {"FX Forward", "Cross Currency Basis Swap", "Cross Currency Fix Float Swap"}},
        {"ZeroSpread", {"Zero Spread"}},
        {"FittedBond", {"FittedBond"}},
        {"YieldPlusDefault", {"Yield Plus Default"}},
        {"WeightedAverage", {"Weighted Average"}},
        {"DiscountRatio", {"Discount Ratio"}},
        {"IborFallback", {"Ibor Fallback"}},
        {"BondYieldShifted", {"Bond Yield Shifted"}}
        };

    std::list<std::string> validTypes = validSegmentTypes.at(name);
    QL_REQUIRE(std::find(validTypes.begin(), validTypes.end(), typeID_) != validTypes.end(),
               "The curve type " << typeID_ << " is not a valid " << name << " curve segment type");

    quotes_.clear();
    if (name == "AverageOIS") {
        XMLNode* quotesNode = XMLUtils::getChildNode(node, "Quotes");
        if (quotesNode) {
            for (XMLNode* child = XMLUtils::getChildNode(quotesNode, "CompositeQuote"); child;
                 child = XMLUtils::getNextSibling(child)) {
                quotes_.push_back(quote(XMLUtils::getChildValue(child, "RateQuote", true)));
                quotes_.push_back(quote(XMLUtils::getChildValue(child, "SpreadQuote", true)));
            }
        } else {
            QL_FAIL("No Quotes in segment. Remove segment or add quotes.");
        }
    } else {
        XMLNode* quotesNode = XMLUtils::getChildNode(node, "Quotes");
        if (quotesNode) {
            for (auto n : XMLUtils::getChildrenNodes(quotesNode, "Quote")) {
                string attr = XMLUtils::getAttribute(n, "optional"); // return "" if not present
                bool opt = (!attr.empty() && parseBool(attr));
                quotes_.emplace_back(quote(XMLUtils::getNodeValue(n), opt));
            }
        }
    }
    type_ = parseYieldCurveSegment(typeID_);
    conventionsID_ = XMLUtils::getChildValue(node, "Conventions", false);
    pillarChoice_ = parsePillarChoice(XMLUtils::getChildValue(node, "PillarChoice", false, "LastRelevantDate"));
    QL_REQUIRE(pillarChoice_ == QuantLib::Pillar::MaturityDate || pillarChoice_ == QuantLib::Pillar::LastRelevantDate,
               "PillarChoice " << pillarChoice_ << " not supported, expected MaturityDate, LastRelevantDate");
    priority_ = XMLUtils::getChildValueAsInt(node, "Priority", false, 0);
    minDistance_ = XMLUtils::getChildValueAsInt(node, "MinDistance", false, 1);
}

XMLNode* YieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Segment");
    XMLUtils::addChild(doc, node, "Type", typeID_);
    if (!quotes_.empty()) {
        XMLNode* quotesNode = doc.allocNode("Quotes");
        // Special case handling for AverageOIS where the quotes are stored as pairs
        // Spread and Rate.
        if (type_ == YieldCurveSegment::Type::AverageOIS) {
            QL_REQUIRE(quotes_.size() % 2 == 0, "Invalid quotes vector should be even");
            for (Size i = 0; i < quotes_.size(); i = i + 2) {
                string rateQuote = quotes_[i].first;
                string spreadQuote = quotes_[i + 1].first;

                XMLNode* compositeQuoteNode = doc.allocNode("CompositeQuote");
                XMLUtils::addChild(doc, compositeQuoteNode, "SpreadQuote", spreadQuote);
                XMLUtils::addChild(doc, compositeQuoteNode, "RateQuote", rateQuote);
                XMLUtils::appendNode(quotesNode, compositeQuoteNode);
            }
        } else {
            for (auto q : quotes_) {
                XMLNode* qNode = doc.allocNode("Quote", q.first);
                if (q.second)
                    XMLUtils::addAttribute(doc, qNode, "optional", "true");
                XMLUtils::appendNode(quotesNode, qNode);
            }
        }
        XMLUtils::appendNode(node, quotesNode);
    }

    if (!conventionsID_.empty())
        XMLUtils::addChild(doc, node, "Conventions", conventionsID_);
    XMLUtils::addChild(doc, node, "PillarChoice", ore::data::to_string(pillarChoice_));
    XMLUtils::addChild(doc, node, "Priority", (int)priority_);
    XMLUtils::addChild(doc, node, "MinDistance", (int)minDistance_);
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
    : YieldCurveSegment(typeID, conventionsID, quotes) {}

void DirectYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Direct");
    YieldCurveSegment::fromXML(node);
}

XMLNode* DirectYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "Direct");
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
    : YieldCurveSegment(typeID, conventionsID, quotes), projectionCurveID_(projectionCurveID) {}

void SimpleYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Simple");
    YieldCurveSegment::fromXML(node);
    projectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurve", false);
}

XMLNode* SimpleYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "Simple");
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
                                                         const vector<string>& quotes, const string& projectionCurveID)
    : YieldCurveSegment(typeID, conventionsID, quotes), projectionCurveID_(projectionCurveID) {}

void AverageOISYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AverageOIS");
    YieldCurveSegment::fromXML(node);
    projectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurve", false);
}

XMLNode* AverageOISYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "AverageOIS");
    if (!projectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurve", projectionCurveID_);
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
                                                         const string& receiveProjectionCurveID,
                                                         const string& payProjectionCurveID)
    : YieldCurveSegment(typeID, conventionsID, quotes), receiveProjectionCurveID_(receiveProjectionCurveID),
      payProjectionCurveID_(payProjectionCurveID) {}

void TenorBasisYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "TenorBasis");
    YieldCurveSegment::fromXML(node);
    receiveProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveReceive", false);
    payProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurvePay", false);

    // handle deprecated fields...
    XMLNode* projectionCurveShort = XMLUtils::getChildNode(node, "ProjectionCurveShort");
    if (projectionCurveShort) {
        ALOG("TenorBasisYieldCurveSegment: ProjectionCurveShort is deprecated, fill empty receiveProjectionCurveID");
        if (receiveProjectionCurveID_.empty())
            receiveProjectionCurveID_ = XMLUtils::getNodeValue(projectionCurveShort);
    }

    XMLNode* projectionCurveLong = XMLUtils::getChildNode(node, "ProjectionCurveLong");
    if (projectionCurveLong) {
        ALOG("TenorBasisYieldCurveSegment: projectionCurveLong is deprecated, fill empty payProjectionCurveID");
        if (payProjectionCurveID_.empty())
            payProjectionCurveID_ = XMLUtils::getNodeValue(projectionCurveLong);
    }
}

XMLNode* TenorBasisYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "TenorBasis");
    if (!payProjectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurvePay", payProjectionCurveID_);
    if (!receiveProjectionCurveID_.empty())
        XMLUtils::addChild(doc, node, "ProjectionCurveReceive", receiveProjectionCurveID_);
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
    : YieldCurveSegment(type, conventionsID, quotes), spotRateID_(spotRateID),
      foreignDiscountCurveID_(foreignDiscountCurveID), domesticProjectionCurveID_(domesticProjectionCurveID),
      foreignProjectionCurveID_(foreignProjectionCurveID) {}

void CrossCcyYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CrossCurrency");
    YieldCurveSegment::fromXML(node);
    foreignDiscountCurveID_ = XMLUtils::getChildValue(node, "DiscountCurve", true);
    spotRateID_ = XMLUtils::getChildValue(node, "SpotRate", true);
    domesticProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveDomestic", false);
    foreignProjectionCurveID_ = XMLUtils::getChildValue(node, "ProjectionCurveForeign", false);
}

XMLNode* CrossCcyYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "CrossCurrency");
    XMLUtils::addChild(doc, node, "DiscountCurve", foreignDiscountCurveID_);
    XMLUtils::addChild(doc, node, "SpotRate", spotRateID_);
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
    : YieldCurveSegment(typeID, conventionsID, quotes), referenceCurveID_(referenceCurveID) {}

void ZeroSpreadedYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ZeroSpread");
    YieldCurveSegment::fromXML(node);
    referenceCurveID_ = XMLUtils::getChildValue(node, "ReferenceCurve", false);
}

XMLNode* ZeroSpreadedYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "ZeroSpread");
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

DiscountRatioYieldCurveSegment::DiscountRatioYieldCurveSegment(
    const string& typeId, const string& baseCurveId, const string& baseCurveCurrency, const string& numeratorCurveId,
    const string& numeratorCurveCurrency, const string& denominatorCurveId, const string& denominatorCurveCurrency)
    : YieldCurveSegment(typeId, "", vector<string>()), baseCurveId_(baseCurveId), baseCurveCurrency_(baseCurveCurrency),
      numeratorCurveId_(numeratorCurveId), numeratorCurveCurrency_(numeratorCurveCurrency),
      denominatorCurveId_(denominatorCurveId), denominatorCurveCurrency_(denominatorCurveCurrency) {}

void DiscountRatioYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "DiscountRatio");
    YieldCurveSegment::fromXML(node);

    XMLNode* aNode = XMLUtils::getChildNode(node, "BaseCurve");
    QL_REQUIRE(aNode, "Discount ratio segment needs a BaseCurve node");
    baseCurveId_ = XMLUtils::getNodeValue(aNode);
    baseCurveCurrency_ = XMLUtils::getAttribute(aNode, "currency");

    aNode = XMLUtils::getChildNode(node, "NumeratorCurve");
    QL_REQUIRE(aNode, "Discount ratio segment needs a NumeratorCurve node");
    numeratorCurveId_ = XMLUtils::getNodeValue(aNode);
    numeratorCurveCurrency_ = XMLUtils::getAttribute(aNode, "currency");

    aNode = XMLUtils::getChildNode(node, "DenominatorCurve");
    QL_REQUIRE(aNode, "Discount ratio segment needs a DenominatorCurve node");
    denominatorCurveId_ = XMLUtils::getNodeValue(aNode);
    denominatorCurveCurrency_ = XMLUtils::getAttribute(aNode, "currency");
}

XMLNode* DiscountRatioYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "DiscountRatio");

    XMLNode* baseCurveNode = doc.allocNode("BaseCurve", baseCurveId_);
    XMLUtils::appendNode(node, baseCurveNode);
    XMLUtils::addAttribute(doc, baseCurveNode, "currency", baseCurveCurrency_);

    XMLNode* numCurveNode = doc.allocNode("NumeratorCurve", numeratorCurveId_);
    XMLUtils::appendNode(node, numCurveNode);
    XMLUtils::addAttribute(doc, numCurveNode, "currency", numeratorCurveCurrency_);

    XMLNode* denCurveNode = doc.allocNode("DenominatorCurve", denominatorCurveId_);
    XMLUtils::appendNode(node, denCurveNode);
    XMLUtils::addAttribute(doc, denCurveNode, "currency", denominatorCurveCurrency_);

    return node;
}

void DiscountRatioYieldCurveSegment::accept(AcyclicVisitor& v) {
    if (Visitor<DiscountRatioYieldCurveSegment>* v1 = dynamic_cast<Visitor<DiscountRatioYieldCurveSegment>*>(&v))
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

FittedBondYieldCurveSegment::FittedBondYieldCurveSegment(const string& typeID, const vector<string>& quotes,
                                                         const map<string, string>& iborIndexCurves,
                                                         const bool extrapolateFlat)
    : YieldCurveSegment(typeID, "", quotes), iborIndexCurves_(iborIndexCurves), extrapolateFlat_(extrapolateFlat) {}

void FittedBondYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "FittedBond");
    YieldCurveSegment::fromXML(node);

    vector<string> iborIndexNames;
    vector<string> iborIndexCurves = XMLUtils::getChildrenValuesWithAttributes(
        node, "IborIndexCurves", "IborIndexCurve", "iborIndex", iborIndexNames, false);
    for (Size i = 0; i < iborIndexNames.size(); ++i) {
        iborIndexCurves_[iborIndexNames[i]] = iborIndexCurves[i];
    }

    if (auto n = XMLUtils::getChildNode(node, "ExtrapolateFlat")) {
        extrapolateFlat_ = parseBool(XMLUtils::getNodeValue(n));
    } else {
        extrapolateFlat_ = false;
    }
}

XMLNode* FittedBondYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "FittedBond");

    vector<string> iborIndexNames;
    vector<string> iborIndexCurves;
    for (auto const& c : iborIndexCurves_) {
        iborIndexNames.push_back(c.first);
        iborIndexCurves.push_back(c.second);
    }
    XMLUtils::addChildrenWithAttributes(doc, node, "IborIndexCurves", "IborIndexCurve", iborIndexCurves, "iborIndex",
                                        iborIndexNames);

    XMLUtils::addChild(doc, node, "ExtrapolateFlat", extrapolateFlat_);
    return node;
}

void FittedBondYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<FittedBondYieldCurveSegment>* v1 = dynamic_cast<Visitor<FittedBondYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

WeightedAverageYieldCurveSegment::WeightedAverageYieldCurveSegment(const string& typeID,
                                                                   const string& referenceCurveID1,
                                                                   const string& referenceCurveID2, const Real weight1,
                                                                   const Real weight2)
    : YieldCurveSegment(typeID, "", {}), referenceCurveID1_(referenceCurveID1), referenceCurveID2_(referenceCurveID2),
      weight1_(weight1), weight2_(weight2) {}

void WeightedAverageYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "WeightedAverage");
    YieldCurveSegment::fromXML(node);
    referenceCurveID1_ = XMLUtils::getChildValue(node, "ReferenceCurve1", true);
    referenceCurveID2_ = XMLUtils::getChildValue(node, "ReferenceCurve2", true);
    weight1_ = XMLUtils::getChildValueAsDouble(node, "Weight1", true);
    weight2_ = XMLUtils::getChildValueAsDouble(node, "Weight2", true);
}

XMLNode* WeightedAverageYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "WeightedAverage");
    XMLUtils::addChild(doc, node, "ReferenceCurve1", referenceCurveID1_);
    XMLUtils::addChild(doc, node, "ReferenceCurve2", referenceCurveID2_);
    XMLUtils::addChild(doc, node, "Weight1", weight1_);
    XMLUtils::addChild(doc, node, "Weight2", weight2_);
    return node;
}

void WeightedAverageYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<WeightedAverageYieldCurveSegment>* v1 = dynamic_cast<Visitor<WeightedAverageYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

YieldPlusDefaultYieldCurveSegment::YieldPlusDefaultYieldCurveSegment(const string& typeID,
                                                                     const string& referenceCurveID,
                                                                     const std::vector<std::string>& defaultCurveIDs,
                                                                     const std::vector<Real>& weights)
    : YieldCurveSegment(typeID, "", {}), referenceCurveID_(referenceCurveID), defaultCurveIDs_(defaultCurveIDs),
      weights_(weights) {}

void YieldPlusDefaultYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "YieldPlusDefault");
    YieldCurveSegment::fromXML(node);
    referenceCurveID_ = XMLUtils::getChildValue(node, "ReferenceCurve", true);
    defaultCurveIDs_ = XMLUtils::getChildrenValues(node, "DefaultCurves", "DefaultCurve", true);
    weights_ = XMLUtils::getChildrenValuesAsDoubles(node, "Weights", "Weight", true);
}

XMLNode* YieldPlusDefaultYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "YieldPlusDefault");
    XMLUtils::addChild(doc, node, "ReferenceCurve", referenceCurveID_);
    XMLUtils::addChildren(doc, node, "DefaultCurves", "DefaultCurve", defaultCurveIDs_);
    XMLUtils::addChildren(doc, node, "Weights", "Weight", weights_);
    return node;
}

void YieldPlusDefaultYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<YieldPlusDefaultYieldCurveSegment>* v1 = dynamic_cast<Visitor<YieldPlusDefaultYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

IborFallbackCurveSegment::IborFallbackCurveSegment(const string& typeID, const string& iborIndex,
                                                   const string& rfrCurve, const boost::optional<string>& rfrIndex,
                                                   const boost::optional<Real>& spread)
    : YieldCurveSegment(typeID, "", {}), iborIndex_(iborIndex), rfrCurve_(rfrCurve), rfrIndex_(rfrIndex),
      spread_(spread) {}

void IborFallbackCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IborFallback");
    YieldCurveSegment::fromXML(node);
    iborIndex_ = XMLUtils::getChildValue(node, "IborIndex", true);
    rfrCurve_ = XMLUtils::getChildValue(node, "RfrCurve", true);
    rfrIndex_ = boost::none;
    spread_ = boost::none;
    if (auto n = XMLUtils::getChildNode(node, "RfrIndex"))
        rfrIndex_ = XMLUtils::getNodeValue(n);
    if (auto n = XMLUtils::getChildNode(node, "Spread"))
        spread_ = parseReal(XMLUtils::getNodeValue(n));
}

XMLNode* IborFallbackCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "IborFallback");
    XMLUtils::addChild(doc, node, "IborIndex", iborIndex_);
    XMLUtils::addChild(doc, node, "RfrCurve", rfrCurve_);
    if (rfrIndex_)
        XMLUtils::addChild(doc, node, "RfrIndex", *rfrIndex_);
    if (spread_)
        XMLUtils::addChild(doc, node, "Spread", *spread_);
    return node;
}

void IborFallbackCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<IborFallbackCurveSegment>* v1 = dynamic_cast<Visitor<IborFallbackCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}

BondYieldShiftedYieldCurveSegment::BondYieldShiftedYieldCurveSegment(const string& typeID, const string& referenceCurveID, const vector<string>& quotes,
                                                                     const map<string, string>& iborIndexCurves, const bool extrapolateFlat)
    : YieldCurveSegment(typeID, "", quotes), referenceCurveID_(referenceCurveID), iborIndexCurves_(iborIndexCurves),
      extrapolateFlat_(extrapolateFlat) {}

void BondYieldShiftedYieldCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BondYieldShifted");
    YieldCurveSegment::fromXML(node);

    referenceCurveID_ = XMLUtils::getChildValue(node, "ReferenceCurve", true);

    vector<string> iborIndexNames;
    vector<string> iborIndexCurves = XMLUtils::getChildrenValuesWithAttributes(
        node, "IborIndexCurves", "IborIndexCurve", "iborIndex", iborIndexNames, false);
    for (Size i = 0; i < iborIndexNames.size(); ++i) {
        iborIndexCurves_[iborIndexNames[i]] = iborIndexCurves[i];
    }

	if (auto n = XMLUtils::getChildNode(node, "ExtrapolateFlat")) {
        extrapolateFlat_ = parseBool(XMLUtils::getNodeValue(n));
    } else {
        extrapolateFlat_ = false;
    }
}

XMLNode* BondYieldShiftedYieldCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* node = YieldCurveSegment::toXML(doc);
    XMLUtils::setNodeName(doc, node, "BondYieldShifted");
    XMLUtils::addChild(doc, node, "ReferenceCurve", referenceCurveID_);
    return node;
}

void BondYieldShiftedYieldCurveSegment::accept(AcyclicVisitor& v) {
    Visitor<BondYieldShiftedYieldCurveSegment>* v1 = dynamic_cast<Visitor<BondYieldShiftedYieldCurveSegment>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        YieldCurveSegment::accept(v);
}


} // namespace data
} // namespace ore
