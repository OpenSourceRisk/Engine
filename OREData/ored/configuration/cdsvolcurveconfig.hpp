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

/*! \file ored/configuration/cdsvolcurveconfig.hpp
    \brief CDS and index CDS volatility configuration
    \ingroup configuration
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ored/configuration/volatilityconfig.hpp>

namespace ore {
namespace data {

/*! CDS and index CDS volatility configuration

    If there is a need for different volatility surfaces for different terms, we expect the term to be
    a suffix on the curve configuration ID e.g. "-5Y" for a 5Y CDS, "-10Y" for a 10Y CDS etc. This term
    will be used to differentiate between market volatility quotes when building volatility structures. If
    the parsing of the term from the ID is not needed, it can be turned off below by setting \c parseTerm
    to \c false in the constructors.

    \ingroup configuration
*/
class CDSVolatilityCurveConfig : public CurveConfig {
public:
    //! Default constructor
    CDSVolatilityCurveConfig(bool parseTerm = false);

    //! Detailed constructor
    CDSVolatilityCurveConfig(const std::string& curveId, const std::string& curveDescription,
                             const boost::shared_ptr<VolatilityConfig>& volatilityConfig,
                             const std::string& dayCounter = "A365", const std::string& calendar = "NullCalendar",
                             const std::string& strikeType = "", const std::string& quoteName = "",
                             QuantLib::Real strikeFactor = 1.0, bool parseTerm = false);

    //! \name Inspectors
    //@{
    const boost::shared_ptr<VolatilityConfig>& volatilityConfig() const;
    const std::string& dayCounter() const;
    const std::string& calendar() const;
    const std::string& strikeType() const;
    const std::string& quoteName() const;
    QuantLib::Real strikeFactor() const;
    bool parseTerm() const;
    const std::pair<std::string, std::string>& nameTerm() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    boost::shared_ptr<VolatilityConfig> volatilityConfig_;
    std::string dayCounter_;
    std::string calendar_;
    std::string strikeType_;
    std::string quoteName_;
    QuantLib::Real strikeFactor_;
    bool parseTerm_;

    //! Name and term pair if the \c curveID_ is of that form and \c parseTerm_ is \c true.
    std::pair<std::string, std::string> nameTerm_;

    //! Attempt to populate the nameTerm_ pair from the curve configuration's ID.
    void populateNameTerm();

    //! Populate CurveConfig::quotes_ with the required quotes.
    void populateQuotes();

    //! Populate required curve ids
    void populateRequiredCurveIds();

    //! Give back the quote stem used in construction of the quote strings
    std::string quoteStem() const;
};

/*! Given an \p id of the form \c name-tenor, parse and return a pair of strings where the first element of the pair
    is the name and the second element is the tenor. A check is made that the tenor can be parsed as a
    QuantLib::Period. If the parse is not successful, the pair of strings that are returned are empty.
*/
std::pair<std::string, std::string> extractTermFromId(const std::string& id);

/*! Given an id of the form name-tenor, return name. For the form name, return name. */
std::string stripTermFromId(const std::string& id);

} // namespace data
} // namespace ore
