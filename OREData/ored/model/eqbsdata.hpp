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

/*! \file model/eqbsdata.hpp
    \brief EQ component data for the cross asset model
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

//! EQ Model Parameters
/*!
  Specification for a EQ model component in the Cross Asset LGM (i.e. lognormal Equity with
  stochastic IR/FX differential).
  The specification applies to the volatility component (sigma) of the EQ model only.

  \ingroup models
*/
class EqBsData {
public:
    //! Default constructor
    EqBsData() {}

    //! Detailed constructor
    EqBsData(std::string name, std::string currency, CalibrationType calibrationType, bool calibrateSigma,
             ParamType sigmaType, const std::vector<Time>& sigmaTimes, const std::vector<Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>())
        : name_(name), ccy_(currency), calibrationType_(calibrationType), calibrateSigma_(calibrateSigma),
          sigmaType_(sigmaType), sigmaTimes_(sigmaTimes), sigmaValues_(sigmaValues), optionExpiries_(optionExpiries),
          optionStrikes_(optionStrikes) {}

    //! \name Setters/Getters
    //@{
    const std::string& eqName() const { return name_; }
    std::string& eqName() { return name_; }
    std::string& currency() { return ccy_; }
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
    bool operator==(const EqBsData& rhs);
    bool operator!=(const EqBsData& rhs);
    //@}

private:
    std::string name_;
    std::string ccy_;
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
