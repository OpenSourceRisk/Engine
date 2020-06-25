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

#include <boost/shared_ptr.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ored/configuration/volatilityconfig.hpp>

namespace ore {
namespace data {

//! Commodity volatility configuration
/*! \ingroup configuration
 */
class CommodityVolatilityConfig : public CurveConfig {
public:
    //! Default constructor
    CommodityVolatilityConfig();

    //! Explicit constructor
    CommodityVolatilityConfig(const std::string& curveId, const std::string& curveDescription,
                              const std::string& currency, const boost::shared_ptr<VolatilityConfig>& volatilityConfig,
                              const std::string& dayCounter = "A365", const std::string& calendar = "NullCalendar",
                              const std::string& futureConventionsId = "", QuantLib::Natural optionExpiryRollDays = 0,
                              const std::string& priceCurveId = "", const std::string& yieldCurveId = "");

    //! \name Inspectors
    //@{
    const std::string& currency() const;
    const boost::shared_ptr<VolatilityConfig>& volatilityConfig() const;
    const std::string& dayCounter() const;
    const std::string& calendar() const;
    const std::string& futureConventionsId() const;
    QuantLib::Natural optionExpiryRollDays() const;
    const std::string& priceCurveId() const;
    const std::string& yieldCurveId() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::string currency_;
    boost::shared_ptr<VolatilityConfig> volatilityConfig_;
    std::string dayCounter_;
    std::string calendar_;
    std::string futureConventionsId_;
    QuantLib::Natural optionExpiryRollDays_;
    std::string priceCurveId_;
    std::string yieldCurveId_;

    //! Populate CurveConfig::quotes_ with the required quotes.
    void populateQuotes();
};

} // namespace data
} // namespace ore
