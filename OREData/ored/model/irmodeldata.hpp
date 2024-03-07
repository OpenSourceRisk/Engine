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

/*! \file model/irmodeldata.hpp
    \brief Generic interest rate model data
    \ingroup models
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace data {
using namespace QuantLib;

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

//! Supported calibration strategies
enum class CalibrationStrategy { CoterminalATM, CoterminalDealStrike, UnderlyingATM, UnderlyingDealStrike, None };

//! Convert calibration type string into enumerated class value
CalibrationType parseCalibrationType(const string& s);
//! Convert enumerated class value into a string
std::ostream& operator<<(std::ostream& oss, const CalibrationType& type);

//! Convert calibration strategy string into enumerated class value
CalibrationStrategy parseCalibrationStrategy(const string& s);
//! Convert enumerated class value into a string
std::ostream& operator<<(std::ostream& oss, const CalibrationStrategy& type);

//! Linear Gauss Markov Model Parameters
/*!
  This class contains the description of a Linear Gauss Markov interest rate model
  and instructions for how to calibrate it.

  \ingroup models
 */
class IrModelData : public XMLSerializable {
public:
    //! minimal constructor
    IrModelData(const std::string& name) : name_(name), calibrationType_(CalibrationType::None) {}

    //! Detailed constructor
    IrModelData(const std::string& name,const std::string& qualifier, CalibrationType calibrationType)
        : name_(name), qualifier_(qualifier), calibrationType_(calibrationType) {}

    //! Clear list of calibration instruments
    virtual void clear();

    //! Reset member variables to defaults
    virtual void reset();

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Setters/Getters
    //@{
    const std::string& name() { return name_; }
    std::string& qualifier() { return qualifier_; }
    CalibrationType& calibrationType() { return calibrationType_; }

    // ccy associated to qualifier, which might be an ibor / ois index name or a currency
    virtual std::string ccy() const;       
    //@}

protected:
    std::string name_;
    std::string qualifier_;
    CalibrationType calibrationType_;
};

} // namespace data
} // namespace ore
