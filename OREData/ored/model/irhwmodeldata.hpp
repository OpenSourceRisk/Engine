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

/*! \file model/irhwmodeldata.hpp
    \brief Hull White model data
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <ored/model/irmodeldata.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Hull White Model Parameters
/*!
  This class contains the description of a Hull White interest rate model
  and instructions for how to calibrate it.

  \ingroup models
 */
class HwModelData : public IrModelData {
public:
    //! Default constructor
    HwModelData()
        : IrModelData("HwModel", "", CalibrationType::None), calibrateKappa_(false), kappaType_(ParamType::Constant),
          calibrateSigma_(false), sigmaType_(ParamType::Constant) {}

    //! Detailed constructor
    HwModelData(std::string qualifier, CalibrationType calibrationType, bool calibrateKappa, ParamType kappaType,
                std::vector<Time> kappaTimes, std::vector<QuantLib::Array> kappaValues, bool calibrateSigma,
                ParamType sigmaType, std::vector<Time> sigmaTimes, std::vector<QuantLib::Matrix> sigmaValues,
                std::vector<std::string> optionExpiries = std::vector<std::string>(),
                std::vector<std::string> optionTerms = std::vector<std::string>(),
                std::vector<std::string> optionStrikes = std::vector<std::string>())
        : IrModelData("HwModel", qualifier, calibrationType), calibrateKappa_(calibrateKappa), kappaType_(kappaType),
          kappaTimes_(kappaTimes), kappaValues_(kappaValues), calibrateSigma_(calibrateSigma), sigmaType_(sigmaType),
          sigmaTimes_(sigmaTimes), sigmaValues_(sigmaValues), optionExpiries_(optionExpiries),
          optionTerms_(optionTerms), optionStrikes_(optionStrikes) {}

    //! Clear list of calibration instruments
    void clear() override;

    //! Reset member variables to defaults
    void reset() override;

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Setters/Getters
    //@{
    bool& calibrateKappa() { return calibrateKappa_; }
    ParamType& kappaType() { return kappaType_; }
    std::vector<Time>& kappaTimes() { return kappaTimes_; }
    std::vector<Array>& kappaValues() { return kappaValues_; }
    bool& calibrateSigma() { return calibrateSigma_; }
    ParamType& sigmaType() { return sigmaType_; }
    std::vector<Time>& sigmaTimes() { return sigmaTimes_; }
    std::vector<QuantLib::Matrix>& sigmaValues() { return sigmaValues_; }
    std::vector<std::string>& optionExpiries() { return optionExpiries_; }
    std::vector<std::string>& optionTerms() { return optionTerms_; }
    std::vector<std::string>& optionStrikes() { return optionStrikes_; }
    //@}

    //! \name Operators
    //@{
    bool operator==(const HwModelData& rhs);
    bool operator!=(const HwModelData& rhs);
    //@}

private:
    bool calibrateKappa_;
    ParamType kappaType_;
    std::vector<Time> kappaTimes_;
    std::vector<QuantLib::Array> kappaValues_;

    bool calibrateSigma_;
    ParamType sigmaType_;
    std::vector<Time> sigmaTimes_;
    std::vector<QuantLib::Matrix> sigmaValues_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionTerms_;
    std::vector<std::string> optionStrikes_;
};


} // namespace data
} // namespace ore
