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

/*! \file model/hwdata.hpp
    \brief Hull White model data
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <ored/model/lgmdata.hpp>
#include <qle/models/hwmodel.hpp>

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
class HwModelData : public XMLSerializable {
public:
    //! Default constructor
    HwModelData()
        : calibrationType_(CalibrationType::None), calibrateKappa_(false), kappaType_(ParamType::Constant),
          calibrateSigma_(false), sigmaType_(ParamType::Constant),
          shiftHorizon_(0.0), scaling_(1.0) {}

    //! Detailed constructor
    HwModelData(std::string qualifier, CalibrationType calibrationType, bool calibrateKappa, ParamType kappaType,
                std::vector<Time> kappaTimes, std::vector<QuantLib::Array> kappaValues, bool calibrateSigma,
                ParamType sigmaType, std::vector<Time> sigmaTimes,
                std::vector<QuantLib::Matrix> sigmaValues, Real shiftHorizon = 0.0, Real scaling = 1.0)
        : qualifier_(qualifier), calibrationType_(calibrationType), 
          calibrateKappa_(calibrateKappa),
          kappaType_(kappaType), kappaTimes_(kappaTimes), kappaValues_(kappaValues), calibrateSigma_(calibrateSigma),
          sigmaType_(sigmaType_), sigmaTimes_(sigmaTimes), sigmaValues_(sigmaValues), shiftHorizon_(shiftHorizon),
          scaling_(scaling) {}

    //! Clear list of calibration instruments
    virtual void clear();

    //! Reset member variables to defaults
    virtual void reset();

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Setters/Getters
    //@{
    std::string& qualifier() { return qualifier_; }
    CalibrationType& calibrationType() { return calibrationType_; }
    bool& calibrateKappa() { return calibrateKappa_; }
    ParamType& kappaType() { return kappaType_; }
    std::vector<Time>& kappaTimes() { return kappaTimes_; }
    std::vector<Array>& kappaValues() { return kappaValues_; }
    bool& calibrateSigma() { return calibrateSigma_; }
    ParamType& sigmaType() { return sigmaType_; }
    std::vector<Time>& sigmaTimes() { return sigmaTimes_; }
    std::vector<QuantLib::Matrix>& sigmaValues() { return sigmaValues_; }
    Real& shiftHorizon() { return shiftHorizon_; }
    Real& scaling() { return scaling_; }
    //@}

    //! \name Operators
    //@{
    bool operator==(const HwModelData& rhs);
    bool operator!=(const HwModelData& rhs);
    //@}

protected:
    std::string qualifier_;

private:
    CalibrationType calibrationType_;
    bool calibrateKappa_;
    ParamType kappaType_;
    std::vector<Time> kappaTimes_;
    std::vector<QuantLib::Array> kappaValues_;

    bool calibrateSigma_;
    ParamType sigmaType_;
    std::vector<Time> sigmaTimes_;
    std::vector<QuantLib::Matrix> sigmaValues_;


    Real shiftHorizon_, scaling_;
};


} // namespace data
} // namespace ore
