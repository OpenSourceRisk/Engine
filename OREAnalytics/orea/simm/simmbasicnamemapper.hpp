/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/simm/simmbasicnamemapper.hpp
    \brief Basic SIMM class for mapping names to SIMM qualifiers
*/

#pragma once

#include <orea/simm/simmnamemapper.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/time/date.hpp>

#include <map>
#include <string>

namespace ore {
namespace analytics {

class SimmBasicNameMapper : public SimmNameMapper, public ore::data::XMLSerializable {
public:
    /*! Return the SIMM <em>Qualifier</em> for a given external name.

        \warning If the \p externalName cannot be mapped to a qualifier
                 the externalName itself is returned. In this way, the
                 mapper is basic and places the burden on the caller to
                 call it only when a mapping is needed.
     */
    std::string qualifier(const std::string& externalName) const override;

    //! Qualifier mapping expiry (ISO date), may be blank and interpreted as QL::maxDate()
    std::string validTo(const std::string& externalName) const;
    //! Qualifier mapping expiry (QuantLib date)
    QuantLib::Date validToDate(const std::string& externalName) const;
    //! Qualifier mapping start date (ISO date), may be blank and interpreted as QL::minDate()
    std::string validFrom(const std::string& externalName) const;
    //! Qualifier mapping startDate (QuantLib date)
    QuantLib::Date validFromDate(const std::string& externalName) const;

    //! Has qualifier
    bool hasQualifier(const std::string& externalName) const override;

    //! Has qualifier that is valid w.r.t. given reference date
    bool hasValidQualifier(const std::string& externalName, const QuantLib::Date& referenceDate) const;

    //! reverse lookup on qualifier
    std::string externalName(const std::string& qualifier) const override;

    //! \name Serialisation
    //@{
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    void fromXML(ore::data::XMLNode* node) override;
    //@}

    //! Add a single mapping
    /*! \todo Not very convenient. If deemed useful, add more methods for adding
              mappings e.g. <code>addMappingRange</code> that adds a range of
              mappings at once
    */
    void addMapping(const std::string& externalName, const std::string& qualifier, const std::string& validFrom = "",
                    const std::string& validTo = "");

protected:
    /*! A mapping from external name to SIMM <em>Qualifier</em>
     */
    std::map<std::string, std::string> mapping_;

    /*! Start and expiry date (ISO) of each mapping
     */
    std::map<std::string, std::string> validFrom_, validTo_;
};

} // namespace analytics
} // namespace ore
