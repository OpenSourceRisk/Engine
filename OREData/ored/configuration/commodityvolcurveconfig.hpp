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

/*! \file ored/configuration/commodityvolcurveconfig.hpp
    \brief Commodity volatility curve configuration
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>

namespace ore {
namespace data {

//! Commodity volatility configuration
/*! \ingroup configuration
*/
class CommodityVolatilityCurveConfig : public CurveConfig {
public:
    //! Supported types of commodity volatility
    enum class Type {
        //! Single volatility number applicable for all strikes and tenors
        Constant, 
        //! Time depending volatilities with one volatility for all strikes at a given tenor
        Curve, 
        //! Time and strike dependent surface of volatilities
        Surface
    };

    //! \name Constructors
    //@{
    //! Default constructor
    CommodityVolatilityCurveConfig() {}

    //! Constructor for single constant volatility i.e. Type is Constant
    CommodityVolatilityCurveConfig(const std::string& curveId,
        const std::string& curveDescription,
        const std::string& currency,
        const std::string& quote,
        const std::string& dayCounter = "A365",
        const std::string& calendar = "NullCalendar");

    //! Constructor for time depending volatilities i.e. Type is Curve
    CommodityVolatilityCurveConfig(const std::string& curveId,
        const std::string& curveDescription,
        const std::string& currency,
        const std::vector<std::string>& quotes,
        const std::string& dayCounter = "A365",
        const std::string& calendar = "NullCalendar",
        bool extrapolate = true);

    //! Constructor for volatility surface i.e. Type is Surface
    /*! Quotes are built up from the expiries and strikes strings
        \param expiries Can be period or date strings denoting option expiries
        \param strikes  Can be strike price or ATMF denoting ATM forward quote
    */
    CommodityVolatilityCurveConfig(const std::string& curveId, 
        const std::string& curveDescription,
        const std::string& currency, 
        const std::vector<std::string>& expiries, 
        const std::vector<std::string>& strikes,
        const std::string& dayCounter = "A365",
        const std::string& calendar = "NullCalendar",
        bool extrapolate = true,
        bool lowerStrikeConstantExtrapolation = false,
        bool upperStrikeConstantExtrapolation = false);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& currency() const { return currency_; }
    const Type& type() const { return type_; }
    const std::vector<std::string>& expiries() const { return expiries_; }
    const std::string& dayCounter() const { return dayCounter_; }
    const std::vector<std::string>& strikes() const { return strikes_; }
    const std::string& calendar() const { return calendar_; }
    bool extrapolate() const { return extrapolate_; }
    bool lowerStrikeConstantExtrapolation() const { return lowerStrikeConstantExtrapolation_; }
    bool upperStrikeConstantExtrapolation() const { return upperStrikeConstantExtrapolation_; }
    //@}

    //! \name Setters
    //@{
    std::string& currency() { return currency_; }
    Type& type() { return type_; }
    std::vector<std::string>& expiries() { return expiries_; }
    std::string& dayCounter() { return dayCounter_; }
    std::vector<std::string>& strikes() { return strikes_; }
    std::string& calendar() { return calendar_; }
    bool& extrapolate() { return extrapolate_; }
    bool& lowerStrikeConstantExtrapolation() { return lowerStrikeConstantExtrapolation_; }
    bool& upperStrikeConstantExtrapolation() { return upperStrikeConstantExtrapolation_; }
    //@}

private:
    void buildQuotes();
    std::string typeToString(const Type& type) const;
    Type stringToType(const std::string& type) const;

    std::string currency_;
    Type type_;
    std::vector<std::string> expiries_;
    std::vector<std::string> strikes_;
    std::string dayCounter_;
    std::string calendar_;
    bool extrapolate_;
    bool lowerStrikeConstantExtrapolation_;
    bool upperStrikeConstantExtrapolation_;
};

}
}
