/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file ored/model/crcirdata.hpp
    \brief CIR credit model data
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

class CrCirData : public XMLSerializable {
public:
    enum class CalibrationStrategy { CurveAndFlatVol, None };

    //! Default constructor
    CrCirData() {}

    //! Detailed constructor
    CrCirData(std::string name, std::string currency, CalibrationType calibrationType,
              CalibrationStrategy calibrationStrategy, Real startValue, Real reversionValue, Real longTermValue,
              Real volatility, bool relaxedFeller, Real fellerFactor, Real tolerance,
              std::vector<std::string> optionExpiries = std::vector<std::string>(),
              std::vector<std::string> optionTerms = std::vector<std::string>(),
              std::vector<std::string> optionStrikes = std::vector<std::string>())
        : name_(name), currency_(currency), calibrationType_(calibrationType), calibrationStrategy_(calibrationStrategy),
          startValue_(startValue), reversionValue_(reversionValue), longTermValue_(longTermValue),
          volatility_(volatility), relaxedFeller_(relaxedFeller), fellerFactor_(fellerFactor), tolerance_(tolerance),
          optionExpiries_(optionExpiries), optionTerms_(optionTerms), optionStrikes_(optionStrikes) {}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}

    //! setter/getter for qualifier in derived classes

    //! \name Setters/Getters
    //@{
    std::string& name() { return name_; }
    std::string& currency() { return currency_; }
    CalibrationType& calibrationType() { return calibrationType_; }
    CalibrationStrategy& calibrationStrategy() { return calibrationStrategy_; }
    Real& startValue() { return startValue_; }
    Real& reversionValue() { return reversionValue_; }
    Real& longTermValue() { return longTermValue_; }
    Real& volatility() { return volatility_; }
    std::vector<std::string>& optionExpiries() { return optionExpiries_; }
    std::vector<std::string>& optionTerms() { return optionTerms_; }
    std::vector<std::string>& optionStrikes() { return optionStrikes_; }
    bool& relaxedFeller() { return relaxedFeller_; }
    Real& fellerFactor() { return fellerFactor_; }
    Real& tolerance() { return tolerance_; }
    //@}

    //! \name Operators
    //@{
    bool operator==(const CrCirData& rhs);
    bool operator!=(const CrCirData& rhs);
    //@}

private:
    std::string name_;
    std::string currency_;
    CalibrationType calibrationType_;
    CalibrationStrategy calibrationStrategy_;
    Real startValue_, reversionValue_, longTermValue_, volatility_;
    bool relaxedFeller_;
    Real fellerFactor_;
    Real tolerance_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionTerms_;
    std::vector<std::string> optionStrikes_;
};

CrCirData::CalibrationStrategy parseCirCalibrationStrategy(const string& s);
std::ostream& operator<<(std::ostream& oss, const CrCirData::CalibrationStrategy& s);

} // namespace data
} // namespace ore
