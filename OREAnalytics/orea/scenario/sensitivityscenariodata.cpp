/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>

using ore::analytics::RiskFactorKey;
using ore::data::XMLDocument;
using std::string;

using namespace QuantLib;

namespace ore {
namespace analytics {

using RFType = RiskFactorKey::KeyType;
using ShiftData = SensitivityScenarioData::ShiftData;

void SensitivityScenarioData::shiftDataFromXML(XMLNode* child, ShiftData& data) {
    data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
    data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
}

void SensitivityScenarioData::curveShiftDataFromXML(XMLNode* child, CurveShiftData& data) {
    shiftDataFromXML(child, data);
    data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
}

void SensitivityScenarioData::volShiftDataFromXML(XMLNode* child, VolShiftData& data) {
    shiftDataFromXML(child, data);
    data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
    data.shiftStrikes = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", true);
    if (data.shiftStrikes.size() == 0)
        data.shiftStrikes = {0.0};
}

void SensitivityScenarioData::shiftDataToXML(XMLDocument& doc, XMLNode* node, const ShiftData& data) const {
    XMLUtils::addChild(doc, node, "ShiftType", data.shiftType);
    XMLUtils::addChild(doc, node, "ShiftSize", data.shiftSize);
}

void SensitivityScenarioData::curveShiftDataToXML(XMLDocument& doc, XMLNode* node, const CurveShiftData& data) const {
    shiftDataToXML(doc, node, data);
    XMLUtils::addGenericChildAsList(doc, node, "ShiftTenors", data.shiftTenors);
}

void SensitivityScenarioData::volShiftDataToXML(XMLDocument& doc, XMLNode* node, const VolShiftData& data) const {
    shiftDataToXML(doc, node, data);
    XMLUtils::addGenericChildAsList(doc, node, "ShiftExpiries", data.shiftExpiries);
    XMLUtils::addChild(doc, node, "ShiftStrikes", data.shiftStrikes);
}

const ShiftData& SensitivityScenarioData::shiftData(const RFType& keyType, const string& name) const {
    // Not nice but not spending time refactoring the class now.
    switch (keyType) {
    case RFType::DiscountCurve:
        return *discountCurveShiftData().at(name);
    case RFType::IndexCurve:
        return *indexCurveShiftData().at(name);
    case RFType::YieldCurve:
        return *yieldCurveShiftData().at(name);
    case RFType::FXSpot:
        return fxShiftData().at(name);
    case RFType::SwaptionVolatility:
        return swaptionVolShiftData().at(name);
    case RFType::OptionletVolatility:
        return capFloorVolShiftData().at(name);
    case RFType::FXVolatility:
        return fxVolShiftData().at(name);
    case RFType::CDSVolatility:
        return cdsVolShiftData().at(name);
    case RFType::BaseCorrelation:
        return baseCorrelationShiftData().at(name);
    case RFType::ZeroInflationCurve:
        return *zeroInflationCurveShiftData().at(name);
    case RFType::SurvivalProbability:
        return *creditCurveShiftData().at(name);
    case RFType::YoYInflationCurve:
        return *yoyInflationCurveShiftData().at(name);
    case RFType::EquitySpot:
        return equityShiftData().at(name);
    case RFType::EquityVolatility:
        return equityVolShiftData().at(name);
    case RFType::EquityForecastCurve:
        return *equityForecastCurveShiftData().at(name);
    case RFType::DividendYield:
        return *dividendYieldShiftData().at(name);
    case RFType::CommoditySpot:
        return commodityShiftData().at(name);
    case RFType::CommodityCurve:
        return *commodityCurveShiftData().at(name);
    case RFType::CommodityVolatility:
        return commodityVolShiftData().at(name);
    case RFType::SecuritySpread:
        return securityShiftData().at(name);
    default:
        QL_FAIL("Cannot return shift data for key type: " << keyType);
    }
}

void SensitivityScenarioData::fromXML(XMLNode* root) {
    XMLNode* node = XMLUtils::locateNode(root, "SensitivityAnalysis");
    XMLUtils::checkNode(node, "SensitivityAnalysis");

    LOG("Get discount curve sensitivity parameters");
    XMLNode* discountCurves = XMLUtils::getChildNode(node, "DiscountCurves");
    if (discountCurves) {
        for (XMLNode* child = XMLUtils::getChildNode(discountCurves, "DiscountCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccy = XMLUtils::getAttribute(child, "ccy");
            LOG("Discount curve for ccy " << ccy);
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            discountCurveShiftData_[ccy] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get index curve sensitivity parameters");
    XMLNode* indexCurves = XMLUtils::getChildNode(node, "IndexCurves");
    if (indexCurves) {
        for (XMLNode* child = XMLUtils::getChildNode(indexCurves, "IndexCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string index = XMLUtils::getAttribute(child, "index");
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            indexCurveShiftData_[index] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get yield curve sensitivity parameters");
    XMLNode* yieldCurves = XMLUtils::getChildNode(node, "YieldCurves");
    if (yieldCurves) {
        for (XMLNode* child = XMLUtils::getChildNode(yieldCurves, "YieldCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string curveName = XMLUtils::getAttribute(child, "name");
            CurveShiftData data;
            curveShiftDataFromXML(child, data);

            string curveType = XMLUtils::getChildValue(child, "CurveType", false);
            if (curveType == "EquityForecast") {
                equityForecastCurveShiftData_[curveName] = boost::make_shared<CurveShiftData>(data);
            } else {
                yieldCurveShiftData_[curveName] = boost::make_shared<CurveShiftData>(data);
            }
        }
    }

    LOG("Get dividend yield curve sensitivity parameters");
    XMLNode* dividendYieldCurves = XMLUtils::getChildNode(node, "DividendYieldCurves");
    if (dividendYieldCurves) {
        for (XMLNode* child = XMLUtils::getChildNode(dividendYieldCurves, "DividendYieldCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string curveName = XMLUtils::getAttribute(child, "equity");
            LOG("Add dividend yield curve data for equity " << curveName);
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            dividendYieldShiftData_[curveName] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get FX spot sensitivity parameters");
    XMLNode* fxSpots = XMLUtils::getChildNode(node, "FxSpots");
    if (fxSpots) {
        for (XMLNode* child = XMLUtils::getChildNode(fxSpots, "FxSpot"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccypair = XMLUtils::getAttribute(child, "ccypair");
            SpotShiftData data;
            shiftDataFromXML(child, data);
            fxShiftData_[ccypair] = data;
        }
    }

    LOG("Get swaption vol sensitivity parameters");
    XMLNode* swaptionVols = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    if (swaptionVols) {
        for (XMLNode* child = XMLUtils::getChildNode(swaptionVols, "SwaptionVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccy = XMLUtils::getAttribute(child, "ccy");
            SwaptionVolShiftData data;
            volShiftDataFromXML(child, data);
            data.shiftTerms = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTerms", true);
            if (data.shiftStrikes.size() == 0)
                data.shiftStrikes = {0.0};
            swaptionVolShiftData_[ccy] = data;
        }
    }

    LOG("Get cap/floor vol sensitivity parameters");
    XMLNode* capVols = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    if (capVols) {
        for (XMLNode* child = XMLUtils::getChildNode(capVols, "CapFloorVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccy = XMLUtils::getAttribute(child, "ccy");
            CapFloorVolShiftData data;
            volShiftDataFromXML(child, data);
            data.indexName = XMLUtils::getChildValue(child, "Index", true);
            capFloorVolShiftData_[ccy] = data;
        }
    }

    LOG("Get fx vol sensitivity parameters");
    XMLNode* fxVols = XMLUtils::getChildNode(node, "FxVolatilities");
    if (fxVols) {
        for (XMLNode* child = XMLUtils::getChildNode(fxVols, "FxVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccypair = XMLUtils::getAttribute(child, "ccypair");
            VolShiftData data;
            volShiftDataFromXML(child, data);
            fxVolShiftData_[ccypair] = data;
        }
    }

    LOG("Get credit curve sensitivity parameters");
    XMLNode* creditCurves = XMLUtils::getChildNode(node, "CreditCurves");
    if (creditCurves) {
        for (XMLNode* child = XMLUtils::getChildNode(creditCurves, "CreditCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            string ccy = XMLUtils::getChildValue(child, "Currency", true);
            creditCcys_[name] = ccy;
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            creditCurveShiftData_[name] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get cds vol sensitivity parameters");
    XMLNode* cdsVols = XMLUtils::getChildNode(node, "CDSVolatilities");
    if (cdsVols) {
        for (XMLNode* child = XMLUtils::getChildNode(cdsVols, "CDSVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            CdsVolShiftData data;
            shiftDataFromXML(child, data);
            data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
            cdsVolShiftData_[name] = data;
        }
    }

    LOG("Get Base Correlation sensitivity parameters");
    XMLNode* bcNode = XMLUtils::getChildNode(node, "BaseCorrelations");
    if (bcNode) {
        for (XMLNode* child = XMLUtils::getChildNode(bcNode, "BaseCorrelation"); child;
             child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "indexName");
            BaseCorrelationShiftData data;
            shiftDataFromXML(child, data);
            data.shiftTerms = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTerms", true);
            data.shiftLossLevels = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftLossLevels", true);
            baseCorrelationShiftData_[name] = data;
        }
    }

    LOG("Get Equity spot sensitivity parameters");
    XMLNode* equitySpots = XMLUtils::getChildNode(node, "EquitySpots");
    if (equitySpots) {
        for (XMLNode* child = XMLUtils::getChildNode(equitySpots, "EquitySpot"); child;
             child = XMLUtils::getNextSibling(child)) {
            string equity = XMLUtils::getAttribute(child, "equity");
            SpotShiftData data;
            shiftDataFromXML(child, data);
            equityShiftData_[equity] = data;
        }
    }

    LOG("Get Equity vol sensitivity parameters");
    XMLNode* equityVols = XMLUtils::getChildNode(node, "EquityVolatilities");
    if (equityVols) {
        for (XMLNode* child = XMLUtils::getChildNode(equityVols, "EquityVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string equity = XMLUtils::getAttribute(child, "equity");
            VolShiftData data;
            volShiftDataFromXML(child, data);
            equityVolShiftData_[equity] = data;
        }
    }

    LOG("Get Zero Inflation sensitivity parameters");
    XMLNode* zeroInflation = XMLUtils::getChildNode(node, "ZeroInflationIndexCurves");
    if (zeroInflation) {
        for (XMLNode* child = XMLUtils::getChildNode(zeroInflation, "ZeroInflationIndexCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string index = XMLUtils::getAttribute(child, "index");
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            zeroInflationCurveShiftData_[index] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get Yoy Inflation sensitivity parameters");
    XMLNode* yoyInflation = XMLUtils::getChildNode(node, "YYInflationIndexCurves");
    if (yoyInflation) {
        for (XMLNode* child = XMLUtils::getChildNode(yoyInflation, "YYInflationIndexCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string index = XMLUtils::getAttribute(child, "index");
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            yoyInflationCurveShiftData_[index] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get commodity spot sensitivity parameters");
    XMLNode* csNode = XMLUtils::getChildNode(node, "CommoditySpots");
    if (csNode) {
        for (XMLNode* child = XMLUtils::getChildNode(csNode, "CommoditySpot"); child;
            child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            SpotShiftData data;
            shiftDataFromXML(child, data);
            commodityShiftData_[name] = data;
        }
    }

    LOG("Get commodity curve sensitivity parameters");
    XMLNode* ccNode = XMLUtils::getChildNode(node, "CommodityCurves");
    if (ccNode) {
        for (XMLNode* child = XMLUtils::getChildNode(ccNode, "CommodityCurve"); child;
            child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            commodityCurrencies_[name] = XMLUtils::getChildValue(child, "Currency", true);
            CurveShiftData data;
            curveShiftDataFromXML(child, data);
            commodityCurveShiftData_[name] = boost::make_shared<CurveShiftData>(data);
        }
    }

    LOG("Get commodity volatility sensitivity parameters");
    XMLNode* cvNode = XMLUtils::getChildNode(node, "CommodityVolatilities");
    if (cvNode) {
        for (XMLNode* child = XMLUtils::getChildNode(cvNode, "CommodityVolatility"); child;
            child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            VolShiftData data;
            volShiftDataFromXML(child, data);
            // If data has one strike and it is 0.0, it needs to be overwritten for commodity volatilities
            // Commodity volatility surface in simulation market is defined in terms of spot moneyness e.g.
            // strike sets like {0.99 * S(0), 1.00 * S(0), 1.01 * S(0)} => we need to define sensitivity 
            // data in the same way
            if (data.shiftStrikes.size() == 1 && close_enough(data.shiftStrikes[0], 0.0)) {
                data.shiftStrikes[0] = 1.0;
            }
            commodityVolShiftData_[name] = data;
        }
    }

    LOG("Get security spread sensitivity parameters");
    XMLNode* securitySpreads = XMLUtils::getChildNode(node, "SecuritySpreads");
    if (securitySpreads) {
        for (XMLNode* child = XMLUtils::getChildNode(securitySpreads, "SecuritySpread"); child;
            child = XMLUtils::getNextSibling(child)) {
            string bond = XMLUtils::getAttribute(child, "security");
            SpotShiftData data;
            shiftDataFromXML(child, data);
            securityShiftData_[bond] = data;
        }
    }

    LOG("Get cross gamma parameters");
    vector<string> filter = XMLUtils::getChildrenValues(node, "CrossGammaFilter", "Pair", true);
    for (Size i = 0; i < filter.size(); ++i) {
        vector<string> tokens;
        boost::split(tokens, filter[i], boost::is_any_of(","));
        QL_REQUIRE(tokens.size() == 2, "expected 2 tokens, found " << tokens.size() << " in " << filter[i]);
        crossGammaFilter_.push_back(pair<string, string>(tokens[0], tokens[1]));
    }
}

XMLNode* SensitivityScenarioData::toXML(XMLDocument& doc) {
    
    XMLNode* root = doc.allocNode("SensitivityAnalysis");
    
    if (!discountCurveShiftData_.empty()) {
        LOG("toXML for DiscountCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "DiscountCurves");
        for (const auto& kv : discountCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "DiscountCurve");
            XMLUtils::addAttribute(doc, node, "ccy", kv.first);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!indexCurveShiftData_.empty()) {
        LOG("toXML for IndexCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "IndexCurves");
        for (const auto& kv : indexCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "IndexCurve");
            XMLUtils::addAttribute(doc, node, "index", kv.first);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    XMLNode* yieldCurvesNode = XMLUtils::addChild(doc, root, "YieldCurves");
    if (!equityForecastCurveShiftData_.empty()) {
        LOG("toXML for YieldCurves of type EquityForecast");
        for (const auto& kv : equityForecastCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, yieldCurvesNode, "YieldCurve");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            XMLUtils::addChild(doc, node, "CurveType", "EquityForecast");
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }
    if (!yieldCurveShiftData_.empty()) {
        LOG("toXML for YieldCurves that are not of type EquityForecast");
        for (const auto& kv : yieldCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, yieldCurvesNode, "YieldCurve");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!dividendYieldShiftData_.empty()) {
        LOG("toXML for DividendYieldCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "DividendYieldCurves");
        for (const auto& kv : dividendYieldShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "DividendYieldCurve");
            XMLUtils::addAttribute(doc, node, "equity", kv.first);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!fxShiftData_.empty()) {
        LOG("toXML for FxSpots");
        XMLNode* parent = XMLUtils::addChild(doc, root, "FxSpots");
        for (const auto& kv : fxShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "FxSpot");
            XMLUtils::addAttribute(doc, node, "ccypair", kv.first);
            shiftDataToXML(doc, node, kv.second);
        }
    }

    if (!swaptionVolShiftData_.empty()) {
        LOG("toXML for SwaptionVolatilities");
        XMLNode* parent = XMLUtils::addChild(doc, root, "SwaptionVolatilities");
        for (const auto& kv : swaptionVolShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "SwaptionVolatility");
            XMLUtils::addAttribute(doc, node, "ccy", kv.first);
            volShiftDataToXML(doc, node, kv.second);
            XMLUtils::addGenericChildAsList(doc, node, "ShiftTerms", kv.second.shiftTerms);
        }
    }

    if (!capFloorVolShiftData_.empty()) {
        LOG("toXML for CapFloorVolatilities");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CapFloorVolatilities");
        for (const auto& kv : capFloorVolShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "CapFloorVolatility");
            XMLUtils::addAttribute(doc, node, "ccy", kv.first);
            volShiftDataToXML(doc, node, kv.second);
            XMLUtils::addChild(doc, node, "Index", kv.second.indexName);
        }
    }

    if (!fxVolShiftData_.empty()) {
        LOG("toXML for FxVolatilities");
        XMLNode* parent = XMLUtils::addChild(doc, root, "FxVolatilities");
        for (const auto& kv : fxVolShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "FxVolatility");
            XMLUtils::addAttribute(doc, node, "ccypair", kv.first);
            volShiftDataToXML(doc, node, kv.second);
        }
    }

    if (!creditCurveShiftData_.empty()) {
        LOG("toXML for CreditCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CreditCurves");
        for (const auto& kv : creditCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "CreditCurve");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            XMLUtils::addChild(doc, node, "Currency", creditCcys_[kv.first]);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!cdsVolShiftData_.empty()) {
        LOG("toXML for CDSVolatilities");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CDSVolatilities");
        for (const auto& kv : cdsVolShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "CDSVolatility");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            shiftDataToXML(doc, node, kv.second);
            XMLUtils::addGenericChildAsList(doc, node, "ShiftExpiries", kv.second.shiftExpiries);
        }
    }

    if (!baseCorrelationShiftData_.empty()) {
        LOG("toXML for BaseCorrelations");
        XMLNode* parent = XMLUtils::addChild(doc, root, "BaseCorrelations");
        for (const auto& kv : baseCorrelationShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "BaseCorrelation");
            XMLUtils::addAttribute(doc, node, "indexName", kv.first);
            shiftDataToXML(doc, node, kv.second);
            XMLUtils::addGenericChildAsList(doc, node, "ShiftTerms", kv.second.shiftTerms);
            XMLUtils::addChild(doc, node, "ShiftLossLevels", kv.second.shiftLossLevels);
        }
    }

    if (!equityShiftData_.empty()) {
        LOG("toXML for EquitySpots");
        XMLNode* parent = XMLUtils::addChild(doc, root, "EquitySpots");
        for (const auto& kv : equityShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "EquitySpot");
            XMLUtils::addAttribute(doc, node, "equity", kv.first);
            shiftDataToXML(doc, node, kv.second);
        }
    }

    if (!equityVolShiftData_.empty()) {
        LOG("toXML for EquityVolatilities");
        XMLNode* parent = XMLUtils::addChild(doc, root, "EquityVolatilities");
        for (const auto& kv : equityVolShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "EquityVolatility");
            XMLUtils::addAttribute(doc, node, "equity", kv.first);
            volShiftDataToXML(doc, node, kv.second);
        }
    }

    if (!zeroInflationCurveShiftData_.empty()) {
        LOG("toXML for ZeroInflationIndexCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "ZeroInflationIndexCurves");
        for (const auto& kv : zeroInflationCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "ZeroInflationIndexCurve");
            XMLUtils::addAttribute(doc, node, "index", kv.first);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!yoyInflationCurveShiftData_.empty()) {
        LOG("toXML for YYInflationIndexCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "YYInflationIndexCurves");
        for (const auto& kv : yoyInflationCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "YYInflationIndexCurve");
            XMLUtils::addAttribute(doc, node, "index", kv.first);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!commodityShiftData_.empty()) {
        LOG("toXML for CommoditySpots");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CommoditySpots");
        for (const auto& kv : commodityShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "CommoditySpot");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            shiftDataToXML(doc, node, kv.second);
        }
    }

    if (!commodityCurveShiftData_.empty()) {
        LOG("toXML for CommodityCurves");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CommodityCurves");
        for (const auto& kv : commodityCurveShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "CommodityCurve");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            XMLUtils::addChild(doc, node, "Currency", commodityCurrencies_[kv.first]);
            curveShiftDataToXML(doc, node, *kv.second);
        }
    }

    if (!commodityVolShiftData_.empty()) {
        LOG("toXML for CommodityVolatilities");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CommodityVolatilities");
        for (const auto& kv : commodityVolShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "CommodityVolatility");
            XMLUtils::addAttribute(doc, node, "name", kv.first);
            volShiftDataToXML(doc, node, kv.second);
        }
    }

    if (!securityShiftData_.empty()) {
        LOG("toXML for SecuritySpreads");
        XMLNode* parent = XMLUtils::addChild(doc, root, "SecuritySpreads");
        for (const auto& kv : securityShiftData_) {
            XMLNode* node = XMLUtils::addChild(doc, parent, "SecuritySpread");
            XMLUtils::addAttribute(doc, node, "security", kv.first);
            shiftDataToXML(doc, node, kv.second);
        }
    }

    if (!crossGammaFilter_.empty()) {
        LOG("toXML for CrossGammaFilter");
        XMLNode* parent = XMLUtils::addChild(doc, root, "CrossGammaFilter");
        for (const auto& crossGamma : crossGammaFilter_) {
            XMLUtils::addChild(doc, parent, "Pair", crossGamma.first + "," + crossGamma.second);
        }
    }

    return root;
}

string SensitivityScenarioData::getIndexCurrency(string indexName) {
    vector<string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() > 1, "expected 2 or 3 tokens, found " << tokens.size() << " in " << indexName);
    return tokens[0];
}
} // namespace analytics
} // namespace ore
