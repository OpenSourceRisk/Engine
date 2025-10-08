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

#include <ored/configuration/inflationcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

using namespace ore::data;

namespace ore {
namespace data {
    
std::ostream& operator<<(std::ostream& out, InflationCurveConfig::Type t) {
    switch (t) {
    case InflationCurveConfig::Type::ZC:
        return out << "ZC";
    case InflationCurveConfig::Type::YY:
        return out << "YY";
    default:
        QL_FAIL("unknown Type(" << Integer(t) << ")");
    }
}


InflationCurveSegment::InflationCurveSegment(const std::string& convention,
                                             const std::vector<std::string>& quotes)
    : convention_(convention), quotes_(quotes) {}

void InflationCurveSegment::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Segment");
    convention_ = XMLUtils::getChildValue(node, "Conventions", true);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
}

XMLNode* InflationCurveSegment::toXML(XMLDocument& doc) const {
    XMLNode* segmentNode = doc.allocNode("Segment");
    XMLUtils::addChild(doc, segmentNode, "Conventions", convention_);
    XMLUtils::addChildren(doc, segmentNode, "Quotes", "Quote", quotes_);
    return segmentNode;
}


InflationCurveConfig::InflationCurveConfig(
    const string& curveID, const string& curveDescription, const string& nominalTermStructure, const InflationCurveConfig::Type type,
    const vector<string>& swapQuotes, const string& conventions, const bool extrapolate, const Calendar& calendar,
    const DayCounter& dayCounter, const Period& lag, const Frequency& frequency, const Real baseRate,
    const Real tolerance, const bool useLastAvailableFixingAsBaseDate, const Date& seasonalityBaseDate,
    const Frequency& seasonalityFrequency, const vector<string>& seasonalityFactors,
    const vector<double>& overrideSeasonalityFactors, const InterpolationVariable& interpolationVariable)
    : CurveConfig(curveID, curveDescription), nominalTermStructure_(nominalTermStructure),
      type_(type), segments_({InflationCurveSegment(conventions, swapQuotes)}), extrapolate_(extrapolate), calendar_(calendar),
      dayCounter_(dayCounter), lag_(lag), frequency_(frequency), baseRate_(baseRate), tolerance_(tolerance),
      useLastAvailableFixingAsBaseDate_(useLastAvailableFixingAsBaseDate), seasonalityBaseDate_(seasonalityBaseDate),
      seasonalityFrequency_(seasonalityFrequency), seasonalityFactors_(seasonalityFactors),
      overrideSeasonalityFactors_(overrideSeasonalityFactors), interpolationVariable_(interpolationVariable) {
    initQuotes();
}

InflationCurveConfig::InflationCurveConfig(
    const string& curveID, const string& curveDescription, const string& nominalTermStructure,
    const InflationCurveConfig::Type type, const std::vector<InflationCurveSegment>& segments, bool extrapolate, const Calendar& calendar,
    const DayCounter& dayCounter, const Period& lag, const Frequency& frequency, const Real baseRate,
    const Real tolerance, const bool useLastAvailableFixingAsBaseDate, const Date& seasonalityBaseDate,
    const Frequency& seasonalityFrequency, const vector<string>& seasonalityFactors,
    const vector<double>& overrideSeasonalityFactors, const InterpolationVariable& interpolationVariable)
    : CurveConfig(curveID, curveDescription), nominalTermStructure_(nominalTermStructure), type_(type), segments_(segments),
      extrapolate_(extrapolate), calendar_(calendar), dayCounter_(dayCounter), lag_(lag), frequency_(frequency),
      baseRate_(baseRate), tolerance_(tolerance), useLastAvailableFixingAsBaseDate_(useLastAvailableFixingAsBaseDate),
      seasonalityBaseDate_(seasonalityBaseDate), seasonalityFrequency_(seasonalityFrequency),
      seasonalityFactors_(seasonalityFactors), overrideSeasonalityFactors_(overrideSeasonalityFactors),
      interpolationVariable_(interpolationVariable) {
    initQuotes();
}

void InflationCurveConfig::initQuotes() {
    quotes_.clear();
    for (const auto& s : segments_) {
        quotes_.insert(quotes_.end(), s.quotes().begin(), s.quotes().end());
    }
    quotes_.insert(quotes_.end(), seasonalityFactors_.begin(), seasonalityFactors_.end());
}

void InflationCurveConfig::populateRequiredIds() const {
    if (!nominalTermStructure().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(nominalTermStructure())->curveConfigID());
}

void InflationCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "InflationCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    nominalTermStructure_ = XMLUtils::getChildValue(node, "NominalTermStructure", true);

    string type = XMLUtils::getChildValue(node, "Type", true);
    if (type == "ZC") {
        type_ = Type::ZC;
    } else if (type == "YY") {
        type_ = Type::YY;
    } else
        QL_FAIL("Type " << type << " not recognized");
        
    segments_.clear();
    XMLNode* segmentsNode = XMLUtils::getChildNode(node, "Segments");
    if (segmentsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(segmentsNode); child; child = XMLUtils::getNextSibling(child)) {
            if (XMLUtils::getNodeName(child) == "Segment") {
                segments_.emplace_back();
                segments_.back().fromXML(child);
            }
        }
    } else {
        // Legacy support for single segment curves
        
        vector<string> swapQuotes = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
        std::string conventions = XMLUtils::getChildValue(node, "Conventions", true);
        segments_.push_back(InflationCurveSegment(conventions, swapQuotes));
    }

    extrapolate_ = XMLUtils::getChildValueAsBool(node, "Extrapolation", false);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    string lag = XMLUtils::getChildValue(node, "Lag", true);
    lag_ = parsePeriod(lag);

    string freq = XMLUtils::getChildValue(node, "Frequency", true);
    frequency_ = parseFrequency(freq);

    baseRate_ = QuantLib::Null<Real>();
    string baseZr = XMLUtils::getChildValue(node, "BaseRate", false);
    if (baseZr != "")
        baseRate_ = parseReal(baseZr);

    string interpolationVariableStr = XMLUtils::getChildValue(node, "InterpolationVariable", false);
    if(interpolationVariableStr.empty() || interpolationVariableStr != "PriceIndex"){
        interpolationVariable_ = InterpolationVariable::ZeroRate;
    } else {
        interpolationVariable_ = InterpolationVariable::PriceIndex;
    }

    interpolationMethod_ = XMLUtils::getChildValue(node, "InterpolationMethod", false, "Linear");

    string tol = XMLUtils::getChildValue(node, "Tolerance", true);
    tolerance_ = parseReal(tol);

    useLastAvailableFixingAsBaseDate_ = XMLUtils::getChildValueAsBool(node, "UseLastFixingDate", false, false);

    XMLNode* seasonalityNode = XMLUtils::getChildNode(node, "Seasonality");
    seasonalityBaseDate_ = QuantLib::Null<Date>();
    seasonalityFrequency_ = QuantLib::NoFrequency;
    seasonalityFactors_.clear();
    if (seasonalityNode != nullptr) {
        seasonalityBaseDate_ = parseDate(XMLUtils::getChildValue(seasonalityNode, "BaseDate", true));
        seasonalityFrequency_ = parseFrequency(XMLUtils::getChildValue(seasonalityNode, "Frequency", true));
        seasonalityFactors_ = XMLUtils::getChildrenValues(seasonalityNode, "Factors", "Factor", false);
        std::string overrideFctStr = XMLUtils::getChildValue(seasonalityNode, "OverrideFactors", false);
        overrideSeasonalityFactors_ = parseListOfValues<Real>(overrideFctStr, &parseReal);
    }
    initQuotes();
}

XMLNode* InflationCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("InflationCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "NominalTermStructure", nominalTermStructure_);
    XMLUtils::addChild(doc, node, "Type", to_string(type_));

    // Add the segments node.
    XMLNode* segmentsNode = doc.allocNode("Segments");
    XMLUtils::appendNode(node, segmentsNode);
    for (const auto& segment : segments_) {
        XMLUtils::appendNode(segmentsNode, segment.toXML(doc));
    }

    string extrap = (extrapolate_ ? "true" : "false");
    XMLUtils::addChild(doc, node, "Extrapolation", extrap);

    string baseRateStr;
    if (baseRate_ != QuantLib::Null<Real>())
        baseRateStr = to_string(baseRate_);
    else
        baseRateStr = "";

    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "Lag", to_string(lag_));
    XMLUtils::addChild(doc, node, "Frequency", to_string(frequency_));
    XMLUtils::addChild(doc, node, "BaseRate", baseRateStr);
    XMLUtils::addChild(doc, node, "Tolerance", tolerance_);

    XMLUtils::addChild(doc, node, "InterpolationVariable",
                       interpolationVariable_ == InterpolationVariable::PriceIndex ? "PriceIndex" : "ZeroRate");

    if (!interpolationMethod_.empty()) {
        XMLUtils::addChild(doc, node, "InterpolationMethod", interpolationMethod_);
    } 

    if (useLastAvailableFixingAsBaseDate_)
        XMLUtils::addChild(doc, node, "UseLastFixingDate", to_string(useLastAvailableFixingAsBaseDate_));

    if (seasonalityBaseDate_ != QuantLib::Null<Date>()) {
        XMLNode* seasonalityNode = XMLUtils::addChild(doc, node, "Seasonality");
        std::ostringstream dateStr, sFreq;
        dateStr << QuantLib::io::iso_date(seasonalityBaseDate_);
        sFreq << seasonalityFrequency_;
        XMLUtils::addChild(doc, seasonalityNode, "BaseDate", dateStr.str());
        XMLUtils::addChild(doc, seasonalityNode, "Frequency", sFreq.str());
        if (!seasonalityFactors_.empty())
            XMLUtils::addChildren(doc, seasonalityNode, "Factors", "Factor", seasonalityFactors_);
        if (!overrideSeasonalityFactors_.empty())
            XMLUtils::addChild(doc, seasonalityNode, "OverrideFactors", overrideSeasonalityFactors_);
    }

    return node;
}
} // namespace data
} // namespace ore
