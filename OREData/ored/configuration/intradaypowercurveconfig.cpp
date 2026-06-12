/*
 Copyright (C) 2026 AcadiaSoft Inc
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

#include <ored/configuration/intradaypowercurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/log.hpp>

using std::string;

namespace ore {
namespace data {

IntradayPowerCurveConfig::IntradayPowerCurveConfig(const string& curveId, const string& curveDescription,
                                                   const string& currency,
                                                   const string& dailyAveragePriceCurve,
                                                   const string& shapeQuoteName)
    : CurveConfig(curveId, curveDescription), currency_(currency),
      dailyAveragePriceCurve_(dailyAveragePriceCurve), shapeQuoteName_(shapeQuoteName) {
        quotes_.push_back("SHAPE_PROFILE/SHAPE_FACTOR/" + shapeQuoteName +"/*");
      }

void IntradayPowerCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IntradayPowerCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    dailyAveragePriceCurve_ = XMLUtils::getChildValue(node, "DailyAveragePriceCurve", true);
    shapeQuoteName_ = XMLUtils::getChildValue(node, "ShapeQuoteName", true);
}

XMLNode* IntradayPowerCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("IntradayPowerCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChild(doc, node, "DailyAveragePriceCurve", dailyAveragePriceCurve_);
    XMLUtils::addChild(doc, node, "ShapeQuoteName", shapeQuoteName_);

    return node;
}

void IntradayPowerCurveConfig::populateRequiredIds() const {
    // dailyAveragePriceCurve_ is a full spec string, e.g. "Commodity/USD/PJM_WH_RT_AVG".
    // Parse it to extract the commodity curve config ID so that the DependencyGraph can
    // create an edge from this node to the underlying commodity curve node.
    if (!dailyAveragePriceCurve_.empty()) {
        try {
            auto spec = parseCurveSpec(dailyAveragePriceCurve_);
            requiredCurveIds_[spec->baseType()].insert(spec->curveConfigID());
        } catch (const std::exception& ex) {
            WLOG("IntradayPowerCurveConfig: could not parse DailyAveragePriceCurve spec '"
                 << dailyAveragePriceCurve_ << "': " << ex.what());
        }
    }
}

} // namespace data
} // namespace ore
