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

#include <ored/model/inflation/infjydata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using QuantLib::Real;
using QuantLib::Size;
using std::vector;

namespace ore {
namespace data {

InfJyData::InfJyData() {}

InfJyData::InfJyData(CalibrationType calibrationType, const vector<CalibrationBasket>& calibrationBaskets,
                     const std::string& currency, const std::string& index, const ReversionParameter& realRateReversion,
                     const VolatilityParameter& realRateVolatility, const VolatilityParameter& indexVolatility,
                     const LgmReversionTransformation& reversionTransformation,
                     const CalibrationConfiguration& calibrationConfiguration,
                     const bool ignoreDuplicateCalibrationExpiryTimes, const bool linkRealToNominalRateParams,
                     const Real linkedRealRateVolatilityScaling)
    : InflationModelData(calibrationType, calibrationBaskets, currency, index, ignoreDuplicateCalibrationExpiryTimes),
      realRateReversion_(realRateReversion), realRateVolatility_(realRateVolatility), indexVolatility_(indexVolatility),
      reversionTransformation_(reversionTransformation), calibrationConfiguration_(calibrationConfiguration),
      linkRealToNominalRateParams_(linkRealToNominalRateParams),
      linkedRealRateVolatilityScaling_(linkedRealRateVolatilityScaling) {}

const ReversionParameter& InfJyData::realRateReversion() const {
    return realRateReversion_;
}

const VolatilityParameter& InfJyData::realRateVolatility() const {
    return realRateVolatility_;
}

const VolatilityParameter& InfJyData::indexVolatility() const {
    return indexVolatility_;
}

const LgmReversionTransformation& InfJyData::reversionTransformation() const {
    return reversionTransformation_;
}

const CalibrationConfiguration& InfJyData::calibrationConfiguration() const {
    return calibrationConfiguration_;
}

void InfJyData::setRealRateReversion(ReversionParameter p) { realRateReversion_ = std::move(p); }

void InfJyData::setRealRateVolatility(VolatilityParameter p) { realRateVolatility_ = std::move(p); }

bool InfJyData::linkRealRateParamsToNominalRateParams() const { return linkRealToNominalRateParams_; }

Real InfJyData::linkedRealRateVolatilityScaling() const { return linkedRealRateVolatilityScaling_; }

void InfJyData::fromXML(XMLNode* node) {
    
    // Check the node is not null and that name is LGM or DodgsonKainth. LGM is for backward compatibility.
    XMLUtils::checkNode(node, "JarrowYildirim");

    InflationModelData::fromXML(node);
    
    // Get reversion and volatility for the real rate process
    XMLNode* rrNode = XMLUtils::getChildNode(node, "RealRate");
    QL_REQUIRE(rrNode, "JarrowYildirim inflation model data should have RealRate node.");
    realRateReversion_.fromXML(XMLUtils::getChildNode(rrNode, "Reversion"));
    realRateVolatility_.fromXML(XMLUtils::getChildNode(rrNode, "Volatility"));
    if (XMLNode* n = XMLUtils::getChildNode(rrNode, "ParameterTransformation")) {
        reversionTransformation_.fromXML(n);
    }

    // Get volatility for the inflation index process
    XMLNode* idxNode = XMLUtils::getChildNode(node, "Index");
    QL_REQUIRE(idxNode, "JarrowYildirim inflation model data should have Index node.");
    indexVolatility_.fromXML(XMLUtils::getChildNode(idxNode, "Volatility"));

    // Get the calibration configuration
    if (XMLNode* ccNode = XMLUtils::getChildNode(node, "CalibrationConfiguration"))
        calibrationConfiguration_.fromXML(ccNode);

    // Get the link to nominal param fields
    linkRealToNominalRateParams_ =
        parseBool(XMLUtils::getChildValue(node, "LinkRealToNominalRateParams", false, "false"));
    if (linkRealToNominalRateParams_) {
        linkedRealRateVolatilityScaling_ =
            parseReal(XMLUtils::getChildValue(node, "LinkedRealRateVolatilityScaling", false, "1.0"));
    }
}

XMLNode* InfJyData::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("JarrowYildirim");
    InflationModelData::append(doc, node);

    XMLNode* rrNode = doc.allocNode("RealRate");
    XMLUtils::appendNode(rrNode, realRateReversion_.toXML(doc));
    XMLUtils::appendNode(rrNode, realRateVolatility_.toXML(doc));
    XMLUtils::appendNode(rrNode, reversionTransformation_.toXML(doc));
    XMLUtils::appendNode(node, rrNode);

    XMLNode* idxNode = doc.allocNode("Index");
    XMLUtils::appendNode(idxNode, indexVolatility_.toXML(doc));
    XMLUtils::appendNode(node, idxNode);

    XMLUtils::appendNode(node, calibrationConfiguration_.toXML(doc));

    if (linkRealToNominalRateParams_) {
        XMLUtils::addChild(doc, node, "LinkRealToNominalRateParams", linkRealToNominalRateParams_);
        XMLUtils::addChild(doc, node, "LinkedRealRateVolatilityScaling", linkedRealRateVolatilityScaling_);
    }

    return node;
}

}
}
