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

/*! \file model/fxbsdata.hpp
    \brief FX component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! FX Model Parameters
/*!
  Specification for a FX model component in the Cross Ccy LGM (i.e. lognormal FX with
  stochastic IR differential).
  The specification applies to the volatility component (sigma) of the FX model only.

  \ingroup models
*/
class FxBsData {
public:
    //! Default constructor
    FxBsData() {}

    //! Detailed constructor
    FxBsData(std::string foreignCcy, std::string domesticCcy, CalibrationType calibrationType, bool calibrateSigma,
             ParamType sigmaType, const std::vector<Time>& sigmaTimes, const std::vector<Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>())
        : foreignCcy_(foreignCcy), domesticCcy_(domesticCcy), calibrationType_(calibrationType),
          calibrateSigma_(calibrateSigma), sigmaType_(sigmaType), sigmaTimes_(sigmaTimes), sigmaValues_(sigmaValues),
          optionExpiries_(optionExpiries), optionStrikes_(optionStrikes) {}

    //! \name Setters/Getters
    //@{
    const std::string& foreignCcy() const { return foreignCcy_; }
    std::string& foreignCcy() { return foreignCcy_; }
    const std::string& domesticCcy() const { return domesticCcy_; }
    std::string& domesticCcy() { return domesticCcy_; }
    CalibrationType& calibrationType() { return calibrationType_; }
    bool& calibrateSigma() { return calibrateSigma_; }
    ParamType& sigmaParamType() { return sigmaType_; }
    std::vector<Time>& sigmaTimes() { return sigmaTimes_; }
    std::vector<Real>& sigmaValues() { return sigmaValues_; }
    std::vector<std::string>& optionExpiries() { return optionExpiries_; }
    std::vector<std::string>& optionStrikes() { return optionStrikes_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Operators
    //@{
    bool operator==(const FxBsData& rhs);
    bool operator!=(const FxBsData& rhs);
    //@}

private:
    std::string foreignCcy_;
    std::string domesticCcy_;
    CalibrationType calibrationType_;
    bool calibrateSigma_;
    ParamType sigmaType_;
    std::vector<Time> sigmaTimes_;
    std::vector<Real> sigmaValues_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionStrikes_;
};
} // namespace data
} // namespace ore
