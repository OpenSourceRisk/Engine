/*
    Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file portfolio/powerloadprofiledata.hpp
    \brief Power load profile data class
    \ingroup portfolio
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/date.hpp>
#include <qle/termstructures/intradaypowerloadtermstructure.hpp>

#include <map>

namespace ore {
namespace data {
//! Serializable object holding power load profile data

class PowerLoadProfileData : public XMLSerializable {
public:
    PowerLoadProfileData() {}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    const std::map<QuantLib::Date, QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>>& getLoadProfiles() const {
        return loadProfiles_;
    }

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>> loadProfiles_;
};

} // namespace data
} // namespace ore