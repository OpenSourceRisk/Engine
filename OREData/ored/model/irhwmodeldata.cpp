/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/model/irhwmodeldata.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

bool HwModelData::operator==(const HwModelData& rhs) {

    if (qualifier_ != rhs.qualifier_ || calibrationType_ != rhs.calibrationType_ ||
        calibrateKappa_ != rhs.calibrateKappa_ || kappaType_ != rhs.kappaType_ || kappaTimes_ != rhs.kappaTimes_ ||
        kappaValues_ != rhs.kappaValues_ || calibrateSigma_ != rhs.calibrateSigma_ || sigmaType_ != rhs.sigmaType_ ||
        sigmaTimes_ != rhs.sigmaTimes_ || sigmaValues_ != rhs.sigmaValues_) {
        return false;
    }
    return true;
}

bool HwModelData::operator!=(const HwModelData& rhs) { return !(*this == rhs); }

void HwModelData::clear() {
    optionExpiries_.clear();
    optionTerms_.clear();
    optionStrikes_.clear();
}

void HwModelData::reset() {
    IrModelData::reset();
    calibrationType_ = CalibrationType::None;
    calibrateKappa_ = false;
    kappaType_ = ParamType::Constant;
    kappaTimes_ = {};
    kappaValues_ = {};
    calibrateSigma_ = false;
    sigmaType_ = ParamType::Constant;
    sigmaTimes_ = {};
    sigmaValues_ = {};
    pcaLoadings_ = {};
    calibratePcaSigma0_ = false;
    pcaSigma0Type_ = ParamType::Constant;
    pcaSigma0Times_ = {};
    pcaSigma0Values_ = {};
    pcaSigmaRatios_ = {};
}

void HwModelData::fromXML(XMLNode* node) {

    reset();

    qualifier_ = XMLUtils::getAttribute(node, "key");
    if (qualifier_.empty()) {
        std::string ccy = XMLUtils::getAttribute(node, "ccy");
        if (!ccy.empty()) {
            qualifier_ = ccy;
            WLOG("HwModelData: attribute ccy is deprecated, use key instead.");
        }
    }
    LOG("HwModelData with attribute (key) = " << qualifier_);

    // Mean reversion config

    if (XMLNode* reversionNode = XMLUtils::getChildNode(node, "Reversion")) {
        calibrateKappa_ = XMLUtils::getChildValueAsBool(reversionNode, "Calibrate", true);
        DLOG("Hull White mean reversion calibrate = " << calibrateKappa_);

        std::string kappaCalibrationString = XMLUtils::getChildValue(reversionNode, "ParamType", true);
        kappaType_ = parseParamType(kappaCalibrationString);
        DLOG("Hull White Kappa param type = " << kappaCalibrationString);

        kappaTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(reversionNode, "TimeGrid", true);
        LOG("Hull White Reversion time grid size = " << kappaTimes_.size());

        XMLNode* initialValuesNode = XMLUtils::getChildNode(reversionNode, "InitialValue");
        for (XMLNode* child = XMLUtils::getChildNode(initialValuesNode, "Kappa"); child;
             child = XMLUtils::getNextSibling(child, "Kappa")) {
            auto kappa_t = XMLUtils::getNodeValueAsDoublesCompact(child);
            kappaValues_.push_back(Array(kappa_t.begin(), kappa_t.end()));
        }

        QL_REQUIRE(!kappaValues_.empty(), "No initial mean reversion values given");
        for (size_t i = 1; i < kappaValues_.size(); ++i) {
            QL_REQUIRE(kappaValues_.front().size() == kappaValues_[i].size(),
                       "expect " << kappaValues_.front().size() << " factors but got " << kappaValues_[i].size()
                                 << " at time " << kappaTimes_[i]);
        }
    }

    // Volatility config

    if (XMLNode* volatilityNode = XMLUtils::getChildNode(node, "Volatility")) {
        calibrateSigma_ = XMLUtils::getChildValueAsBool(volatilityNode, "Calibrate", true);
        DLOG("Hull White volatility calibrate = " << calibrateSigma_);

        std::string sigmaParameterTypeString = XMLUtils::getChildValue(volatilityNode, "ParamType", true);
        sigmaType_ = parseParamType(sigmaParameterTypeString);
        DLOG("Hull White Volatility param type = " << sigmaParameterTypeString);

        sigmaTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(volatilityNode, "TimeGrid", true);
        DLOG("Hull White volatility time grid size = " << sigmaTimes_.size());

        XMLNode* initialSigmasNode = XMLUtils::getChildNode(volatilityNode, "InitialValue");
        for (XMLNode* sigmaNode = XMLUtils::getChildNode(initialSigmasNode, "Sigma"); sigmaNode;
             sigmaNode = XMLUtils::getNextSibling(sigmaNode, "Sigma")) {
            std::vector<std::vector<double>> matrix;
            for (XMLNode* rowNode = XMLUtils::getChildNode(sigmaNode, "Row"); rowNode;
                 rowNode = XMLUtils::getNextSibling(rowNode, "Row")) {
                matrix.push_back(XMLUtils::getNodeValueAsDoublesCompact(rowNode));
                QL_REQUIRE(kappaValues_.empty() || matrix.back().size() == kappaValues_.front().size(),
                           "Mismatch between kappa and sigma");
            }
            QL_REQUIRE(!matrix.empty(), "Sigma not provided");
            QL_REQUIRE(!matrix.front().empty(), "Sigma not provided");
            Matrix sigma(matrix.size(), matrix.front().size(), 0.0);
            for (size_t i = 0; i < sigma.rows(); ++i) {
                for (size_t j = 0; j < sigma.columns(); ++j)
                    sigma[i][j] = matrix[i][j];
            }
            sigmaValues_.push_back(sigma);
        }

        QL_REQUIRE(!sigmaValues_.empty(), "need at least one sigma matrix");
        for (size_t i = 0; i < sigmaValues_.size(); ++i) {
            QL_REQUIRE(sigmaValues_[i].rows() == sigmaValues_.front().rows(),
                       "all sigma matrixes need to have the same row dimension");
        }
    }

    // PCA loadings

    auto loadingsStr = XMLUtils::getChildrenValues(node, "PCALoadings", "Loadings", false);
    pcaLoadings_.resize(loadingsStr.size());
    for (Size i = 0; i < loadingsStr.size(); ++i) {
        pcaLoadings_[i] = parseListOfValues<double>(loadingsStr[i], parseReal);
    }

    // PCA Sigma0

    if (XMLNode* sigma0Node = XMLUtils::getChildNode(node, "PCASigma0")) {
        calibratePcaSigma0_ = XMLUtils::getChildValueAsBool(sigma0Node, "Calibrate", true);
        DLOG("Hull White sigma0 calibrate = " << calibrateSigma_);

        std::string pcaSigma0ParameterTypeString = XMLUtils::getChildValue(sigma0Node, "ParamType", true);
        pcaSigma0Type_ = parseParamType(pcaSigma0ParameterTypeString);
        DLOG("Hull White pca sigma 0 param type = " << pcaSigma0ParameterTypeString);

        pcaSigma0Times_ = XMLUtils::getChildrenValuesAsDoublesCompact(sigma0Node, "TimeGrid", true);
        DLOG("Hull White pca sigma 0time grid size = " << pcaSigma0Times_.size());

        pcaSigma0Values_ = XMLUtils::getChildrenValuesAsDoublesCompact(sigma0Node, "InitialValue", true);
    }

    // PCA Sigma Ratios

    pcaSigmaRatios_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "PCASigmaRatios", false);

    // Calibration Swaptions

    if (XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationSwaptions")) {
        optionExpiries() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", false);
        optionTerms() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Terms", false);
        QL_REQUIRE(optionExpiries().size() == optionTerms().size(),
                   "vector size mismatch in swaption expiries/terms for ccy " << qualifier_);
        optionStrikes() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);
        if (optionStrikes().size() > 0) {
            QL_REQUIRE(optionStrikes().size() == optionExpiries().size(),
                       "vector size mismatch in swaption expiries/strikes for ccy " << qualifier_);
        } else // Default: ATM
            optionStrikes().resize(optionExpiries().size(), "ATM");

        for (Size i = 0; i < optionExpiries().size(); i++) {
            LOG("LGM calibration swaption " << optionExpiries()[i] << " x " << optionTerms()[i] << " "
                                            << optionStrikes()[i]);
        }
    }

    IrModelData::fromXML(node);
    LOG("HwModelData done");
}

XMLNode* HwModelData::toXML(XMLDocument& doc) const {

    XMLNode* hwModelNode = IrModelData::toXML(doc);

    // Reversion

    if (!kappaValues_.empty()) {
        XMLNode* reversionNode = XMLUtils::addChild(doc, hwModelNode, "Reversion");
        XMLUtils::addChild(doc, reversionNode, "Calibrate", calibrateKappa_);
        XMLUtils::addGenericChild(doc, reversionNode, "ParamType", kappaType_);
        XMLUtils::addGenericChildAsList(doc, reversionNode, "TimeGrid", kappaTimes_);

        auto kappaNode = XMLUtils::addChild(doc, reversionNode, "InitialValue");
        for (const auto& kappa : kappaValues_) {
            std::ostringstream oss;
            if (kappa.size() == 0) {
                oss << "";
            } else {
                oss << kappa[0];
                for (Size i = 1; i < kappa.size(); i++) {
                    oss << ", " << kappa[i];
                }
            }
            XMLUtils::addChild(doc, kappaNode, "Kappa", oss.str());
        }
    }

    // Volatility

    if (!sigmaValues_.empty()) {
        XMLNode* volatilityNode = XMLUtils::addChild(doc, hwModelNode, "Volatility");
        XMLUtils::addChild(doc, volatilityNode, "Calibrate", calibrateSigma_);
        XMLUtils::addGenericChild(doc, volatilityNode, "ParamType", sigmaType_);
        XMLUtils::addGenericChildAsList(doc, volatilityNode, "TimeGrid", sigmaTimes_);

        auto sigmaValues = XMLUtils::addChild(doc, volatilityNode, "InitialValue");
        for (const auto& sigma : sigmaValues_) {
            auto sigmaNode = XMLUtils::addChild(doc, sigmaValues, "Sigma");
            for (size_t row = 0; row < sigma.rows(); ++row) {
                std::ostringstream oss;
                if (sigma.columns() == 0) {
                    oss << "";
                } else {
                    oss << sigma[row][0];
                    for (Size col = 0; col < sigma.columns(); col++) {
                        oss << ", " << sigma[row][col];
                    }
                }
                XMLUtils::addChild(doc, sigmaNode, "Row", oss.str());
            }
        }
    }

    // PCA Loadings

    if (!pcaLoadings_.empty()) {
        XMLNode* pcaLoadingsNode = XMLUtils::addChild(doc, hwModelNode, "PCALoadings");
        for (auto const& l : pcaLoadings_) {
            XMLUtils::addChild(doc, pcaLoadingsNode, "Loadings", l);
        }
    }

    // PCA Sigma0

    if (!pcaSigma0Values_.empty()) {
        XMLNode* sigma0Node = XMLUtils::addChild(doc, hwModelNode, "PCASigma0");
        XMLUtils::addChild(doc, sigma0Node, "Calibrate", calibrateSigma_);
        XMLUtils::addGenericChild(doc, sigma0Node, "ParamType", sigmaType_);
        XMLUtils::addGenericChildAsList(doc, sigma0Node, "TimeGrid", sigmaTimes_);
        XMLUtils::addChild(doc, sigma0Node, "InitialValue", pcaSigma0Values_);
    }

    // PCA Sigma Ratios

    if (!pcaSigmaRatios_.empty()) {
        XMLUtils::addChild(doc, hwModelNode, "PCASigmaRatios", pcaSigmaRatios_);
    }

    return hwModelNode;
}

} // namespace data
} // namespace ore
