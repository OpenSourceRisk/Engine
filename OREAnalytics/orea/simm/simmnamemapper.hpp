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

/*! \file orea/simm/simmnamemapper.hpp
    \brief Abstract base class for classes that map names to SIMM qualifiers
*/

#pragma once

#include <string>
#include <ql/time/date.hpp>

namespace ore {
namespace analytics {

class SimmNameMapper {
public:
    //! Destructor
    virtual ~SimmNameMapper() {}

    /*! Return the SIMM <em>Qualifier</em> for a given external name.

        For example, an external name may be an ORE name for an equity.
        This should give back the SIMM <em>Qualifier</em>, i.e. in this
        case the ISIN in the form 'ISIN:XXXXXXXXXXXX' for that equity.

        This method should always return a value, if hasQualifier is false
        it should return the input externalName
     */
    virtual std::string qualifier(const std::string& externalName) const = 0;
   
    /*! Return if this qualifier has a mapping or not
     */
    virtual bool hasQualifier(const std::string& externalName) const = 0;

    /*! return the reverse lookup on qualifier
    */
    virtual std::string externalName(const std::string& qualifier) const = 0;

    /*! \todo Possibly extend the interface to have a method like the following:
        \code{.cpp}
        virtual std::string qualifier(const std::string& externalName,
            const SimmConfiguration::RiskType& riskType) const = 0;
        \endcode
    */
};

} // namespace analytics
} // namespace ore
