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
    /*! The type of commodity curve that has been configured:
         - Direct: if the commodity price curve is built from commodity forward quotes
         - CrossCurrency: if the commodity price curve is implied from a price curve in a different currency
         - Basis: if the commodity price curve is built from basis quotes
    */
    enum class Type { Direct, CrossCurrency, Basis };

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    CommodityCurveConfig() {}

    //! Detailed constructor for Direct commodity curve configuration
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::vector<std::string>& quotes, const std::string& commoditySpotQuote = "",
                         const std::string& dayCountId = "A365", const std::string& interpolationMethod = "Linear",
                         bool extrapolation = true, const std::string& conventionsId = "");
    
    //! Detailed constructor for CrossCurrency commodity curve configuration
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
        const std::string& basePriceCurveId, const std::string& baseYieldCurveId,
        const std::string& yieldCurveId, bool extrapolation = true);

    //! Detailed constructor for Basis commodity curve configuration
    CommodityCurveConfig(const std::string& curveId,
        const std::string& curveDescription,
        const std::string& currency,
        const std::string& basePriceCurveId,
        const std::string& baseConventionsId,
        const std::vector<std::string>& basisQuotes,
        const std::string& basisConventionsId,
        const std::string& dayCountId = "A365",
        const std::string& interpolationMethod = "Linear",
        bool extrapolation = true,
        bool addBasis = true);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const Type& type() const { return type_; }
    const std::string& currency() const { return currency_; }
    const std::string& commoditySpotQuoteId() const { return commoditySpotQuoteId_; }
    const std::string& dayCountId() const { return dayCountId_; }
    const std::string& interpolationMethod() const { return interpolationMethod_; }
    const std::string& basePriceCurveId() const { return basePriceCurveId_; }
    const std::string& baseYieldCurveId() const { return baseYieldCurveId_; }
    const std::string& yieldCurveId() const { return yieldCurveId_; }
    bool extrapolation() const { return extrapolation_; }
    const vector<string>& fwdQuotes() const { return fwdQuotes_; }
    const std::string& conventionsId() const { return conventionsId_; }
    const std::string& baseConventionsId() const { return baseConventionsId_; }
    bool addBasis() const { return addBasis_; }
    //@}

    //! \name Setters
    //@{
    Type& type() { return type_; }
    std::string& currency() { return currency_; }
    std::string& commoditySpotQuoteId() { return commoditySpotQuoteId_; }
    std::string& dayCountId() { return dayCountId_; }
    std::string& interpolationMethod() { return interpolationMethod_; }
    std::string& basePriceCurveId() { return basePriceCurveId_; }
    std::string& baseYieldCurveId() { return baseYieldCurveId_; }
    std::string& yieldCurveId() { return yieldCurveId_; }
    bool& extrapolation() { return extrapolation_; }
    std::string& conventionsId() { return conventionsId_; }
    std::string& baseConventionsId() { return baseConventionsId_; }
    bool& addBasis() { return addBasis_; }
    //@}

private:
    Type type_;
    vector<string> fwdQuotes_;
    std::string currency_;
    std::string commoditySpotQuoteId_;
    std::string dayCountId_;
    std::string interpolationMethod_;
    std::string basePriceCurveId_;
    std::string baseYieldCurveId_;
    std::string yieldCurveId_;
    bool extrapolation_;
    std::string conventionsId_;
    std::string baseConventionsId_;
    bool addBasis_;
};

} // namespace data
} // namespace ore
