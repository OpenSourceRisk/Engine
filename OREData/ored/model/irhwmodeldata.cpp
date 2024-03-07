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
    kappaValues_ = {Array(1, 0.01)};
    calibrateSigma_ = false;
    sigmaType_ = ParamType::Constant;
    sigmaTimes_ = {};
    sigmaValues_ = {Matrix(1, 1, 0.03)};
}

void HwModelData::fromXML(XMLNode* node) {

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

    XMLNode* reversionNode = XMLUtils::getChildNode(node, "Reversion");

    calibrateKappa_ = XMLUtils::getChildValueAsBool(reversionNode, "Calibrate", true);
    LOG("Hull White mean reversion calibrate = " << calibrateKappa_);
    QL_REQUIRE(!calibrateKappa_, "Calibration of kappa not supported yet");

    std::string kappaCalibrationString = XMLUtils::getChildValue(reversionNode, "ParamType", true);
    kappaType_ = parseParamType(kappaCalibrationString);
    LOG("Hull White Kappa param type = " << kappaCalibrationString);
    QL_REQUIRE(kappaType_ == ParamType::Constant, "Only constant mean reversion supported at the moment");

    kappaTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(reversionNode, "TimeGrid", true);
    LOG("Hull White Reversion time grid size = " << kappaTimes_.size());

    XMLNode* initialValuesNode = XMLUtils::getChildNode(reversionNode, "InitialValue");
    for (XMLNode* child = XMLUtils::getChildNode(initialValuesNode, "Kappa"); child;
         child = XMLUtils::getNextSibling(child, "Kappa")) {
        auto kappa_t = XMLUtils::getNodeValueAsDoublesCompact(child);
        kappaValues_.push_back(Array(kappa_t.begin(), kappa_t.end()));
    }

    QL_REQUIRE(!kappaValues_.empty(), "No initial mean reversion values given");
    size_t nFactors = kappaValues_.front().size();
    for (size_t i = 1; i < kappaValues_.size(); ++i) {
        QL_REQUIRE(nFactors == kappaValues_[i].size(), "expect " << nFactors << " factors but got "
                                                                 << kappaValues_[i].size() << " at time "
                                                                 << kappaTimes_[i]);
    }

    // Volatility config

    XMLNode* volatilityNode = XMLUtils::getChildNode(node, "Volatility");

    calibrateSigma_ = XMLUtils::getChildValueAsBool(volatilityNode, "Calibrate", true);
    LOG("Hull White volatility calibrate = " << calibrateSigma_);
    QL_REQUIRE(!calibrateSigma_, "Calibration of kappa not supported yet");

    std::string sigmaParameterTypeString = XMLUtils::getChildValue(volatilityNode, "ParamType", true);
    sigmaType_ = parseParamType(sigmaParameterTypeString);
    LOG("Hull White Volatility param type = " << sigmaParameterTypeString);
    QL_REQUIRE(sigmaType_ == ParamType::Constant, "Only constant constant supported at the moment");

    sigmaTimes_ = XMLUtils::getChildrenValuesAsDoublesCompact(volatilityNode, "TimeGrid", true);
    LOG("Hull White volatility time grid size = " << sigmaTimes_.size());

    XMLNode* initialSigmasNode = XMLUtils::getChildNode(volatilityNode, "InitialValue");
    for (XMLNode* sigmaNode = XMLUtils::getChildNode(initialSigmasNode, "Sigma"); sigmaNode;
         sigmaNode = XMLUtils::getNextSibling(sigmaNode, "Sigma")) {
        std::vector<std::vector<double>> matrix;
        for (XMLNode* rowNode = XMLUtils::getChildNode(sigmaNode, "Row"); rowNode;
             rowNode = XMLUtils::getNextSibling(rowNode, "Row")) {
            matrix.push_back(XMLUtils::getNodeValueAsDoublesCompact(rowNode));
            QL_REQUIRE(matrix.back().size() == nFactors, "Mismatch between kappa and sigma");
        }
        QL_REQUIRE(!matrix.empty(), "Sigma not provided");
        QL_REQUIRE(!matrix.front().empty(), "Sigma not provided");
        Matrix sigma(matrix.size(), matrix.front().size(), 0.0);
        for (size_t i = 0; i < matrix.size(); ++i) {
            for (size_t j = 0; j < nFactors; ++j)
                sigma[i][j] = matrix[i][j];
        }
        sigmaValues_.push_back(sigma);
    }

    QL_REQUIRE(!sigmaValues_.empty(), "need at least one sigma matrix");
    size_t m_brownians = sigmaValues_.front().rows();
    for (size_t i = 0; i < sigmaValues_.size(); ++i) {
        QL_REQUIRE(sigmaValues_[i].rows() == m_brownians, "all sigma matrixes need to have the same row dimension");
    }

    IrModelData::fromXML(node);
    LOG("HwModelData done");
}

XMLNode* HwModelData::toXML(XMLDocument& doc) const {

    XMLNode* hwModelNode = IrModelData::toXML(doc);
    // reversion
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
    // volatility
    XMLNode* volatilityNode = XMLUtils::addChild(doc, hwModelNode, "Volatility");
    XMLUtils::addChild(doc, volatilityNode, "Calibrate", calibrateSigma_);

    XMLUtils::addGenericChild(doc, volatilityNode, "ParamType", sigmaType_);
    XMLUtils::addGenericChildAsList(doc, volatilityNode, "TimeGrid", sigmaTimes_);

    auto sigmaValues = XMLUtils::addChild(doc, reversionNode, "InitialValue");
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
    return hwModelNode;
}

} // namespace data
} // namespace ore
