/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file ored/portfolio/optionasiandata.hpp
    \brief asian option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/instruments/averagetype.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace data {

/*! Serializable object holding Asian option data for options with payoff type Asian.
    \ingroup tradedata
*/
class OptionAsianData : public XMLSerializable {
public:
    enum class AsianType { Price, Strike };

    //! Default constructor
    OptionAsianData();

    //! Constructor taking an Asian type, average type
    OptionAsianData(const AsianType& asianType, const QuantLib::Average::Type& averageType);

    //! \name Inspectors
    //@{
    const AsianType& asianType() const { return asianType_; }
    const QuantLib::Average::Type& averageType() const { return averageType_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:

    AsianType asianType_;
    QuantLib::Average::Type averageType_;

    //! Populate the value of asianType_ from string
    void populateAsianType(const std::string& s);
};

//! Print AsianType enum values.
std::ostream& operator<<(std::ostream& out, const OptionAsianData::AsianType& asianType);

} // namespace data
} // namespace ore
