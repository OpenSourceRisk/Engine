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

/*! \file model/commodityschwartzmodeldata.hpp
    \brief COM component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! COM Schwartz Model Parameters
/*!
  Specification for a COM model component with lognormal forwards in the Cross Asset LGM. 
  This class covers the CommoditySchwartz parametrization.
  \ingroup models
*/
class CommoditySchwartzData {
public:
    //! Default constructor
    CommoditySchwartzData(bool driftFreeState = false,
                          QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod = QuantLib::ext::make_shared<LevenbergMarquardt>(1E-8, 1E-8, 1E-8),
                          EndCriteria endCriteria = EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8),
                          Constraint constraint = Constraint(),
                          BlackCalibrationHelper::CalibrationErrorType calibrationErrorType = BlackCalibrationHelper::RelativePriceError)
        : driftFreeState_(driftFreeState), optimizationMethod_(optimizationMethod), endCriteria_(endCriteria),
          constraint_(constraint), calibrationErrorType_(calibrationErrorType) {}

    //! Detailed constructor
    CommoditySchwartzData(std::string name, std::string currency, CalibrationType calibrationType,
                          bool calibrateSigma, Real sigma,
                          bool calibrateKappa, Real kappa,
                          std::vector<std::string> optionExpiries = std::vector<std::string>(),
                          std::vector<std::string> optionStrikes = std::vector<std::string>(),
                          QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod = QuantLib::ext::make_shared<LevenbergMarquardt>(1E-8, 1E-8, 1E-8),
                          EndCriteria endCriteria = EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8),
                          Constraint constraint = Constraint(),
                          BlackCalibrationHelper::CalibrationErrorType calibrationErrorType = BlackCalibrationHelper::RelativePriceError,
                          bool driftFreeState = false)
        : name_(name), ccy_(currency), calibrationType_(calibrationType),
          calibrateSigma_(calibrateSigma), sigmaType_(ParamType::Constant), sigmaValue_(sigma),
          calibrateKappa_(calibrateKappa),kappaType_(ParamType::Constant), kappaValue_(kappa),
          optionExpiries_(optionExpiries), optionStrikes_(optionStrikes),
          driftFreeState_(driftFreeState), optimizationMethod_(optimizationMethod),
          endCriteria_(endCriteria), constraint_(constraint), calibrationErrorType_(calibrationErrorType) {}

    //! \name Setters/Getters
    //@{
    std::string& name() { return name_; }
    std::string& currency() { return ccy_; }
    CalibrationType& calibrationType() { return calibrationType_; }
    bool& calibrateSigma() { return calibrateSigma_; }
    ParamType& sigmaParamType() { return sigmaType_; }
    Real& sigmaValue() { return sigmaValue_; }
    bool& calibrateKappa() { return calibrateKappa_; }
    ParamType& kappaParamType() { return sigmaType_; }
    Real& kappaValue() { return kappaValue_; }
    std::vector<std::string>& optionExpiries() { return optionExpiries_; }
    std::vector<std::string>& optionStrikes() { return optionStrikes_; }
    bool& driftFreeState() { return driftFreeState_; }
    QuantLib::ext::shared_ptr<OptimizationMethod>& optimizationMethod() { return optimizationMethod_; }
    EndCriteria& endCriteria() { return endCriteria_; }
    Constraint& constraint() { return constraint_; }
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType() { return calibrationErrorType_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Operators
    //@{
    bool operator==(const CommoditySchwartzData& rhs);
    bool operator!=(const CommoditySchwartzData& rhs);
    //@}

private:
    std::string name_;
    std::string ccy_;
    CalibrationType calibrationType_ = CalibrationType::None;
    bool calibrateSigma_ = false;
    ParamType sigmaType_ = ParamType::Constant;
    Real sigmaValue_ = 0.0;
    bool calibrateKappa_ = false;
    ParamType kappaType_ = ParamType::Constant;
    Real kappaValue_ = 0.0;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionStrikes_;
    bool driftFreeState_ = false;
    QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;
    Constraint constraint_;
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType_ = BlackCalibrationHelper::RelativePriceError;
};
} // namespace data
} // namespace ore
