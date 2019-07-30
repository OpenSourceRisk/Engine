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

/*! \file ored/marketdata/commoditycurve.hpp
    \brief Class for building a commodity price curve
    \ingroup curves
*/

#pragma once

#include <qle/termstructures/pricecurve.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>

namespace ore {
namespace data {

class CommodityCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityCurve() {}

    //! Detailed constructor
    CommodityCurve(const QuantLib::Date& asof, const CommodityCurveSpec& spec, const Loader& loader,
                   const CurveConfigurations& curveConfigs, const Conventions& conventions);
    //@}

    //! \name Inspectors
    //@{
    const CommodityCurveSpec& spec() const { return spec_; }
    boost::shared_ptr<QuantExt::PriceTermStructure> commodityPriceCurve() const { return commodityPriceCurve_; }
    QuantLib::Real commoditySpot() const { return commoditySpot_; }
    //@}

private:
    CommodityCurveSpec spec_;
    boost::shared_ptr<QuantExt::PriceTermStructure> commodityPriceCurve_;

    //! Store the commodity spot value
    QuantLib::Real commoditySpot_;

    //! Store the overnight value if any
    QuantLib::Real onValue_;

    //! Store the tomorrow next value if any
    QuantLib::Real tnValue_;

    //! Populate \p data with dates and prices from the loader
    void populateData(std::map<QuantLib::Date, QuantLib::Real>& data, const QuantLib::Date& asof, 
        const boost::shared_ptr<CommodityCurveConfig>& config, const Loader& loader, const Conventions& conventions);

    //! Add node to price curve \p data with check for duplicate expiry dates
    void add(const QuantLib::Date& asof, const QuantLib::Date& expiry, QuantLib::Real value,
        std::map<QuantLib::Date, QuantLib::Real>& data, bool outright, QuantLib::Real pointsFactor = 1.0);

    //! Build price curve using the curve \p data
    void buildCurve(const std::map<QuantLib::Date, QuantLib::Real>& data,
        const boost::shared_ptr<CommodityCurveConfig>& config);
};
} // namespace data
} // namespace ore
