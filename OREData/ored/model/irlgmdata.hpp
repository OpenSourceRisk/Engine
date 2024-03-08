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

/*! \file model/irlgmdata.hpp
    \brief IR component data for the cross asset model
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

//! INF Model Parameters
/*!
    Specification for a IR model component in the Cross Asset LGM (i.e. lognormal Inflation with
    stochastic IR/FX differential).
    The specification applies to the volatility component (sigma) of the IR model only.

    \ingroup models
*/

class IrLgmData : public LgmData {
public:
    //! Default constructor
    IrLgmData() {}

    //! Detailed constructor
    IrLgmData(std::string qualifier, CalibrationType calibrationType, ReversionType revType, VolatilityType volType,
              bool calibrateH, ParamType hType, std::vector<Time> hTimes, std::vector<Real> hValues, bool calibrateA,
              ParamType aType, std::vector<Time> aTimes, std::vector<Real> aValues, Real shiftHorizon = 0.0,
              Real scaling = 1.0, std::vector<std::string> optionExpiries = std::vector<std::string>(),
              std::vector<std::string> optionTerms = std::vector<std::string>(),
              std::vector<std::string> optionStrikes = std::vector<std::string>())
        : LgmData(qualifier, calibrationType, revType, volType, calibrateH, hType, hTimes, hValues, calibrateA, aType,
                  aTimes, aValues, shiftHorizon, scaling, optionExpiries, optionTerms, optionStrikes) {}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}
    void clear() override { LgmData::clear(); }
    void reset() override {
        LgmData::reset();
    }
};
} // namespace data
} // namespace ore
