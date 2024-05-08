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
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/irmodeldata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

class VolatilityParameter;
class ReversionParameter;

//! Linear Gauss Markov Model Parameters
/*!
  This class contains the description of a Linear Gauss Markov interest rate model
  and instructions for how to calibrate it.

  \ingroup models
 */
class LgmData : public IrModelData {
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

    //! Default constructor
    LgmData()
        : IrModelData("LGM", "", CalibrationType::None), revType_(ReversionType::Hagan),
          volType_(VolatilityType::Hagan), calibrateH_(false), hType_(ParamType::Constant), calibrateA_(false),
          aType_(ParamType::Constant), shiftHorizon_(0.0), scaling_(1.0) {}

    //! Detailed constructor
    LgmData(std::string qualifier, CalibrationType calibrationType, ReversionType revType, VolatilityType volType,
            bool calibrateH, ParamType hType, std::vector<Time> hTimes, std::vector<Real> hValues, bool calibrateA,
            ParamType aType, std::vector<Time> aTimes, std::vector<Real> aValues, Real shiftHorizon = 0.0,
            Real scaling = 1.0, std::vector<std::string> optionExpiries = std::vector<std::string>(),
            std::vector<std::string> optionTerms = std::vector<std::string>(),
            std::vector<std::string> optionStrikes = std::vector<std::string>(),
            const QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping inputFloatSpreadMapping =
                QuantExt::AnalyticLgmSwaptionEngine::proRata)
        : IrModelData("LGM", qualifier, calibrationType), revType_(revType), volType_(volType), calibrateH_(calibrateH),
          hType_(hType), hTimes_(hTimes), hValues_(hValues), calibrateA_(calibrateA), aType_(aType), aTimes_(aTimes),
          aValues_(aValues), shiftHorizon_(shiftHorizon), scaling_(scaling), optionExpiries_(optionExpiries),
          optionTerms_(optionTerms), optionStrikes_(optionStrikes), floatSpreadMapping_(inputFloatSpreadMapping) {}

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
    QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping& floatSpreadMapping() { return floatSpreadMapping_; }
    std::vector<std::string>& optionExpiries() const { return optionExpiries_; }
    std::vector<std::string>& optionTerms() const { return optionTerms_; }
    std::vector<std::string>& optionStrikes() const { return optionStrikes_; }
    ReversionParameter reversionParameter() const;
    VolatilityParameter volatilityParameter() const;
    //@}

    //! \name Operators
    //@{
    bool operator==(const LgmData& rhs);
    bool operator!=(const LgmData& rhs);
    //@}

private:
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
    mutable std::vector<std::string> optionExpiries_;
    mutable std::vector<std::string> optionTerms_;
    mutable std::vector<std::string> optionStrikes_;
    QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping floatSpreadMapping_ =
        QuantExt::AnalyticLgmSwaptionEngine::proRata;
};

//! Enum parsers
LgmData::ReversionType parseReversionType(const string& s);
LgmData::VolatilityType parseVolatilityType(const string& s);
QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping parseFloatSpreadMapping(const string& s);

//! Enum to string
std::ostream& operator<<(std::ostream& oss, const LgmData::ReversionType& type);
std::ostream& operator<<(std::ostream& oss, const LgmData::VolatilityType& type);
std::ostream& operator<<(std::ostream& oss, const QuantExt::AnalyticLgmSwaptionEngine::FloatSpreadMapping& m);

/*! LGM reversion transformation.
    
    This class holds values for possibly transforming the reversion parameter of the LGM model. The use of this is 
    outlined in <em>Modern Derivatives Pricing and Credit Exposure Analysis</em>, Section 16.4.

    \ingroup models
 */
class LgmReversionTransformation : public XMLSerializable {
public:
    //! Default constructor setting the horizon to 0.0 and the scaling to 1.0.
    LgmReversionTransformation();

    //! Detailed constructor
    LgmReversionTransformation(QuantLib::Time horizon, QuantLib::Real scaling);

    //! \name Inspectors
    //@{
    QuantLib::Time horizon() const;
    QuantLib::Real scaling() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    QuantLib::Time horizon_;
    QuantLib::Real scaling_;
};

} // namespace data
} // namespace ore
