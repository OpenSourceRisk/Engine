/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
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
    CommodityCurve();

    //! Detailed constructor
    CommodityCurve(const QuantLib::Date& asof, const CommodityCurveSpec& spec, const Loader& loader,
                   const CurveConfigurations& curveConfigs,
                   const FXTriangulation& fxSpots = FXTriangulation(),
                   const std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves = {},
                   const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves = {},
                   bool const buildCalibrationInfo = false);
    //@}

    //! \name Inspectors
    //@{
    const CommodityCurveSpec& spec() const { return spec_; }
    QuantLib::ext::shared_ptr<QuantExt::PriceTermStructure> commodityPriceCurve() const { return commodityPriceCurve_; }
    QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> commodityIndex() const { return commodityIndex_; }
    QuantLib::ext::shared_ptr<CommodityCurveCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}

private:
    CommodityCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantExt::PriceTermStructure> commodityPriceCurve_;
    QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> commodityIndex_;
    QuantLib::ext::shared_ptr<CommodityCurveCalibrationInfo> calibrationInfo_;

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
    void populateData(std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote>>& data, const QuantLib::Date& asof,
                      const QuantLib::ext::shared_ptr<CommodityCurveConfig>& config, const Loader& loader);

    //! Add node to price curve \p data with check for duplicate expiry dates
    void add(const QuantLib::Date& asof, const QuantLib::Date& expiry, QuantLib::Real value,
             std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote>>& data, bool outright, QuantLib::Real pointsFactor = 1.0);

    //! Build price curve using the curve \p data
    void buildCurve(const QuantLib::Date& asof, const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote>>& data,
                    const QuantLib::ext::shared_ptr<CommodityCurveConfig>& config);

    //! Build cross currency commodity price curve
    void buildCrossCurrencyPriceCurve(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<CommodityCurveConfig>& config,
                                      const QuantLib::ext::shared_ptr<CommodityCurveConfig>& baseConfig,
                                      const FXTriangulation& fxSpots,
                                      const std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                                      const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves);

    //! Build commodity basis price curve
    void buildBasisPriceCurve(const QuantLib::Date& asof, const CommodityCurveConfig& config,
                              const QuantLib::Handle<QuantExt::PriceTermStructure>& basePts, const Loader& loader);

    //! Build commodity piecewise price curve
    void buildPiecewiseCurve(const QuantLib::Date& asof, const CommodityCurveConfig& config,
        const Loader& loader,
        const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves);

    //! Get the configured quotes. If filter is \c true, remove tenor based quotes and quotes with expiry before asof.
    std::vector<QuantLib::ext::shared_ptr<CommodityForwardQuote>>
    getQuotes(const QuantLib::Date& asof, const std::string& /*configId*/, const std::vector<std::string>& quotes,
        const Loader& loader, bool filter = false);

    //! Method for populating the price curve
    template <template <class> class CurveType, typename... Args> void populateCurve(Args... args);

    //! Add the instruments relating to a \p priceSegment to \p instruments.
    using Helper = QuantLib::BootstrapHelper<QuantExt::PriceTermStructure>;
    void addInstruments(const QuantLib::Date& asof, const Loader& loader, const std::string& configId,
        const std::string& currency, const PriceSegment& priceSegment,
        const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves,
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<Helper>>& instruments);

    //! Special method to add instruments when the \p priceSegment is \c OffPeakPowerDaily
    void addOffPeakPowerInstruments(const QuantLib::Date& asof, const Loader& loader, const std::string& configId,
        const PriceSegment& priceSegment,
        std::map<QuantLib::Date, QuantLib::ext::shared_ptr<Helper>>& instruments);
};

template <template <class> class CurveType, typename... Args> void CommodityCurve::populateCurve(Args... args) {

    if (interpolationMethod_ == "Linear") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantLib::Linear>>(args...);
    } else if (interpolationMethod_ == "LogLinear") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantLib::LogLinear>>(args...);
    } else if (interpolationMethod_ == "Cubic") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantLib::Cubic>>(args...);
    } else if (interpolationMethod_ == "Hermite") {
         commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantLib::Cubic>>(
             args..., QuantLib::Cubic(QuantLib::CubicInterpolation::Parabolic));
    } else if (interpolationMethod_ == "LinearFlat") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantExt::LinearFlat>>(args...);
    } else if (interpolationMethod_ == "LogLinearFlat") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantExt::LogLinearFlat>>(args...);
    } else if (interpolationMethod_ == "CubicFlat") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantExt::CubicFlat>>(args...);
    } else if (interpolationMethod_ == "HermiteFlat") {
         commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantExt::HermiteFlat>>(args...);
    } else if (interpolationMethod_ == "BackwardFlat") {
        commodityPriceCurve_ = QuantLib::ext::make_shared<CurveType<QuantLib::BackwardFlat>>(args...);
    } else {
        QL_FAIL("The interpolation method, " << interpolationMethod_ << ", is not supported.");
    }
}

} // namespace data
} // namespace ore
