/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/portfolio/intradaypowerlegdata.hpp>

#include <ored/utilities/parsers.hpp>

using ore::data::parseBool;
using ore::data::parseInteger;
using ore::data::parseReal;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;
using QuantLib::Natural;
using QuantLib::Null;
using QuantLib::Real;
using std::string;
using std::vector;

namespace ore {
namespace data {

IntradayPowerLegData::IntradayPowerLegData()
    : LegAdditionalData(LegType::IntradayPowerFloating), includePeriodStart_(true), includePeriodEnd_(false),
      avgPricePrecision_(Null<Natural>()) {}

IntradayPowerLegData::IntradayPowerLegData(const string& name, const vector<Real>& quantities,
                                           const vector<string>& quantityDates, const vector<Real>& spreads,
                                           const vector<string>& spreadDates, const vector<Real>& gearings,
                                           const vector<string>& gearingDates, const string& pricingCalendar,
                                           bool includePeriodStart, bool includePeriodEnd,
                                           const PowerLoadProfileData& loadProfileData, const string& fxIndex,
                                           Natural avgPricePrecision)
    : LegAdditionalData(LegType::IntradayPowerFloating), name_(name), quantities_(quantities),
      quantityDates_(quantityDates), spreads_(spreads), spreadDates_(spreadDates), gearings_(gearings),
      gearingDates_(gearingDates), pricingCalendar_(pricingCalendar), includePeriodStart_(includePeriodStart),
      includePeriodEnd_(includePeriodEnd), loadProfileData_(loadProfileData), fxIndex_(fxIndex),
      avgPricePrecision_(avgPricePrecision) {
    indices_.insert("POWER-" + name_);
}

void IntradayPowerLegData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "IntradayPowerFloatingLegData");

    indices_.clear();
    name_ = XMLUtils::getChildValue(node, "Name", true);
    indices_.insert("POWER-" + name_);

    quantities_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Quantities", "Quantity", "startDate",
                                                                  quantityDates_, &parseReal, true);

    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate",
                                                                gearingDates_, &parseReal);

    pricingCalendar_ = XMLUtils::getChildValue(node, "PricingCalendar", false);

    includePeriodStart_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IncludePeriodStart")) {
        includePeriodStart_ = parseBool(XMLUtils::getNodeValue(n));
    }

    includePeriodEnd_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IncludePeriodEnd")) {
        includePeriodEnd_ = parseBool(XMLUtils::getNodeValue(n));
    }

    loadProfileData_ = PowerLoadProfileData();
    if (XMLNode* n = XMLUtils::getChildNode(node, "PowerLoadProfileData")) {
        loadProfileData_.fromXML(n);
    }

    fxIndex_ = XMLUtils::getChildValue(node, "FXIndex", false);

    avgPricePrecision_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "AvgPricePrecision")) {
        int precision = parseInteger(XMLUtils::getNodeValue(n));
        QL_REQUIRE(precision >= 0,
                   "IntradayPowerLegData: avgPricePrecision must be non-negative, got " << precision);
        avgPricePrecision_ = static_cast<Natural>(precision);
    }
}

XMLNode* IntradayPowerLegData::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("IntradayPowerFloatingLegData");

    XMLUtils::addChild(doc, node, "Name", name_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Quantities", "Quantity", quantities_, "startDate",
                                                quantityDates_);

    if (!spreads_.empty())
        XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate",
                                                    spreadDates_);

    if (!gearings_.empty())
        XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                    gearingDates_);

    if (!pricingCalendar_.empty())
        XMLUtils::addChild(doc, node, "PricingCalendar", pricingCalendar_);

    XMLUtils::addChild(doc, node, "IncludePeriodStart", includePeriodStart_);
    XMLUtils::addChild(doc, node, "IncludePeriodEnd", includePeriodEnd_);

    if (!loadProfileData_.getLoadProfiles().empty()) {
        auto lpNode = loadProfileData_.toXML(doc);
        XMLUtils::appendNode(node, lpNode);
    }

    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, node, "FXIndex", fxIndex_);

    if (avgPricePrecision_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "AvgPricePrecision", static_cast<int>(avgPricePrecision_));

    return node;
}

} // namespace data
} // namespace ore
