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

#include <orea/scenario/stressscenariodata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace analytics {

void StressTestScenarioData::fromXML(XMLNode* root) {
    data_.clear();

    XMLNode* node = XMLUtils::locateNode(root, "StressTesting");
    XMLUtils::checkNode(node, "StressTesting");

    useSpreadedTermStructures_ =
        ore::data::parseBool(XMLUtils::getChildValue(node, "UseSpreadedTermStructures", false, "false"));

    for (XMLNode* testCase = XMLUtils::getChildNode(node, "StressTest"); testCase;
         testCase = XMLUtils::getNextSibling(testCase)) {

        StressTestData test;
        test.label = XMLUtils::getAttribute(testCase, "id");
        // XMLUtils::getChildValue(testCase, "Label", true);

        LOG("Load stress test label " << test.label);

        XMLNode* parShiftsNode = XMLUtils::getChildNode(testCase, "ParShifts");
        if (parShiftsNode) {
            test.irCurveParShifts = XMLUtils::getChildValueAsBool(parShiftsNode, "IRCurves", false, false);
            test.irCapFloorParShifts =
                XMLUtils::getChildValueAsBool(parShiftsNode, "CapFloorVolatilities", false, false);
            test.creditCurveParShifts =
                XMLUtils::getChildValueAsBool(parShiftsNode, "SurvivalProbability", false, false);
        }

        LOG("Get recovery rate shift parameters");

        test.recoveryRateShifts.clear();
        XMLNode* recoveryRates = XMLUtils::getChildNode(testCase, "RecoveryRates");
        if (recoveryRates) {
            for (XMLNode* child = XMLUtils::getChildNode(recoveryRates, "RecoveryRate"); child;
                 child = XMLUtils::getNextSibling(child)) {
                string isin = XMLUtils::getAttribute(child, "id");
                LOG("Loading stress parameters for recovery rate for " << isin);
                SpotShiftData data;
                data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
                data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
                test.recoveryRateShifts[isin] = ext::make_shared<SpotShiftData>(data);
            }
        }
        LOG("Get survival probability shift parameters");
        XMLNode* survivalProbability = XMLUtils::getChildNode(testCase, "SurvivalProbabilities");
        QL_REQUIRE(survivalProbability, "Survival Probabilities node not found");
        test.survivalProbabilityShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(survivalProbability, "SurvivalProbability"); child;
             child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            LOG("Loading stress parameters for survival probability for " << name);
            CurveShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shifts = XMLUtils::getChildrenValuesAsDoublesCompact(child, "Shifts", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            QL_REQUIRE(data.shifts.size() == data.shiftTenors.size(),
                       "number of tenors and shifts does not match in survival probability stress data");
            QL_REQUIRE(data.shifts.size() > 0, "no shifts provided in survival probability stress data");
            test.survivalProbabilityShifts[name] = ext::make_shared<CurveShiftData>(data);
        }

        LOG("Get discount curve shift parameters");
        XMLNode* discountCurves = XMLUtils::getChildNode(testCase, "DiscountCurves");
        QL_REQUIRE(discountCurves, "DiscountCurves node not found");
        test.discountCurveShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(discountCurves, "DiscountCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccy = XMLUtils::getAttribute(child, "ccy");
            LOG("Loading stress parameters for discount curve for ccy " << ccy);
            CurveShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shifts = XMLUtils::getChildrenValuesAsDoublesCompact(child, "Shifts", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            QL_REQUIRE(data.shifts.size() == data.shiftTenors.size(),
                       "number of tenors (" << data.shiftTenors.size() << ")and shifts (" << data.shifts.size()
                                            << ") does not match in discount curve stress data for ccy = " << ccy);
            QL_REQUIRE(data.shifts.size() > 0, "no shifts provided in discount curve stress data for ccy = " << ccy);
            test.discountCurveShifts[ccy] = ext::make_shared<CurveShiftData>(data);
        }

        LOG("Get index curve stress parameters");
        XMLNode* indexCurves = XMLUtils::getChildNode(testCase, "IndexCurves");
        QL_REQUIRE(indexCurves, "IndexCurves node not found");
        test.indexCurveShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(indexCurves, "IndexCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string index = XMLUtils::getAttribute(child, "index");
            LOG("Loading stress parameters for index " << index);
            // same as discount curve sensitivity loading from here ...
            CurveShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shifts = XMLUtils::getChildrenValuesAsDoublesCompact(child, "Shifts", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            QL_REQUIRE(data.shifts.size() == data.shiftTenors.size(),
                       "number of tenors (" << data.shiftTenors.size() << ")and shifts (" << data.shifts.size()
                                            << ") does not match in index curve stress data curve = " << index);
            QL_REQUIRE(data.shifts.size() > 0, "no shifts provided in index curve stress data curve = " << index);
            test.indexCurveShifts[index] = ext::make_shared<CurveShiftData>(data);
        }

        LOG("Get yield curve stress parameters");
        XMLNode* yieldCurves = XMLUtils::getChildNode(testCase, "YieldCurves");
        QL_REQUIRE(yieldCurves, "YieldCurves node not found");
        test.yieldCurveShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(yieldCurves, "YieldCurve"); child;
             child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            LOG("Loading stress parameters for yield curve " << name);
            // same as discount curve sensitivity loading from here ...
            CurveShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shifts = XMLUtils::getChildrenValuesAsDoublesCompact(child, "Shifts", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            QL_REQUIRE(data.shifts.size() == data.shiftTenors.size(),
                       "number of tenors (" << data.shiftTenors.size() << ")and shifts (" << data.shifts.size()
                                            << ") does not match in yield curve stress data curve = " << name);
            QL_REQUIRE(data.shifts.size() > 0, "no shifts provided in yield curve stress data curve = " << name);
            test.yieldCurveShifts[name] = ext::make_shared<CurveShiftData>(data);
        }

        LOG("Get FX spot stress parameters");
        XMLNode* fxSpots = XMLUtils::getChildNode(testCase, "FxSpots");
        QL_REQUIRE(fxSpots, "FxSpots node not found");
        test.fxShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(fxSpots, "FxSpot"); child;
             child = XMLUtils::getNextSibling(child)) {
            string ccypair = XMLUtils::getAttribute(child, "ccypair");
            LOG("Loading stress parameters for FX " << ccypair);
            SpotShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            test.fxShifts[ccypair] = ext::make_shared<SpotShiftData>(data);
        }

        LOG("Get fx vol stress parameters");
        XMLNode* fxVols = XMLUtils::getChildNode(testCase, "FxVolatilities");
        test.fxVolShifts.clear();
        if (fxVols) {
            for (XMLNode* child = XMLUtils::getChildNode(fxVols, "FxVolatility"); child;
                 child = XMLUtils::getNextSibling(child)) {
                string ccypair = XMLUtils::getAttribute(child, "ccypair");
                LOG("Loading stress parameters for FX vols " << ccypair);
                auto shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType"));
                XMLNode* shiftsNode = XMLUtils::getChildNode(child, "Shifts");
                XMLNode* shiftExpiriesNode = XMLUtils::getChildNode(child, "ShiftExpiries");
                XMLNode* weightedShiftsNode = XMLUtils::getChildNode(child, "WeightedShifts");
                if ((shiftsNode != nullptr) && (shiftExpiriesNode != nullptr)) {
                    FXVolShiftData data;
                    data.mode = FXVolShiftData::AtmShiftMode::Explicit;
                    data.shiftType = shiftType;
                    data.shifts = XMLUtils::getNodeValueAsDoublesCompact(shiftsNode);
                    data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
                    QL_REQUIRE(data.shifts.size() == data.shiftExpiries.size(),
                               "Length of shifts " << data.shifts.size() << " does not match length of shiftExpiries "
                                                   << data.shiftExpiries.size()
                                                   << ". Please check stresstest config for FxVol " << ccypair);
                    test.fxVolShifts[ccypair] = ext::make_shared<FXVolShiftData>(data);
                } else if (weightedShiftsNode != nullptr) {
                    std::string weightingSchema = XMLUtils::getChildValue(weightedShiftsNode, "WeightingSchema", true);
                    boost::to_lower(weightingSchema);
                    FXVolShiftData data;
                    data.shiftType = shiftType;
                    if (weightingSchema == "unadjusted"){
                        data.mode = FXVolShiftData::AtmShiftMode::Unadjusted;
                        data.shifts = std::vector<Real>(1, XMLUtils::getChildValueAsDouble(weightedShiftsNode, "Shift", true));
                        data.shiftExpiries =
                            std::vector<Period>(1, XMLUtils::getChildValueAsPeriod(weightedShiftsNode, "Tenor", true));
                    } else if(weightingSchema == "weighted"){
                        data.mode = FXVolShiftData::AtmShiftMode::Weighted;
                        data.shifts =
                            std::vector<Real>(1, XMLUtils::getChildValueAsDouble(weightedShiftsNode, "Shift", true));
                        data.shiftExpiries =
                            std::vector<Period>(1, XMLUtils::getChildValueAsPeriod(weightedShiftsNode, "Tenor", true));
                        data.weights =
                            XMLUtils::getChildrenValuesAsDoublesCompact(weightedShiftsNode, "ShiftWeights", true);
                        data.weightTenors =
                            XMLUtils::getChildrenValuesAsPeriods(weightedShiftsNode, "WeightTenors", true);
                        QL_REQUIRE(data.weights.size() == data.weightTenors.size(),
                                   "Length of weights "
                                       << data.weights.size() << " does not match length of weightTenors "
                                       << data.weightTenors.size() << ". Please check stresstest config for FxVol "
                                       << ccypair);
                        
                    } else{
                        QL_FAIL("FxVolStressTestData: unexpected weighting scheme, got "
                                << weightingSchema << " expected 'unadjusted' or 'weighted', please check config for "<< ccypair);
                    }
                    test.fxVolShifts[ccypair] = ext::make_shared<FXVolShiftData>(data);
                } else{
                    QL_FAIL("Expect either Shifts and Shiftsexpiries nodes or a WeightedShifts node, please check config "
                         "for FxVolStressScenario "
                         << ccypair);
                }
            }
        }
        LOG("Get Equity spot stress parameters");
        XMLNode* equitySpots = XMLUtils::getChildNode(testCase, "EquitySpots");
        test.equityShifts.clear();
        if (equitySpots) {
            for (XMLNode* child = XMLUtils::getChildNode(equitySpots, "EquitySpot"); child;
                 child = XMLUtils::getNextSibling(child)) {
                string equity = XMLUtils::getAttribute(child, "equity");
                LOG("Loading stress parameters for Equity " << equity);
                SpotShiftData data;
                data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
                data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
                test.equityShifts[equity] = ext::make_shared<SpotShiftData>(data);
            }
        }
        LOG("Get equity vol stress parameters");
        XMLNode* equityVols = XMLUtils::getChildNode(testCase, "EquityVolatilities");
        QL_REQUIRE(equityVols, "EquityVolatilities node not found");
        test.equityVolShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(equityVols, "EquityVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string equity = XMLUtils::getAttribute(child, "equity");
            LOG("Loading stress parameters for Equity vols " << equity);
            VolShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType"));
            data.shifts = XMLUtils::getChildrenValuesAsDoublesCompact(child, "Shifts", true);
            data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
            test.equityVolShifts[equity] = ext::make_shared<VolShiftData>(data);
        }

        LOG("Get swaption vol stress parameters");
        XMLNode* swaptionVols = XMLUtils::getChildNode(testCase, "SwaptionVolatilities");
        QL_REQUIRE(swaptionVols, "SwaptionVols node not found");
        test.swaptionVolShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(swaptionVols, "SwaptionVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {

            string key = XMLUtils::getAttribute(child, "key");
            if (key.empty()) {
                string ccyAttr = XMLUtils::getAttribute(child, "ccy");
                if (!ccyAttr.empty()) {
                    key = ccyAttr;
                    WLOG("StressScenarioData: attribute 'ccy' for SwaptionVolatilities is deprecated, use 'key' "
                         "instead.");
                }
            }
            LOG("Loading stress parameters for swaption vols " << key);
            SwaptionVolShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shiftTerms = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTerms", true);
            data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
            XMLNode* shiftSizes = XMLUtils::getChildNode(child, "Shifts");
            data.parallelShiftSize = 0.0;
            for (XMLNode* child2 = XMLUtils::getChildNode(shiftSizes, "Shift"); child2;
                 child2 = XMLUtils::getNextSibling(child2)) {
                string expiry = XMLUtils::getAttribute(child2, "expiry");
                string term = XMLUtils::getAttribute(child2, "term");
                if (expiry == "" && term == "")
                    data.parallelShiftSize = ore::data::parseReal(XMLUtils::getNodeValue(child2));
                else {
                    QL_REQUIRE(expiry != "" && term != "", "expiry and term attributes required on shift size nodes");
                    Period e = ore::data::parsePeriod(expiry);
                    Period t = ore::data::parsePeriod(term);
                    Real value = ore::data::parseReal(XMLUtils::getNodeValue(child2));
                    pair<Period, Period> shiftKey(e, t);
                    data.shifts[shiftKey] = value;
                }
            }
            test.swaptionVolShifts[key] = ext::make_shared<SwaptionVolShiftData>(data);
        }

        LOG("Get cap/floor vol stress parameters");
        XMLNode* capVols = XMLUtils::getChildNode(testCase, "CapFloorVolatilities");
        QL_REQUIRE(capVols, "CapVols node not found");
        test.capVolShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(capVols, "CapFloorVolatility"); child;
             child = XMLUtils::getNextSibling(child)) {
            string key = XMLUtils::getAttribute(child, "key");
            if (key.empty()) {
                string ccyAttr = XMLUtils::getAttribute(child, "ccy");
                if (!ccyAttr.empty()) {
                    key = ccyAttr;
                    WLOG("StressScenarioData: 'ccy' is deprecated as an attribute for CapFloorVolatilities, use 'key' "
                         "instead.");
                }
            }
            CapFloorVolShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
            data.shiftStrikes = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", false);
            XMLNode* shiftSizesNode = XMLUtils::getChildNode(child, "Shifts");
            for (XMLNode* shiftNode = XMLUtils::getChildNode(shiftSizesNode, "Shift"); shiftNode;
                 shiftNode = XMLUtils::getNextSibling(shiftNode)) {
                Period tenor = ore::data::parsePeriod(XMLUtils::getAttribute(shiftNode, "tenor"));
                data.shifts[tenor] = XMLUtils::getNodeValueAsDoublesCompact(shiftNode);
                QL_REQUIRE((data.shiftStrikes.empty() && data.shifts[tenor].size() == 1) ||
                               (data.shifts[tenor].size() == data.shiftStrikes.size()),
                           "StressScenarioData: CapFloor " << key << ": Mismatch between size of strikes ("
                                                           << data.shiftStrikes.size() << ") and shifts ("
                                                           << data.shifts[tenor].size() << ") for tenor "
                                                           << ore::data::to_string(tenor));
            }
            QL_REQUIRE(data.shifts.size() == data.shiftExpiries.size(),
                       "StressScenarioData: CapFloor " << key << ": Mismatch between size of expiries ("
                                                       << data.shiftExpiries.size() << ") and shifts("
                                                       << data.shifts.size() << ")");
            test.capVolShifts[key] = ext::make_shared<CapFloorVolShiftData>(data);
        }
        LOG("Get Security spread stress parameters");
        XMLNode* securitySpreads = XMLUtils::getChildNode(testCase, "SecuritySpreads");
        QL_REQUIRE(securitySpreads, "SecuritySpreads node not found");
        test.securitySpreadShifts.clear();
        for (XMLNode* child = XMLUtils::getChildNode(securitySpreads, "SecuritySpread"); child;
             child = XMLUtils::getNextSibling(child)) {
            string bond = XMLUtils::getAttribute(child, "security");
            LOG("Loading stress parameters for Security spreads " << bond);
            SpotShiftData data;
            data.shiftType = parseShiftType(XMLUtils::getChildValue(child, "ShiftType", true));
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            test.securitySpreadShifts[bond] = ext::make_shared<SpotShiftData>(data);
        }
        data_.push_back(test);
        LOG("Loading stress test label " << test.label << " done");
    }
    LOG("Loading stress tests done");
}

void curveShiftDataToXml(ore::data::XMLDocument& doc, XMLNode* node,
                         const std::map<std::string, ext::shared_ptr<StressTestScenarioData::CurveShiftData>>& data,
                         const std::string& identifier, const std::string& nodeName,
                         const std::string& parentNodeName = std::string()) {
    std::string name = parentNodeName.empty() ? nodeName + "s" : parentNodeName;
    auto parentNode = XMLUtils::addChild(doc, node, name);
    for (const auto& [key, data] : data) {
        auto childNode = XMLUtils::addChild(doc, parentNode, nodeName);
        XMLUtils::addAttribute(doc, childNode, identifier, key);
        XMLUtils::addChild(doc, childNode, "ShiftType", ore::data::to_string(data->shiftType));
        XMLUtils::addGenericChildAsList(doc, childNode, "Shifts", data->shifts);
        XMLUtils::addGenericChildAsList(doc, childNode, "ShiftTenors", data->shiftTenors);
    }
}

void volShiftDataToXml(ore::data::XMLDocument& doc, XMLNode* node, const std::map<std::string, ext::shared_ptr<StressTestScenarioData::VolShiftData>>& data,
                       const std::string& identifier, const std::string& nodeName, const std::string& parentNodeName) {
    auto parentNode = XMLUtils::addChild(doc, node, parentNodeName);
    for (const auto& [key, data] : data) {
        auto childNode = XMLUtils::addChild(doc, parentNode, nodeName);
        XMLUtils::addAttribute(doc, childNode, identifier, key);
        XMLUtils::addChild(doc, childNode, "ShiftType", ore::data::to_string(data->shiftType));
        XMLUtils::addGenericChildAsList(doc, childNode, "Shifts", data->shifts);
        XMLUtils::addGenericChildAsList(doc, childNode, "ShiftExpiries", data->shiftExpiries);
    }
}


void fxVolDataToXml(ore::data::XMLDocument& doc, XMLNode* node, const std::map < std::string, ext::shared_ptr<StressTestScenarioData::FXVolShiftData>>& shiftdata,
                       const std::string& identifier, const std::string& nodeName, const std::string& parentNodeName) {
    auto parentNode = XMLUtils::addChild(doc, node, parentNodeName);
    for (const auto& [key, data] : shiftdata) {
        if (data->mode == StressTestScenarioData::FXVolShiftData::AtmShiftMode::Explicit) {
            auto childNode = XMLUtils::addChild(doc, parentNode, nodeName);
            XMLUtils::addAttribute(doc, childNode, identifier, key);
            XMLUtils::addChild(doc, childNode, "ShiftType", ore::data::to_string(data->shiftType));
            XMLUtils::addGenericChildAsList(doc, childNode, "Shifts", data->shifts);
            XMLUtils::addGenericChildAsList(doc, childNode, "ShiftExpiries", data->shiftExpiries);
        } else if (data->mode == StressTestScenarioData::FXVolShiftData::AtmShiftMode::Unadjusted) {
            auto childNode = XMLUtils::addChild(doc, parentNode, nodeName);
            XMLUtils::addAttribute(doc, childNode, identifier, key);
            auto weightedShiftsNode = XMLUtils::addChild(doc, childNode, "WeightedShifts");
            XMLUtils::addChild(doc, weightedShiftsNode, "WeightingSchema", "Unadjusted");
            XMLUtils::addChild(doc, weightedShiftsNode, "Shift", data->shifts.front());
            XMLUtils::addChild(doc, weightedShiftsNode, "Tenor", data->shiftExpiries.front());
        } else if (data->mode == StressTestScenarioData::FXVolShiftData::AtmShiftMode::Weighted) {
            QL_REQUIRE(data->shifts.size() == 1, "Internal Error: WeightedShift should have only one shift, please check construction of FxVolShiftData");
            QL_REQUIRE(
                data->shiftExpiries.size() == 1,
                "Internal Error: WeightedShift should have only one shift, please check construction of FxVolShiftData");
            auto childNode = XMLUtils::addChild(doc, parentNode, nodeName);
            XMLUtils::addAttribute(doc, childNode, identifier, key);
            auto weightedShiftsNode = XMLUtils::addChild(doc, childNode, "WeightedShifts");
            XMLUtils::addChild(doc, weightedShiftsNode, "WeightingSchema", "Weighted");
            XMLUtils::addChild(doc, weightedShiftsNode, "Shift", data->shifts.front());
            XMLUtils::addChild(doc, weightedShiftsNode, "Tenor", data->shiftExpiries.front());
            XMLUtils::addGenericChildAsList(doc, weightedShiftsNode, "ShiftWeights", data->weights);
            XMLUtils::addGenericChildAsList(doc, weightedShiftsNode, "WeightTenors", data->weightTenors);
        } else {
            QL_FAIL("internal error: unexpected FxVolShiftData, expected Explicit Shifts, Unadjusted or weighted shifts, contact dev");
        }
    }
}





void spotShiftDataToXml(ore::data::XMLDocument& doc, XMLNode* node,
                        const std::map<std::string, ext::shared_ptr<StressTestScenarioData::SpotShiftData>>& data,
                        const std::string& identifier, const std::string& nodeName) {
    auto parentNode = XMLUtils::addChild(doc, node, nodeName + "s");
    for (const auto& [key, data] : data) {
        auto childNode = XMLUtils::addChild(doc, parentNode, nodeName);
        XMLUtils::addAttribute(doc, childNode, identifier, key);
        XMLUtils::addChild(doc, childNode, "ShiftType", ore::data::to_string(data->shiftType));
        XMLUtils::addChild(doc, childNode, "ShiftSize", data->shiftSize);
    }
}

XMLNode* StressTestScenarioData::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("StressTesting");

    XMLUtils::addChild(doc, node, "UseSpreadedTermStructures", ore::data::to_string(useSpreadedTermStructures_));
    for (const auto& test : data_) {
        // Add test node
        auto testNode = XMLUtils::addChild(doc, node, "StressTest");
        XMLUtils::addAttribute(doc, testNode, "id", test.label);
        // Add Par Shifts node
        auto parShiftsNode = XMLUtils::addChild(doc, testNode, "ParShifts");
        XMLUtils::addChild(doc, parShiftsNode, "IRCurves", test.irCurveParShifts);
        XMLUtils::addChild(doc, parShiftsNode, "CapFloorVolatilities", test.irCapFloorParShifts);
        XMLUtils::addChild(doc, parShiftsNode, "SurvivalProbability", test.creditCurveParShifts);
        // IR
        curveShiftDataToXml(doc, testNode, test.discountCurveShifts, "ccy", "DiscountCurve");
        curveShiftDataToXml(doc, testNode, test.indexCurveShifts, "index", "IndexCurve");
        curveShiftDataToXml(doc, testNode, test.yieldCurveShifts, "name", "YieldCurve");

        LOG("Write capFloor vol stress parameters");
        XMLNode* capFloorVolsNode = XMLUtils::addChild(doc, testNode, "CapFloorVolatilities");
        for (const auto& [key, data] : test.capVolShifts) {
            XMLNode* capFloorVolNode = XMLUtils::addChild(doc, capFloorVolsNode, "CapFloorVolatility");
            XMLUtils::addAttribute(doc, capFloorVolNode, "key", key);
            XMLUtils::addChild(doc, capFloorVolNode, "ShiftType", ore::data::to_string(data->shiftType));
            XMLNode* shiftSizesNode = XMLUtils::addChild(doc, capFloorVolNode, "Shifts");
            for (const auto& [tenor, shifts] : data->shifts) {
                XMLUtils::addGenericChildAsList(doc, shiftSizesNode, "Shift", shifts, "tenor",
                                                ore::data::to_string(tenor));
            }
            XMLUtils::addGenericChildAsList(doc, capFloorVolNode, "ShiftExpiries", data->shiftExpiries);
            XMLUtils::addGenericChildAsList(doc, capFloorVolNode, "ShiftStrikes", data->shiftStrikes);
        }
        // SwaptionVolData
        // TODO: SwaptionVolData Missing
        LOG("Write swaption vol stress parameters");
        XMLNode* swaptionVolsNode = XMLUtils::addChild(doc, testNode, "SwaptionVolatilities");
        const std::vector<std::string> swaptionAttributeNames = {"expiry", "term"};
        for (const auto& [key, data] : test.swaptionVolShifts) {
            XMLNode* swaptionVolNode = XMLUtils::addChild(doc, swaptionVolsNode, "SwaptionVolatility");
            XMLUtils::addAttribute(doc, swaptionVolNode, "key", key);
            XMLUtils::addChild(doc, swaptionVolNode, "ShiftType", ore::data::to_string(data->shiftType));
            
            XMLNode* shiftSizesNode = XMLUtils::addChild(doc, swaptionVolNode, "Shifts");

            if (data->shifts.empty()) {
                XMLUtils::addChild(doc, shiftSizesNode, "Shift", ore::data::to_string(data->parallelShiftSize),
                                   swaptionAttributeNames, {"", ""});
            } else {
                for (const auto& [key, shift] : data->shifts) {
                    const auto& [expiry, term] = key;
                    std::vector<std::string> attributeValues = {ore::data::to_string(expiry),
                                                                ore::data::to_string(term)};
                    XMLUtils::addChild(doc, shiftSizesNode, "Shift", ore::data::to_string(shift),
                                       swaptionAttributeNames, attributeValues);
                }
            }
            XMLUtils::addGenericChildAsList(doc, swaptionVolNode, "ShiftExpiries", data->shiftExpiries);
            XMLUtils::addGenericChildAsList(doc, swaptionVolNode, "ShiftTerms", data->shiftTerms);
            
        }
        // Credit
        curveShiftDataToXml(doc, testNode, test.survivalProbabilityShifts, "name", "SurvivalProbability",
                            "SurvivalProbabilities");
        spotShiftDataToXml(doc, testNode, test.recoveryRateShifts, "id", "RecoveryRate");
        spotShiftDataToXml(doc, testNode, test.securitySpreadShifts, "security", "SecuritySpread");
        // Equity
        spotShiftDataToXml(doc, testNode, test.equityShifts, "equity", "EquitySpot");
        volShiftDataToXml(doc, testNode, test.equityVolShifts, "equity", "EquityVolatility", "EquityVolatilities");
        // FX
        spotShiftDataToXml(doc, testNode, test.fxShifts, "ccypair", "FxSpot");
        fxVolDataToXml(doc, testNode, test.fxVolShifts, "ccypair", "FxVolatility", "FxVolatilities");
    }
    return node;
}
} // namespace analytics
} // namespace ore
