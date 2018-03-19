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

/*! \file ored/configuration/commoditycurveconfig.hpp
    \brief Commodity curve configuration class
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>

namespace ore {
namespace data {

//! Commodity curve configuration
/*!
    \ingroup configuration
*/
class CommodityCurveConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    CommodityCurveConfig() {}
    //! Detailed constructor
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
        const std::string& commoditySpotQuote, const std::vector<std::string>& quotes, const std::string& dayCountId = "A365",
        const std::string& interpolationMethod = "Linear", bool extrapolation = true);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& currency() const { return currency_; }
    const std::string& commoditySpotQuoteId() const { return commoditySpotQuoteId_; }
    const std::string& dayCountId() const { return dayCountId_; }
    const std::string& interpolationMethod() const { return interpolationMethod_; }
    bool extrapolation() const { return extrapolation_; }
    //@}

    //! \name Setters
    //@{
    std::string& currency() { return currency_; }
    std::string& commoditySpotQuoteId() { return commoditySpotQuoteId_; }
    std::string& dayCountId() { return dayCountId_; }
    std::string& interpolationMethod() { return interpolationMethod_; }
    bool& extrapolation() { return extrapolation_; }
    //@}

private:
    std::string currency_;
    std::string commoditySpotQuoteId_;
    std::string dayCountId_;
    std::string interpolationMethod_;
    bool extrapolation_;
};
}
}
