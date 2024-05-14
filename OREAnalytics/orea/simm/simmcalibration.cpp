/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/simm/simmcalibration.hpp>

#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

using QuantLib::Size;
using QuantLib::Real;
using std::map;
using std::make_pair;
using std::pair;
using std::tuple;
using std::string;
using std::vector;
using std::set;
using ore::data::XMLUtils;
using ore::data::XMLNode;
using ore::data::XMLDocument;


namespace ore {
namespace analytics {

namespace {

Size getMPOR(XMLNode* node) {
    string mporStr = XMLUtils::getAttribute(node, "mporDays");
    return mporStr.empty() ? 10 : ore::data::parseInteger(mporStr);
}

}

using RC = SimmConfiguration::RiskClass;
using RT = CrifRecord::RiskType;

SimmCalibration::Amount::Amount(const tuple<string, string, string>& key, const string& value) {
    bucket_ = std::get<0>(key);
    label1_ = std::get<1>(key);
    label2_ = std::get<2>(key);
    value_ = value;
}

XMLNode* SimmCalibration::Amount::toXML(XMLDocument& doc) const {
    auto amountNode = doc.allocNode("Amount", value_);
    if (!bucket_.empty())
        XMLUtils::addAttribute(doc, amountNode, "bucket", bucket_);
    if (!label1_.empty())
        XMLUtils::addAttribute(doc, amountNode, "label1", label1_);
    if (!label2_.empty())
        XMLUtils::addAttribute(doc, amountNode, "label2", label2_);
    return amountNode;
}

void SimmCalibration::Amount::fromXML(XMLNode* node) {
    bucket_ = XMLUtils::getAttribute(node, "bucket");
    label1_ = XMLUtils::getAttribute(node, "label1");
    label2_ = XMLUtils::getAttribute(node, "label2");
    value_ = XMLUtils::getNodeValue(node);
}

const tuple<string, string, string> SimmCalibration::Amount::key() const {
    return std::make_tuple(bucket_, label1_, label2_);
}

SimmCalibration::RiskClassData::RiskWeights::RiskWeights(const RC& riskClass, XMLNode* node) : riskClass_(riskClass) {
    fromXML(node);
}

XMLNode* SimmCalibration::RiskClassData::RiskWeights::toXML(XMLDocument& doc) const {
    auto riskWeightsNode = doc.allocNode("RiskWeights");

    // Delta and Vega risk weights
    for (const string& rwType : {"Delta", "Vega"}) {
        auto& riskWeightsMap = rwType == "Delta" ? delta_ : vega_;

        for (const auto& [mpor, riskWeights] : riskWeightsMap) {
            auto rwTypeNode = doc.allocNode(rwType);
            XMLUtils::addAttribute(doc, rwTypeNode, "mporDays", ore::data::to_string(mpor));
            for (const auto& [rwKey, weight] : riskWeights) {
                Amount amount(rwKey, weight);
                auto amountNode = amount.toXML(doc);
                XMLUtils::setNodeName(doc, amountNode, "Weight");
                XMLUtils::appendNode(rwTypeNode, amountNode);
            }
            XMLUtils::appendNode(riskWeightsNode, rwTypeNode);
        }
    }

    // Historical volatility ratio
    for (const auto& [mpor, amount] : historicalVolatilityRatio_) {
        auto hvrNode = amount->toXML(doc);
        XMLUtils::setNodeName(doc, hvrNode, "HistoricalVolatilityRatio");
        XMLUtils::addAttribute(doc, hvrNode, "mporDays", ore::data::to_string(mpor));
        XMLUtils::appendNode(riskWeightsNode, hvrNode);
    }

    return riskWeightsNode;
}

void SimmCalibration::RiskClassData::RiskWeights::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "RiskWeights");

    // Delta and Vega risk weights
    for (const string& rwType : {"Delta", "Vega"}) {
        auto& riskWeightsMap = rwType == "Delta" ? delta_ : vega_;
        auto rwNodes = XMLUtils::getChildrenNodes(node, rwType);
        for (XMLNode* rwNode : rwNodes) {
            Size mpor = getMPOR(rwNode);
            auto weightNodes = XMLUtils::getChildrenNodes(rwNode, "Weight");
            riskWeightsMap[mpor].clear();
            for (XMLNode* weightNode : weightNodes) {
                Amount amount(weightNode);
                riskWeightsMap[mpor][amount.key()] = amount.value();
            }
        }
    }

    // Historical volatility ratio
    auto hvrNodes = XMLUtils::getChildrenNodes(node, "HistoricalVolatilityRatio");
    for (XMLNode* hvrNode : hvrNodes) {
        Size mpor = getMPOR(hvrNode);
        auto hvr = QuantLib::ext::make_shared<Amount>(hvrNode);
        historicalVolatilityRatio_[mpor] = hvr;
    }
}

SimmCalibration::RiskClassData::IRRiskWeights::IRRiskWeights(XMLNode* node) : RiskWeights(RC::InterestRate) {
    fromXML(node);
}

XMLNode* SimmCalibration::RiskClassData::IRRiskWeights::toXML(XMLDocument& doc) const {
    auto riskWeightsNode = RiskWeights::toXML(doc);

    // Inflation and XCcyBasis
    for (const string& rwType : {"Inflation", "XCcyBasis"}) {
        auto container = rwType == "Inflation" ? inflation_ : xCcyBasis_;
        for (const auto& [mpor, amount] : container) {
            auto rwNode = amount->toXML(doc);
            XMLUtils::setNodeName(doc, rwNode, rwType);
            XMLUtils::addAttribute(doc, rwNode, "mporDays", ore::data::to_string(mpor));
            XMLUtils::appendNode(riskWeightsNode, rwNode);
        }
    }

    // Currency lists
    auto currencyListsNode = doc.allocNode("CurrencyLists");
    for (const auto& [ccyKey, ccyList] : currencyLists_) {
        for (const string& ccy : ccyList) {
            Amount amount(ccyKey, ccy);
            auto ccyNode = amount.toXML(doc);
            XMLUtils::setNodeName(doc, ccyNode, "Currency");
            XMLUtils::appendNode(currencyListsNode, ccyNode);
        }
    }
    XMLUtils::appendNode(riskWeightsNode, currencyListsNode);

    return riskWeightsNode;
}

void SimmCalibration::RiskClassData::IRRiskWeights::fromXML(XMLNode* node) {
    RiskWeights::fromXML(node);

    for (const string& weightType : {"Inflation", "XCcyBasis"}) {
        auto& weightMap = weightType == "Inflation" ? inflation_ : xCcyBasis_;
        weightMap.clear();
        auto weightTypeNodes = XMLUtils::getChildrenNodes(node, weightType);
        for (XMLNode* weightTypeNode : weightTypeNodes) {
            Size mpor = getMPOR(weightTypeNode);
            auto amount = QuantLib::ext::make_shared<Amount>(weightTypeNode);
            weightMap[mpor] = amount;
        }
    }

    // Currency lists
    auto ccyListsNode = XMLUtils::getChildNode(node, "CurrencyLists");
    currencyLists_.clear();
    for (XMLNode* ccyNode : XMLUtils::getChildrenNodes(ccyListsNode, "Currency")) {
        Amount amount(ccyNode);
        currencyLists_[amount.key()].insert(amount.value());
    }
}

const std::map<RT, map<Size, QuantLib::ext::shared_ptr<SimmCalibration::Amount>>>
SimmCalibration::RiskClassData::IRRiskWeights::uniqueRiskWeights() const {
    std::map<RT, map<Size, QuantLib::ext::shared_ptr<SimmCalibration::Amount>>> urwMap;

    for (const auto& [mpor, rw] : inflation_)
        urwMap[RT::Inflation][mpor] = rw;
    for (const auto& [mpor, rw] : xCcyBasis_)
        urwMap[RT::XCcyBasis][mpor] = rw;

    return urwMap;
}

SimmCalibration::RiskClassData::FXRiskWeights::FXRiskWeights(XMLNode* node) : RiskWeights(RC::FX) {
    fromXML(node);
}

XMLNode* SimmCalibration::RiskClassData::FXRiskWeights::toXML(XMLDocument& doc) const {
    auto riskWeightsNode = RiskWeights::toXML(doc);

    // Currency lists
    auto currencyListsNode = doc.allocNode("CurrencyLists");
    for (const auto& [ccyKey, ccyList] : currencyLists_) {
        for (const string& ccy : ccyList) {
            Amount amount(ccyKey, ccy);
            auto ccyNode = amount.toXML(doc);
            XMLUtils::setNodeName(doc, ccyNode, "Currency");
            XMLUtils::appendNode(currencyListsNode, ccyNode);
        }
    }
    XMLUtils::appendNode(riskWeightsNode, currencyListsNode);

    return riskWeightsNode;
}

void SimmCalibration::RiskClassData::FXRiskWeights::fromXML(XMLNode* node) {
    RiskWeights::fromXML(node);

    // Currency lists
    auto ccyListsNode = XMLUtils::getChildNode(node, "CurrencyLists");
    currencyLists_.clear();
    for (XMLNode* ccyNode : XMLUtils::getChildrenNodes(ccyListsNode, "Currency")) {
        Amount amount(ccyNode);
        currencyLists_[amount.key()].insert(amount.value());
    }
}

SimmCalibration::RiskClassData::CreditQRiskWeights::CreditQRiskWeights(XMLNode* node)
    : RiskWeights(RC::CreditQualifying) {
    fromXML(node);
}

XMLNode* SimmCalibration::RiskClassData::CreditQRiskWeights::toXML(XMLDocument& doc) const {
    auto riskWeightsNode = RiskWeights::toXML(doc);

    // Base correlation
    for (const auto& [mpor, amount] : baseCorrelation_) {
        auto rwNode = amount->toXML(doc);
        XMLUtils::setNodeName(doc, rwNode, "BaseCorrelation");
        XMLUtils::addAttribute(doc, rwNode, "mporDays", ore::data::to_string(mpor));
        XMLUtils::appendNode(riskWeightsNode, rwNode);
    }

    return riskWeightsNode;
}

void SimmCalibration::RiskClassData::CreditQRiskWeights::fromXML(XMLNode* node) {
    RiskWeights::fromXML(node);

    // Base correlation
    auto baseCorrNodes = XMLUtils::getChildrenNodes(node, "BaseCorrelation");
    for (XMLNode* baseCorrNode : baseCorrNodes) {
        Size mpor = getMPOR(baseCorrNode);
        auto baseCorrelation = QuantLib::ext::make_shared<Amount>(baseCorrNode);
        baseCorrelation_[mpor] = baseCorrelation;
    }
}

const std::map<RT, map<Size, QuantLib::ext::shared_ptr<SimmCalibration::Amount>>>
SimmCalibration::RiskClassData::CreditQRiskWeights::uniqueRiskWeights() const {
    std::map<RT, map<Size, QuantLib::ext::shared_ptr<SimmCalibration::Amount>>> urwMap;

    for (const auto& [mpor, rw] : baseCorrelation_)
        urwMap[RT::BaseCorr][mpor] = rw;

    return urwMap;
}

XMLNode* SimmCalibration::RiskClassData::Correlations::toXML(XMLDocument& doc) const {
    auto correlationsNode = doc.allocNode("Correlations");

    // Intra- and Inter-bucket correlations
    for (const string& corrType : {"IntraBucket", "InterBucket"}) {
        auto& correlations = corrType == "IntraBucket" ? intraBucketCorrelations_ : interBucketCorrelations_;
        
        if (correlations.empty())
            continue;

        auto corrTypeNode = doc.allocNode(corrType);
        for (const auto& [corrKey, corr] : correlations) {
            Amount amount(corrKey, corr);
            auto correlationNode = amount.toXML(doc);
            XMLUtils::setNodeName(doc, correlationNode, "Correlation");
            XMLUtils::appendNode(corrTypeNode, correlationNode);
        }
        XMLUtils::appendNode(correlationsNode, corrTypeNode);
    }

    return correlationsNode;
}

void SimmCalibration::RiskClassData::Correlations::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Correlations");

    // Intra- and Inter-bucket correlations
    for (const string& corrType : {"IntraBucket", "InterBucket"}) {
        auto& correlationsMap = corrType == "IntraBucket" ? intraBucketCorrelations_ : interBucketCorrelations_;
        correlationsMap.clear();
        auto corrNodes = XMLUtils::getChildrenNodes(node, corrType);
        for (XMLNode* corrNode : corrNodes) {
            for (XMLNode* weightNode : XMLUtils::getChildrenNodes(corrNode, "Correlation")) {
                Amount amount(weightNode);
                correlationsMap[amount.key()] = amount.value();
            }
        }
    }
}

XMLNode* SimmCalibration::RiskClassData::IRCorrelations::toXML(XMLDocument& doc) const {
    auto correlationsNode = Correlations::toXML(doc);

    auto corrTypes = map<string, QuantLib::ext::shared_ptr<Amount>>(
        {{"SubCurves", subCurves_}, {"Inflation", inflation_}, {"XCcyBasis", xCcyBasis_}, {"Outer", outer_}});

    for (const auto& [corrType, container] : corrTypes) {
        auto correlationNode = container->toXML(doc);
        XMLUtils::setNodeName(doc, correlationNode, corrType);
        XMLUtils::appendNode(correlationsNode, correlationNode);
    }

    return correlationsNode;
}

void SimmCalibration::RiskClassData::IRCorrelations::fromXML(XMLNode* node) {
    Correlations::fromXML(node);

    auto corrTypes = map<string, QuantLib::ext::shared_ptr<Amount>&>(
        {{"SubCurves", subCurves_}, {"Inflation", inflation_}, {"XCcyBasis", xCcyBasis_}, {"Outer", outer_}});

    for (auto& [corrType, container] : corrTypes) {
        auto corrNode = XMLUtils::getChildNode(node, corrType);
        container = QuantLib::ext::make_shared<Amount>(corrNode);
    }
}

XMLNode* SimmCalibration::RiskClassData::CreditQCorrelations::toXML(XMLDocument& doc) const {
    auto correlationsNode = Correlations::toXML(doc);

    auto baseCorrelationNode = baseCorrelation_->toXML(doc);
    XMLUtils::setNodeName(doc, baseCorrelationNode, "BaseCorrelation");
    XMLUtils::appendNode(correlationsNode, baseCorrelationNode);

    return correlationsNode;
}

void SimmCalibration::RiskClassData::CreditQCorrelations::fromXML(XMLNode* node) {
    Correlations::fromXML(node);

    auto baseCorrelationNode = XMLUtils::getChildNode(node, "BaseCorrelation");
    baseCorrelation_ = QuantLib::ext::make_shared<Amount>(baseCorrelationNode);
}

XMLNode* SimmCalibration::RiskClassData::FXCorrelations::toXML(XMLDocument& doc) const {
    auto correlationsNode = Correlations::toXML(doc);

    auto volatilityNode = volatility_->toXML(doc);
    XMLUtils::setNodeName(doc, volatilityNode, "Volatility");
    XMLUtils::appendNode(correlationsNode, volatilityNode);

    return correlationsNode;
}

void SimmCalibration::RiskClassData::FXCorrelations::fromXML(XMLNode* node) {
    Correlations::fromXML(node);

    auto volatilityNode = XMLUtils::getChildNode(node, "Volatility");
    volatility_ = QuantLib::ext::make_shared<Amount>(volatilityNode);
}

XMLNode* SimmCalibration::RiskClassData::ConcentrationThresholds::toXML(XMLDocument& doc) const {
    auto concThresholdsNode = doc.allocNode("ConcentrationThresholds");

    // Delta and Vega risk weights
    for (const string& type : {"Delta", "Vega"}) {
        auto& concThresholds = type == "Delta" ? delta_ : vega_;

        auto typeNode = doc.allocNode(type);
        for (const auto& [ctKey, threshold] : concThresholds) {
            Amount amount(ctKey, threshold);
            auto amountNode = amount.toXML(doc);
            XMLUtils::setNodeName(doc, amountNode, "Threshold");
            XMLUtils::appendNode(typeNode, amountNode);
        }
        XMLUtils::appendNode(concThresholdsNode, typeNode);
    }

    return concThresholdsNode;
}

void SimmCalibration::RiskClassData::ConcentrationThresholds::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ConcentrationThresholds");

    // Delta and Vega risk weights
    for (const string& concThresholdType : {"Delta", "Vega"}) {
        auto& concThresholdsMap = concThresholdType == "Delta" ? delta_ : vega_;
        concThresholdsMap.clear();
        auto concThresholdNodes = XMLUtils::getChildrenNodes(node, concThresholdType);
        for (XMLNode* concThresholdNode : concThresholdNodes) {
            auto thresholdNodes = XMLUtils::getChildrenNodes(concThresholdNode, "Threshold");
            for (XMLNode* thresholdNode : thresholdNodes) {
                Amount amount(thresholdNode);
                concThresholdsMap[amount.key()] = amount.value();
            }
        }
    }

}

XMLNode* SimmCalibration::RiskClassData::IRFXConcentrationThresholds::toXML(XMLDocument& doc) const {
    auto concThresholdsNode = ConcentrationThresholds::toXML(doc);

    // Currency lists
    auto currencyListsNode = doc.allocNode("CurrencyLists");
    for (const auto& [ccyKey, ccyList] : currencyLists_) {
        for (const string& ccy : ccyList) {
            Amount amount(ccyKey, ccy);
            auto ccyNode = amount.toXML(doc);
            XMLUtils::setNodeName(doc, ccyNode, "Currency");
            XMLUtils::appendNode(currencyListsNode, ccyNode);
        }
    }
    XMLUtils::appendNode(concThresholdsNode, currencyListsNode);

    return concThresholdsNode;
}

void SimmCalibration::RiskClassData::IRFXConcentrationThresholds::fromXML(XMLNode* node) {
    ConcentrationThresholds::fromXML(node);

    // Currency lists
    auto ccyListsNode = XMLUtils::getChildNode(node, "CurrencyLists");
    currencyLists_.clear();
    for (XMLNode* ccyNode : XMLUtils::getChildrenNodes(ccyListsNode, "Currency")) {
        Amount amount(ccyNode);
        currencyLists_[amount.key()].insert(amount.value());
    }
}

XMLNode* SimmCalibration::RiskClassData::toXML(XMLDocument& doc) const {
    auto riskClassNode = doc.allocNode(ore::data::to_string(riskClass_));

    // Risk weights
    XMLUtils::appendNode(riskClassNode, riskWeights_->toXML(doc));

    // Correlations
    XMLUtils::appendNode(riskClassNode, correlations_->toXML(doc));

    // Concentration thresholds
    XMLUtils::appendNode(riskClassNode, concentrationThresholds_->toXML(doc));
    
    return riskClassNode;
}

void SimmCalibration::RiskClassData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, ore::data::to_string(riskClass_));

    // Risk weights
    XMLNode* riskWeightsNode = XMLUtils::getChildNode(node, "RiskWeights");
    switch (riskClass_) {
    case (RC::InterestRate):
        riskWeights_ = QuantLib::ext::make_shared<IRRiskWeights>(riskWeightsNode);
        break;
    case (RC::CreditQualifying):
        riskWeights_ = QuantLib::ext::make_shared<CreditQRiskWeights>(riskWeightsNode);
        break;
    case (RC::FX):
        riskWeights_ = QuantLib::ext::make_shared<FXRiskWeights>(riskWeightsNode);
        break;
    default:
        riskWeights_ = QuantLib::ext::make_shared<RiskWeights>(riskClass_, riskWeightsNode);
    }

    // Correlations
    XMLNode* correlationsNode = XMLUtils::getChildNode(node, "Correlations");
    switch (riskClass_) {
    case (RC::InterestRate):
        correlations_ = QuantLib::ext::make_shared<IRCorrelations>(correlationsNode);
        break;
    case (RC::CreditQualifying):
        correlations_ = QuantLib::ext::make_shared<CreditQCorrelations>(correlationsNode);
        break;
    case (RC::FX):
        correlations_ = QuantLib::ext::make_shared<FXCorrelations>(correlationsNode);
        break;
    default:
        correlations_ = QuantLib::ext::make_shared<Correlations>(correlationsNode);
    }

    // Concentration Thresholds
    XMLNode* concentrationThresholdsNode = XMLUtils::getChildNode(node, "ConcentrationThresholds");
    if (riskClass_ == RC::InterestRate || riskClass_ == RC::FX) {
        concentrationThresholds_ = QuantLib::ext::make_shared<IRFXConcentrationThresholds>(concentrationThresholdsNode);
    } else {
        concentrationThresholds_ = QuantLib::ext::make_shared<ConcentrationThresholds>(concentrationThresholdsNode);
    }
}

const string& SimmCalibration::version() const { return versionNames_.front(); }

XMLNode* SimmCalibration::toXML(XMLDocument& doc) const {
    XMLNode* simmCalibrationNode = doc.allocNode("SIMMCalibration");
    XMLUtils::addAttribute(doc, simmCalibrationNode, "id", id_);

    // Version names
    XMLNode* versionNamesNode = doc.allocNode("VersionNames");
    for (const string& vname : versionNames()) {
        XMLUtils::addChild(doc, versionNamesNode, "Name", vname);
    }
    XMLUtils::appendNode(simmCalibrationNode, versionNamesNode);

    // Additional fields
    XMLNode* additionalFieldsNode = doc.allocNode("AdditionalFields");
    for (const auto& [nodeName, nodeValue] : additionalFields()) {
        XMLUtils::addChild(doc, additionalFieldsNode, nodeName, nodeValue);
    }
    XMLUtils::appendNode(simmCalibrationNode, additionalFieldsNode);

    // Risk class-specific nodes (e.g. InterestRate, CreditQ, CreditNonQ, etc.)
    for (const auto& [riskClass, rcData] : riskClassData_) {
        const string rcString = ore::data::to_string(riskClass);
        auto rcNode = rcData->toXML(doc);
        XMLUtils::appendNode(simmCalibrationNode, rcNode);
    }

    // Risk class correlations
    XMLNode* riskClassCorrelationsNode = doc.allocNode("RiskClassCorrelations");
    for (const auto& [rcCorrKey, rcCorrelation] : riskClassCorrelations_) {
        Amount amount(rcCorrKey, rcCorrelation);
        auto corrNode = amount.toXML(doc);
        XMLUtils::setNodeName(doc, corrNode, "Correlation");
        XMLUtils::appendNode(riskClassCorrelationsNode, corrNode);
    }
    XMLUtils::appendNode(simmCalibrationNode, riskClassCorrelationsNode);
    return simmCalibrationNode;
}

void SimmCalibration::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "SIMMCalibration");

    id_ = XMLUtils::getAttribute(node, "id");

    // Version names
    XMLNode* versionNamesNode = XMLUtils::getChildNode(node, "VersionNames");
    for (XMLNode* name : XMLUtils::getChildrenNodes(versionNamesNode, "Name"))
        versionNames_.push_back(XMLUtils::getNodeValue(name));
    QL_REQUIRE(!versionNames_.empty(), "Must provide at least one version name for SIMM calibration");

    // Additional fields
    XMLNode* addFieldsNode = XMLUtils::getChildNode(node, "AdditionalFields");
    if (addFieldsNode)
        for (XMLNode* child = XMLUtils::getChildNode(addFieldsNode); child; child = XMLUtils::getNextSibling(child))
            additionalFields_.push_back(std::make_pair(XMLUtils::getNodeName(child), XMLUtils::getNodeValue(child)));

    // Risk class-specific nodes (e.g. InterestRate, CreditQ, CreditNonQ, etc.)
    XMLNode* riskClassNode;
    for (const SimmConfiguration::RiskClass& rc :
        { RC::InterestRate, RC::CreditQualifying, RC::CreditNonQualifying, RC::Equity, RC::Commodity, RC::FX }) {
        riskClassNode = XMLUtils::getChildNode(node, ore::data::to_string(rc));
        auto riskClassData = QuantLib::ext::make_shared<RiskClassData>(rc);
        riskClassData->fromXML(riskClassNode);
        riskClassData_[rc] = riskClassData;
    }

    // Risk class correlations
    auto rcCorrsNode = XMLUtils::getChildNode(node, "RiskClassCorrelations");
    riskClassCorrelations_.clear();
    for (auto rcCorrNode : XMLUtils::getChildrenNodes(rcCorrsNode, "Correlation")) {
        Amount amount(rcCorrNode);
        riskClassCorrelations_[amount.key()] = amount.value();
    }
}

XMLNode* SimmCalibrationData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("SIMMCalibrationData");
    for (const auto& [simmCalibrationId, simmCalibration] : data_) {
        XMLUtils::appendNode(node, simmCalibration->toXML(doc));
    }
    return node;
}

void SimmCalibrationData::fromXML(XMLNode* node) {
	XMLUtils::checkNode(node, "SIMMCalibrationData");
    vector<XMLNode*> simmCalibrationNodes = XMLUtils::getChildrenNodes(node, "SIMMCalibration");
    for (XMLNode* scNode : simmCalibrationNodes) {
        try {
            add(QuantLib::ext::make_shared<SimmCalibration>(scNode));
        } catch (const std::exception& ex) {
            ore::data::StructuredConfigurationErrorMessage("SIMM calibration data", "",
                                                           "SIMM calibration node failed to parse", ex.what())
                .log();
        }
    }
}

void SimmCalibrationData::add(const QuantLib::ext::shared_ptr<SimmCalibration>& simmCalibration) {

    const string configurationType = "SIMM calibration data";
    const string exceptionType = "Adding SIMM calibration";

    // Check for SIMM calibration ID duplicates
    if (data_.find(simmCalibration->id()) != data_.end()) {
        ore::data::StructuredConfigurationErrorMessage(
            configurationType, simmCalibration->id(), exceptionType,
            "Cannot add SIMM calibration data since data with the same ID already exists.")
            .log();
        return;
    }

    // Check for SIMM version name clashes
    const auto& incVersionNames = simmCalibration->versionNames();
    for (const auto& [id, sc] : data_) {
        for (const string& incName : incVersionNames) {
            for (const string& currName : sc->versionNames()) {
                if (incName == currName) {
                    const string msg = "SIMM calibration has duplicate version name '" + incName +
                                       "' (added under calibration id='" + id +
                                       "'). SIMM calibration will not be added.";
                    ore::data::StructuredConfigurationWarningMessage(configurationType, simmCalibration->id(),
                                                                     exceptionType, msg)
                        .log();
                    return;
                }
            }
        }
    }

    data_[simmCalibration->id()] = simmCalibration;
}

const QuantLib::ext::shared_ptr<SimmCalibration>& SimmCalibrationData::getById(const string& id) const {
    if (!hasId(id))
        QL_FAIL("Could not find SIMM calibration with ID '" << id << "'");

    return data_.at(id);
}

const QuantLib::ext::shared_ptr<SimmCalibration> SimmCalibrationData::getBySimmVersion(const string& version) const {
    for (const auto& kv : data_) {
        const auto& simmCalibrationData = kv.second;
        for (const string& scVersion : simmCalibrationData->versionNames()) {
            if (scVersion == version) {
                return simmCalibrationData;
            }
        }
    }

    return nullptr;
}

} // namespace analytics
} // namespace ore
