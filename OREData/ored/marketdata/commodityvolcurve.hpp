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
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <qle/time/futureexpirycalculator.hpp>
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
    CommodityVolCurve(
        const QuantLib::Date& asof,
        const CommodityVolatilityCurveSpec& spec,
        const Loader& loader,
        const CurveConfigurations& curveConfigs,
        const Conventions& conventions,
        const std::map<std::string, boost::shared_ptr<YieldCurve>>& yieldCurves = {},
        const std::map<std::string, boost::shared_ptr<CommodityCurve>>& commodityCurves = {});
    //@}

    //! \name Inspectors
    //@{
    const CommodityVolatilityCurveSpec& spec() const { return spec_; }
    const boost::shared_ptr<QuantLib::BlackVolTermStructure>& volatility() { return volatility_; }
    //@}

private:
    CommodityVolatilityCurveSpec spec_;
    boost::shared_ptr<QuantLib::BlackVolTermStructure> volatility_;
    boost::shared_ptr<QuantExt::FutureExpiryCalculator> expCalc_;
    QuantLib::Calendar calendar_;
    QuantLib::DayCounter dayCounter_;

    //! Build a volatility structure from a single constant volatlity quote
    void buildVolatility(
        const QuantLib::Date& asof,
        const CommodityVolatilityConfig& vc,
        const ConstantVolatilityConfig& cvc,
        const Loader& loader);

    //! Build a volatility curve from a 1-D curve of volatlity quotes
    void buildVolatility(
        const QuantLib::Date& asof,
        const CommodityVolatilityConfig& vc,
        const VolatilityCurveConfig& vcc,
        const Loader& loader);

    //! Build a volatility surface from a collection of expiry and absolute strike pairs.
    void buildVolatility(
        const QuantLib::Date& asof,
        CommodityVolatilityConfig& vc,
        const VolatilityStrikeSurfaceConfig& vssc,
        const Loader& loader);

    /*! Build a volatility surface from a collection of expiry and absolute strike pairs where the strikes and 
        expiries are both explicitly configured i.e. where wild cards are not used for either the strikes or 
        the expiries.
    */
    void buildVolatilityExplicit(
        const QuantLib::Date& asof,
        CommodityVolatilityConfig& vc,
        const VolatilityStrikeSurfaceConfig& vssc,
        const Loader& loader,
        const std::vector<QuantLib::Real>& configuredStrikes);

    /*! Build a volatility surface from a collection of expiry and strike pairs where the strikes are defined in 
        terms of option delta and ATM values.
    */
    void buildVolatility(
        const QuantLib::Date& asof,
        CommodityVolatilityConfig& vc,
        const VolatilityDeltaSurfaceConfig& vdsc,
        const Loader& loader,
        const QuantLib::Handle<QuantExt::PriceTermStructure>& pts,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& yts);

    //! Get an explicit expiry date from a commodity option quote's Expiry
    QuantLib::Date getExpiry(const QuantLib::Date& asof, const boost::shared_ptr<Expiry>& expiry,
        const std::string& name, QuantLib::Natural rollDays) const;
};

}
}
