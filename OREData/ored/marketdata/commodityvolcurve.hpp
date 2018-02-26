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

/*! \file ored/marketdata/commodityvolcurve.hpp
    \brief Wrapper class for building commodity volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace ore {
namespace data {

//! Wrapper class for building commodity volatility structures
/*! \ingroup curves
*/
class CommodityVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityVolCurve() {}

    //! Detailed constructor
    CommodityVolCurve(const QuantLib::Date& asof, 
        const CommodityVolatilityCurveSpec& spec, 
        const Loader& loader,
        const CurveConfigurations& curveConfigs);
    //@}

    //! \name Inspectors
    //@{
    const CommodityVolatilityCurveSpec& spec() const { return spec_; }
    const boost::shared_ptr<QuantLib::BlackVolTermStructure>& volatility() { return volatility_; }
    //@}

private:
    void buildConstantVolatility(const QuantLib::Date& asof, 
        CommodityVolatilityCurveConfig& config, const Loader& loader);
    void buildVolatilityCurve(const QuantLib::Date& asof,
        CommodityVolatilityCurveConfig& config, const Loader& loader);
    void buildVolatilitySurface(const QuantLib::Date& asof,
        CommodityVolatilityCurveConfig& config, const Loader& loader);

    CommodityVolatilityCurveSpec spec_;
    boost::shared_ptr<QuantLib::BlackVolTermStructure> volatility_;
};

}
}
