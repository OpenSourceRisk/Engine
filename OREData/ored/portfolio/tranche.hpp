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

/*! \file portfolio/tranche.hpp
    \brief cbo tranche data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using ore::data::XMLSerializable;

//! Serializable Bond-Basket Data
/*!
  \ingroup tradedata
*/

class TrancheData : public XMLSerializable {
public:
    //! Default constructor
    TrancheData() {}

    TrancheData(const std::string& name, double icRatio, double  ocRatio, const QuantLib::ext::shared_ptr<LegAdditionalData>& concreteLegData)
        : name_(name), icRatio_(icRatio), ocRatio_(ocRatio), concreteLegData_(concreteLegData) {}

    //! \name Inspectors
    //@{
    const std::string name() const { return name_; }
    double faceAmount() { return faceAmount_; }
    double icRatio() { return icRatio_; }
    double ocRatio() { return ocRatio_; }
    const QuantLib::ext::shared_ptr<LegAdditionalData> concreteLegData() {return concreteLegData_; } 
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    //@}
private:
    std::string name_;
    double faceAmount_;
    double icRatio_;
    double ocRatio_;
    QuantLib::ext::shared_ptr<LegAdditionalData> concreteLegData_;

};
} // namespace data
} // namespace ore
