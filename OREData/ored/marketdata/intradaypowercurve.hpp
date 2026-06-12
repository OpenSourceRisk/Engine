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

/*! \file ored/marketdata/intradaypowercurve.hpp
    \brief Class for building an intraday power price curve
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <qle/indexes/intradaypowerindex.hpp>
#include <qle/termstructures/intradaypowerpricetermstructure.hpp>
#include <qle/termstructures/intradayshapetermstructure.hpp>

namespace ore {
namespace data {

//! Builder for an intraday power price curve and the associated intraday power index.
/*! The curve is built from:
    - the daily average commodity price curve referenced in the configuration (taken from the
      already built commodity curves the curve depends on), and
    - an intraday shape term structure built from all SHAPE_PROFILE/SHAPE_FACTOR/<ShapeQuoteName>/
      quotes found in the loader.

    \ingroup curves
*/
class IntradayPowerCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    IntradayPowerCurve() {}

    //! Detailed constructor
    IntradayPowerCurve(const QuantLib::Date& asof, const IntradayPowerCurveSpec& spec, const Loader& loader,
                       const CurveConfigurations& curveConfigs,
                       const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves = {});
    //@}

    //! \name Inspectors
    //@{
    const IntradayPowerCurveSpec& spec() const { return spec_; }
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerPriceTermStructure> intradayPowerPriceCurve() const {
        return curve_;
    }
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerIndex> intradayPowerIndex() const { return index_; }
    //@}

private:
    IntradayPowerCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerPriceTermStructure> curve_;
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerIndex> index_;

    //! Build the intraday shape term structure from SHAPE_PROFILE/SHAPE_FACTOR/<shapeQuoteName>/* quotes.
    QuantLib::ext::shared_ptr<QuantExt::IntradayShapeTermstructure>
    buildShape(const QuantLib::Date& asof, const std::string& shapeQuoteName, const Loader& loader) const;
};

} // namespace data
} // namespace ore
