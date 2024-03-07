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

/*! \file ored/portfolio/optionexercisedata.hpp
    \brief option exercise data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace data {

/*! Serializable object holding option exercise data for options that have been exercised.
    \ingroup tradedata
*/
class OptionExerciseData : public XMLSerializable {
public:
    //! Default constructor
    OptionExerciseData();

    //! Constructor taking an exercise date and exercise price.
    OptionExerciseData(const std::string& date, const std::string& price);

    //! \name Inspectors
    //@{
    const QuantLib::Date& date() const { return date_; }
    QuantLib::Real price() const { return price_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::string strDate_;
    std::string strPrice_;

    QuantLib::Date date_;
    QuantLib::Real price_;

    //! Initialisation
    void init();
};

} // namespace data
} // namespace ore
