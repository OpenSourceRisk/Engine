/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/model/modelparameter.hpp
    \brief class for holding model parameter data
    \ingroup models
*/

#pragma once

#include <ql/types.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <boost/optional.hpp>
#include <vector>

namespace ore {
namespace data {

/*! Abstract base class for holding model parameter data.
    \ingroup models
*/
class ModelParameter : public XMLSerializable {
public:
    //! Default constructor
    ModelParameter();

    //! Detailed constructor
    ModelParameter(bool calibrate, ParamType type, std::vector<QuantLib::Time> times,
                   std::vector<QuantLib::Real> values);

    //! \name Inspectors
    //@{
    bool calibrate() const;
    ParamType type() const;
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Real>& values() const;
    //@}

    //! \name Setters / Modifiers

    //@{
    void setTimes(std::vector<Real> times);
    void setValues(std::vector<Real> values);
    void mult(const Real f);
    void setCalibrate(const bool b);
   //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    //@}

protected:
    //! Method used by toXML in derived classes to add the members here to a node.
    void append(XMLDocument& doc, XMLNode* node) const;

    //! Perform some checks on the parameters
    virtual void check() const;

private:
    bool calibrate_;
    ParamType type_;
    std::vector<QuantLib::Time> times_;
    std::vector<QuantLib::Real> values_;
};

/*! Volatility model parameter with optional volatility type.
    
    \note
    The volatility type is currently an LGM volatility type. We may want to broaden this in future.

    \ingroup models
*/
class VolatilityParameter : public ModelParameter {
public:
    //! Default constructor
    VolatilityParameter();

    //! Constructor for piecewise volatility with an explicit volatility type.
    VolatilityParameter(LgmData::VolatilityType volatilityType, bool calibrate, ParamType type,
                        std::vector<QuantLib::Time> times, std::vector<QuantLib::Real> values);

    //! Constructor for constant volatility with an explicit volatility type.
    VolatilityParameter(LgmData::VolatilityType volatilityType,
        bool calibrate,
        QuantLib::Real value);

    //! Constructor for piecewise volatility without an explicit volatility type.
    VolatilityParameter(bool calibrate, ParamType type, std::vector<QuantLib::Real> times,
                        std::vector<QuantLib::Real> values);

    //! Constructor for constant volatility without an explicit volatility type.
    VolatilityParameter(bool calibrate, QuantLib::Real value);

    //! \name Inspectors
    //@{
    const boost::optional<LgmData::VolatilityType>& volatilityType() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    boost::optional<LgmData::VolatilityType> volatilityType_;
};

/*! Reversion model parameter with specified reversion type.

    \note
    The reversion type is currently an LGM reversion type. We may want to broaden this in future.

    \ingroup models
*/
class ReversionParameter : public ModelParameter {
public:
    //! Default constructor
    ReversionParameter();

    //! Constructor for piecewise reversion
    ReversionParameter(LgmData::ReversionType reversionType, bool calibrate, ParamType type,
                       std::vector<QuantLib::Time> times, std::vector<QuantLib::Real> values);

    //! Constructor for constant reversion
    ReversionParameter(LgmData::ReversionType reversionType, bool calibrate, QuantLib::Real value);

    //! \name Inspectors
    //@{
    LgmData::ReversionType reversionType() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    LgmData::ReversionType reversionType_;
};

}
}
