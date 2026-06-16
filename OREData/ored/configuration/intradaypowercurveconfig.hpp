/*
 Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file ored/configuration/intradaypowercurveconfig.hpp
    \brief Intraday power curve configuration class
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>

namespace ore {
namespace data {

/*! Configuration for an intraday power price curve.

    The intraday power curve is built from:
    - A daily average price curve (a commodity curve) that provides the backbone price for each day.
    - A shape term structure identified by \p shapeQuoteName, which holds intraday shape factors
      (SHAPE_PROFILE/SHAPE_FACTOR quotes) that distribute the daily average price across intraday
      time slots.

    \ingroup configuration
*/
class IntradayPowerCurveConfig : public CurveConfig {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    IntradayPowerCurveConfig() {}

    IntradayPowerCurveConfig(const std::string& curveId, const std::string& curveDescription,
                             const std::string& currency, const std::string& dailyAveragePriceCurve,
                             const std::string& shapeQuoteName, const std::string& indexName = "");
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& currency() const { return currency_; }
    /*! Full spec name of the underlying daily average commodity curve,
        e.g. "Commodity/USD/PJM_WH_RT_AVG". */
    const std::string& dailyAveragePriceCurve() const { return dailyAveragePriceCurve_; }
    /*! Quote name prefix used to retrieve SHAPE_PROFILE/SHAPE_FACTOR market data. */
    const std::string& shapeQuoteName() const { return shapeQuoteName_; }
    const std::string& indexName() const { return indexName_; }
    //@}

    //! \name Setters
    //@{
    std::string& currency() { return currency_; }
    std::string& dailyAveragePriceCurve() { return dailyAveragePriceCurve_; }
    std::string& shapeQuoteName() { return shapeQuoteName_; }
    //@}

private:
    //! Populate \p requiredCurveIds_ with the underlying commodity curve dependency.
    void populateRequiredIds() const override;

    std::string currency_;
    std::string dailyAveragePriceCurve_;
    std::string shapeQuoteName_;
    std::string indexName_;
};

} // namespace data
} // namespace ore
