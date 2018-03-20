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

/*! \file model/lgmdata.hpp
    \brief Linear Gauss Markov model data
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <qle/models/lgm.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/utilities/xmlutils.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

//! Supported calibration parameter type
enum class ParamType {
    Constant,
    Piecewise // i.e. time-dependent, but piecewise constant
};

//! Convert parameter type string into enumerated class value
ParamType parseParamType(const string& s);
//! Convert enumerated class value into a string
std::ostream& operator<<(std::ostream& oss, const ParamType& type);

//! Supported calibration types
enum class CalibrationType {
    /*! Choose this option if the component's calibration strategy is expected to
      yield a perfect match of model to market prices. For example, this can be
      achieved when calibrating an IR component to a series of co-terminal
      swaptions with given mean reversion speed and piecewise volatility
      function (alpha) where jump times coincide with expiry dates in the swaption
      basket. Similarly, when calibrating an FX component to a series of FX Options.
      The calibration routine will throw an exception if no perfect match is
      achieved.
     */
    Bootstrap,
    /*! Choose this if no perfect match like above can be expected, for example when
      an IR component with constant parameters is calibrated to a basket of swaptions.
      The calibration routine will consequently not throw an exception when the match
      is imperfect.
     */
    BestFit,
    /*! No calibration
     */
    None
};

//! Convert calibration type string into enumerated class value
CalibrationType parseCalibrationType(const string& s);
//! Convert enumerated class value into a string
std::ostream& operator<<(std::ostream& oss, const CalibrationType& type);

//! Linear Gauss Markov Model Parameters
/*!
  This class contains the description of a Linear Gauss Markov interest rate model
  and instructions for how to calibrate it.

  \ingroup models
 */
class LgmData : public XMLSerializable {
public:
    //! Supported mean reversion types
    enum class ReversionType {
        /*! Parametrize H(t) via Hull-White mean reversion speed,
          LGM H(t) = int_0^t exp(-kappa(s) *s) ds  with constant or piecewise kappa(s)
        */
        HullWhite,
        //! Parametrize LGM H(t) as H(t) = int_0^t h(s) ds with constant or piecewise h(s)
        Hagan
        // FIXME: indent
    };

    //! Supported volatility types
    enum class VolatilityType {
        //! Parametrize volatility as HullWhite sigma(t)
        HullWhite,
        //! Parametrize volatility as Hagan alpha(t)
        Hagan
    };

    //! Supported calibration strategies
    enum class CalibrationStrategy { CoterminalATM, CoterminalDealStrike, None };

    //! Default constructor
    LgmData() {}

    //! Detailed constructor
    LgmData(std::string qualifier, CalibrationType calibrationType, ReversionType revType, VolatilityType volType,
            bool calibrateH, ParamType hType, std::vector<Time> hTimes, std::vector<Real> hValues, bool calibrateA,
            ParamType aType, std::vector<Time> aTimes, std::vector<Real> aValues, Real shiftHorizon = 0.0,
            Real scaling = 1.0, std::vector<std::string> optionExpiries = std::vector<std::string>(),
            std::vector<std::string> optionTerms = std::vector<std::string>(),
            std::vector<std::string> optionStrikes = std::vector<std::string>())
        : qualifier_(qualifier), calibrationType_(calibrationType), revType_(revType), volType_(volType),
          calibrateH_(calibrateH), hType_(hType), hTimes_(hTimes), hValues_(hValues), calibrateA_(calibrateA),
          aType_(aType), aTimes_(aTimes), aValues_(aValues), shiftHorizon_(shiftHorizon), scaling_(scaling),
          optionExpiries_(optionExpiries), optionTerms_(optionTerms), optionStrikes_(optionStrikes) {}

    //! Clear list of calibration instruments
    void clear();

    //! Reset member variables to defaults
    void reset();

    //! \name Serialisation
    //@{
    void fromFile(const std::string& fileName, const std::string& ccy = "");
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Setters/Getters
    //@{
    std::string& qualifier() { return qualifier_; }
    CalibrationType& calibrationType() { return calibrationType_; }
    ReversionType& reversionType() { return revType_; }
    VolatilityType& volatilityType() { return volType_; }
    bool& calibrateH() { return calibrateH_; }
    ParamType& hParamType() { return hType_; }
    std::vector<Time>& hTimes() { return hTimes_; }
    std::vector<Real>& hValues() { return hValues_; }
    bool& calibrateA() { return calibrateA_; }
    ParamType& aParamType() { return aType_; }
    std::vector<Time>& aTimes() { return aTimes_; }
    std::vector<Real>& aValues() { return aValues_; }
    Real& shiftHorizon() { return shiftHorizon_; }
    Real& scaling() { return scaling_; }
    std::vector<std::string>& optionExpiries() { return optionExpiries_; }
    std::vector<std::string>& optionTerms() { return optionTerms_; }
    std::vector<std::string>& optionStrikes() { return optionStrikes_; }
    CalibrationStrategy& calibrationStrategy() { return calibrationStrategy_; }
    //@}

    //! \name Operators
    //@{
    bool operator==(const LgmData& rhs);
    bool operator!=(const LgmData& rhs);
    //@}

private:
    std::string qualifier_;
    CalibrationType calibrationType_;
    ReversionType revType_;
    VolatilityType volType_;
    bool calibrateH_;
    ParamType hType_;
    std::vector<Time> hTimes_;
    std::vector<Real> hValues_;
    bool calibrateA_;
    ParamType aType_;
    std::vector<Time> aTimes_;
    std::vector<Real> aValues_;
    Real shiftHorizon_, scaling_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionTerms_;
    std::vector<std::string> optionStrikes_;
    CalibrationStrategy calibrationStrategy_;
};

//! Enum parsers used in CrossAssetModelBuilder's fromXML
LgmData::ReversionType parseReversionType(const string& s);
LgmData::VolatilityType parseVolatilityType(const string& s);
LgmData::CalibrationStrategy parseCalibrationStrategy(const string& s);

//! Enum to string used in CrossAssetModelBuilder's toXML
std::ostream& operator<<(std::ostream& oss, const LgmData::ReversionType& type);
std::ostream& operator<<(std::ostream& oss, const LgmData::VolatilityType& type);
std::ostream& operator<<(std::ostream& oss, const LgmData::CalibrationStrategy& type);
} // namespace data
} // namespace ore
