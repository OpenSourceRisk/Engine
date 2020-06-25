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

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/pricecurve.hpp>

namespace ore {
namespace data {

class CommodityCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityCurve() : regexQuotes_(false) {}

    //! Detailed constructor
    CommodityCurve(const QuantLib::Date& asof, const CommodityCurveSpec& spec, const Loader& loader,
                   const CurveConfigurations& curveConfigs, const Conventions& conventions,
                   const FXTriangulation& fxSpots = FXTriangulation(),
                   const std::map<std::string, boost::shared_ptr<YieldCurve>>& yieldCurves = {},
                   const std::map<std::string, boost::shared_ptr<CommodityCurve>>& commodityCurves = {});
    //@}

    //! \name Inspectors
    //@{
    const CommodityCurveSpec& spec() const { return spec_; }
    boost::shared_ptr<QuantExt::PriceTermStructure> commodityPriceCurve() const { return commodityPriceCurve_; }
    //@}

private:
    CommodityCurveSpec spec_;
    boost::shared_ptr<QuantExt::PriceTermStructure> commodityPriceCurve_;

    //! Store the commodity spot value with \c Null<Real>() indicating that none has been provided.
    QuantLib::Real commoditySpot_;

    //! Store the overnight value if any
    QuantLib::Real onValue_;

    //! Store the tomorrow next value if any
    QuantLib::Real tnValue_;

    //! Populated with \c true if the quotes are configured via a wildcard
    bool regexQuotes_;

    //! Store the interpolation method.
    std::string interpolationMethod_;

    //! Store the curve's day counter.
    QuantLib::DayCounter dayCounter_;

    //! Populate \p data with dates and prices from the loader
    void populateData(std::map<QuantLib::Date, QuantLib::Real>& data, const QuantLib::Date& asof,
                      const boost::shared_ptr<CommodityCurveConfig>& config, const Loader& loader,
                      const Conventions& conventions);

    //! Add node to price curve \p data with check for duplicate expiry dates
    void add(const QuantLib::Date& asof, const QuantLib::Date& expiry, QuantLib::Real value,
             std::map<QuantLib::Date, QuantLib::Real>& data, bool outright, QuantLib::Real pointsFactor = 1.0);

    //! Build price curve using the curve \p data
    void buildCurve(const QuantLib::Date& asof, const std::map<QuantLib::Date, QuantLib::Real>& data,
                    const boost::shared_ptr<CommodityCurveConfig>& config);

    //! Build cross currency commodity price curve
    void buildCrossCurrencyPriceCurve(const QuantLib::Date& asof, const boost::shared_ptr<CommodityCurveConfig>& config,
                                      const boost::shared_ptr<CommodityCurveConfig>& baseConfig,
                                      const FXTriangulation& fxSpots,
                                      const std::map<std::string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                      const std::map<std::string, boost::shared_ptr<CommodityCurve>>& commodityCurves);

    //! Build commodity basis price curve
    void buildBasisPriceCurve(const QuantLib::Date& asof, const CommodityCurveConfig& config,
                              const Conventions& conventions,
                              const QuantLib::Handle<QuantExt::PriceTermStructure>& basePts, const Loader& loader);

    //! Get the configured quotes
    std::vector<boost::shared_ptr<CommodityForwardQuote>>
    getQuotes(const QuantLib::Date& asof, const CommodityCurveConfig& config, const Loader& loader);

    //! Method for populating the price curve
    template <template <class> class CurveType, typename... Args> void populateCurve(Args... args);
};

template <template <class> class CurveType, typename... Args> void CommodityCurve::populateCurve(Args... args) {

    if (interpolationMethod_ == "Linear") {
        commodityPriceCurve_ = boost::make_shared<CurveType<QuantLib::Linear>>(args...);
    } else if (interpolationMethod_ == "LogLinear") {
        commodityPriceCurve_ = boost::make_shared<CurveType<QuantLib::LogLinear>>(args...);
    } else if (interpolationMethod_ == "Cubic") {
        commodityPriceCurve_ = boost::make_shared<CurveType<QuantLib::Cubic>>(args...);
    } else if (interpolationMethod_ == "LinearFlat") {
        commodityPriceCurve_ = boost::make_shared<CurveType<QuantExt::LinearFlat>>(args...);
    } else if (interpolationMethod_ == "LogLinearFlat") {
        commodityPriceCurve_ = boost::make_shared<CurveType<QuantExt::LogLinearFlat>>(args...);
    } else if (interpolationMethod_ == "CubicFlat") {
        commodityPriceCurve_ = boost::make_shared<CurveType<QuantExt::CubicFlat>>(args...);
    } else {
        QL_FAIL("The interpolation method, " << interpolationMethod_ << ", is not supported.");
    }
}

} // namespace data
} // namespace ore
