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

#include <ored/model/hwhistoricalcalibrationmodeldata.hpp>

namespace ore {
namespace data {

XMLNode* HwHistoricalCalibrationModelData::toXML(XMLDocument& doc) const {
    XMLNode* crossAssetModel = doc.allocNode("CrossAssetModel");
    if (meanReversionCalibration_) {
        // Output IR parameters
        XMLNode* irModel = XMLUtils::addChild(doc, crossAssetModel, "InterestRateModels");
        for (auto const& ccyMatrix : irKappa_) {
            XMLNode* irHwNf = XMLUtils::addChild(doc, irModel, "HWModel");
            XMLUtils::addAttribute(doc, irHwNf, "ccy", ccyMatrix.first);
            XMLUtils::addChild(doc, irHwNf, "CalibrationType", "None");
            vector<vector<Real>> sigma_vec;
            vector<Real> kappa_vec;
            for (Size i = 0; i < ccyMatrix.second.size(); i++) {
                kappa_vec.push_back(ccyMatrix.second[i]);
            }
            Matrix sigmaFormat = irSigma_.find(ccyMatrix.first)->second;
            for (Size i = 0; i < sigmaFormat.rows(); i++) {
                vector<Real> vec_inner;
                for (Size j = 0; j < sigmaFormat.columns(); j++) {
                    vec_inner.push_back(sigmaFormat[i][j]);
                }
                sigma_vec.push_back(vec_inner);
            }
            XMLNode* volatility = XMLUtils::addChild(doc, irHwNf, "Volatility");
            XMLUtils::addChild(doc, volatility, "Calibrate", "N");
            XMLUtils::addChild(doc, volatility, "VolatilityType", "HullWhite");
            XMLUtils::addChild(doc, volatility, "ParamType", "Constant");
            XMLUtils::addChild(doc, volatility, "TimeGrid");
            XMLNode* sigma = XMLUtils::addChild(doc, volatility, "InitialValue");
            XMLNode* rows = XMLUtils::addChild(doc, sigma, "Sigma");
            for (Size i = 0; i < sigma_vec.size(); ++i) {
                vector<string> formattedRow;
                for (Real val : sigma_vec[i]) {
                    std::ostringstream oss;
                    oss << std::defaultfloat << val;
                    formattedRow.push_back(oss.str());
                }
                XMLUtils::addGenericChildAsList(doc, rows, "Row", formattedRow);
            }
            XMLNode* reversion = XMLUtils::addChild(doc, irHwNf, "Reversion");
            XMLUtils::addChild(doc, reversion, "Calibrate", "N");
            XMLUtils::addChild(doc, reversion, "ReversionType", "HullWhite");
            XMLUtils::addChild(doc, reversion, "ParamType", "Constant");
            XMLUtils::addChild(doc, reversion, "TimeGrid");
            XMLNode* kappa = XMLUtils::addChild(doc, reversion, "InitialValue");
            vector<string> formattedKappa;
            for (Real val : kappa_vec) {
                std::ostringstream oss;
                oss << std::defaultfloat << val;
                formattedKappa.push_back(oss.str());
            }
            XMLUtils::addGenericChildAsList(doc, kappa, "Kappa", formattedKappa);
        }
        if (pcaCalibration_) {
            // Output FX and correlation parameters only when pca calibration is set to true
            XMLNode* fxModel = XMLUtils::addChild(doc, crossAssetModel, "ForeignExchangeModels");
            for (auto const& ccyMatrix : fxSigma_) {
                XMLNode* fxHwNf = XMLUtils::addChild(doc, fxModel, "CrossCcyLGM");
                XMLUtils::addAttribute(doc, fxHwNf, "foreignCcy", ccyMatrix.first.substr(0, 3));
                XMLUtils::addChild(doc, fxHwNf, "DomesticCcy", ccyMatrix.first.substr(3, 3));
                XMLUtils::addChild(doc, fxHwNf, "CalibrationType", "None");
                XMLNode* volatility = XMLUtils::addChild(doc, fxHwNf, "Sigma");
                XMLUtils::addChild(doc, volatility, "Calibrate", "N");
                XMLUtils::addChild(doc, volatility, "ParamType", "Constant");
                XMLUtils::addChild(doc, volatility, "TimeGrid");
                
                std::ostringstream oss;
                oss << std::defaultfloat << ccyMatrix.second;
                XMLUtils::addChild(doc, volatility, "InitialValue", oss.str());
            }
            XMLNode* corrModel = XMLUtils::addChild(doc, crossAssetModel, "InstantaneousCorrelations");
            string correlation;
            vector<string> attrNames{"factor1", "factor2", "index1", "index2"};
            vector<string> attrs;
            for (auto const& ccyMatrix : rho_) {
                for (Size i = 0; i < ccyMatrix.second.rows(); ++i) {
                    for (Size j = 0; j < ccyMatrix.second.columns(); ++j) {
                        std::ostringstream oss;
                        oss << std::defaultfloat << ccyMatrix.second[i][j];
                        vector<string> attrs{ccyMatrix.first.first, ccyMatrix.first.second, std::to_string(i),
                                             std::to_string(j)};
                        XMLUtils::addChild(doc, corrModel, "Correlation", oss.str(), attrNames, attrs);
                    }
                }
            }
        }
    }
    return crossAssetModel;
}

XMLNode* HwHistoricalCalibrationModelData::toXML2(XMLDocument& doc) const {
    XMLNode* crossAssetModel = doc.allocNode("CrossAssetModel");
    if (meanReversionCalibration_) {
        // Output IR parameters
        XMLNode* irModel = XMLUtils::addChild(doc, crossAssetModel, "InterestRateModels");
        for (auto const& ccyMatrix : irKappa_) {
            XMLNode* irHwNf = XMLUtils::addChild(doc, irModel, "HWModel");
            XMLUtils::addAttribute(doc, irHwNf, "ccy", ccyMatrix.first);
            XMLUtils::addChild(doc, irHwNf, "CalibrationType", "StatisticalWithRiskNeutralVolatility");
            vector<Real> kappa_vec;
            for (Size i = 0; i < ccyMatrix.second.size(); i++) {
                kappa_vec.push_back(ccyMatrix.second[i]);
            }
            XMLNode* reversion = XMLUtils::addChild(doc, irHwNf, "Reversion");
            XMLUtils::addChild(doc, reversion, "Calibrate", "N");
            XMLUtils::addChild(doc, reversion, "ReversionType", "HullWhite");
            XMLUtils::addChild(doc, reversion, "ParamType", "Constant");
            XMLUtils::addChild(doc, reversion, "TimeGrid");
            XMLNode* kappa = XMLUtils::addChild(doc, reversion, "InitialValue");
            vector<string> formattedKappa;
            for (Real val : kappa_vec) {
                std::ostringstream oss;
                oss << std::defaultfloat << val;
                formattedKappa.push_back(oss.str());
            }
            XMLUtils::addGenericChildAsList(doc, kappa, "Kappa", formattedKappa);

            XMLNode* pcaV = XMLUtils::addChild(doc, irHwNf, "PCAV");
            Matrix vCcy = v_.find(ccyMatrix.first)->second;
            for (Size i = 0; i < vCcy.rows(); i++) {
                vector<string> formattedRow;
                for (Size j = 0; j < vCcy.columns(); j++) {
                    std::ostringstream oss;
                    oss << std::defaultfloat << vCcy[i][j];
                    formattedRow.push_back(oss.str());
                }
                XMLUtils::addGenericChildAsList(doc, pcaV, "Loadings", formattedRow);
            }

            XMLNode* pcaSigma = XMLUtils::addChild(doc, irHwNf, "PCASigma");
            XMLUtils::addChild(doc, pcaSigma, "Calibrate", "Y");
            XMLUtils::addChild(doc, pcaSigma, "ParamType", "Piecewise");
            XMLUtils::addChild(doc, pcaSigma, "TimeGrid");
            Real firstEigenValue = eigenValues_.find(ccyMatrix.first)->second[0];
            std::ostringstream ossFirst;
            ossFirst << std::defaultfloat << firstEigenValue;
            XMLUtils::addChild(doc, pcaSigma, "InitialValue", ossFirst.str());

            XMLNode* pcaSigmaRatios = XMLUtils::addChild(doc, irHwNf, "PCASigmaRatios");
            vector<string> formattedRatios;
            formattedRatios.push_back("1"); // First ratio is always 1
            for (Size i = 1; i < principalComponents_.find(ccyMatrix.first)->second; i++) {
                std::ostringstream oss;
                oss << std::defaultfloat << (eigenValues_.find(ccyMatrix.first)->second[i] / firstEigenValue);
                formattedRatios.push_back(oss.str());
            }
            XMLUtils::addGenericChildAsList(doc, pcaSigmaRatios, "Ratios", formattedRatios);
        }
        if (pcaCalibration_) {
            // Output FX and correlation parameters only when pca calibration is set to true
            XMLNode* fxModel = XMLUtils::addChild(doc, crossAssetModel, "ForeignExchangeModels");
            for (auto const& ccyMatrix : fxSigma_) {
                XMLNode* fxHwNf = XMLUtils::addChild(doc, fxModel, "CrossCcyLGM");
                XMLUtils::addAttribute(doc, fxHwNf, "foreignCcy", ccyMatrix.first.substr(0, 3));
                XMLUtils::addChild(doc, fxHwNf, "DomesticCcy", ccyMatrix.first.substr(3, 3));
                XMLUtils::addChild(doc, fxHwNf, "CalibrationType", "None");
                XMLNode* volatility = XMLUtils::addChild(doc, fxHwNf, "Sigma");
                XMLUtils::addChild(doc, volatility, "Calibrate", "N");
                XMLUtils::addChild(doc, volatility, "ParamType", "Constant");
                XMLUtils::addChild(doc, volatility, "TimeGrid");

                std::ostringstream oss;
                oss << std::defaultfloat << ccyMatrix.second;
                XMLUtils::addChild(doc, volatility, "InitialValue", oss.str());
            }
            XMLNode* corrModel = XMLUtils::addChild(doc, crossAssetModel, "InstantaneousCorrelations");
            string correlation;
            vector<string> attrNames{"factor1", "factor2", "index1", "index2"};
            vector<string> attrs;
            for (auto const& ccyMatrix : rho_) {
                for (Size i = 0; i < ccyMatrix.second.rows(); ++i) {
                    for (Size j = 0; j < ccyMatrix.second.columns(); ++j) {
                        std::ostringstream oss;
                        oss << std::defaultfloat << ccyMatrix.second[i][j];
                        vector<string> attrs{ccyMatrix.first.first, ccyMatrix.first.second, std::to_string(i),
                                             std::to_string(j)};
                        XMLUtils::addChild(doc, corrModel, "Correlation", oss.str(), attrNames, attrs);
                    }
                }
            }
        }
    }
    return crossAssetModel;
}

} // namespace data
} // namespace ore
