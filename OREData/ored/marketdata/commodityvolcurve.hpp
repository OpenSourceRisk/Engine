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

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>

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
    CommodityVolCurve(const QuantLib::Date& asof, const CommodityVolatilityCurveSpec& spec, const Loader& loader,
                      const CurveConfigurations& curveConfigs,
                      const std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves = {},
                      const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves = {},
                      const std::map<std::string, QuantLib::ext::shared_ptr<CommodityVolCurve>>& commodityVolCurves = {},
                      const map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVolCurves = {},
                      const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves = {},
                      const Market* fxIndices = nullptr, const bool buildCalibrationInfo = true);
    //@}

    //! \name Inspectors
    //@{
    const CommodityVolatilityCurveSpec& spec() const { return spec_; }
    const QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure>& volatility() { return volatility_; }

    //! Build the calibration info
    void buildVolCalibrationInfo(const Date& asof, QuantLib::ext::shared_ptr<VolatilityConfig>& volatilityConfig,
                                    const CurveConfigurations& curveConfigs, const CommodityVolatilityConfig& config);
    const QuantLib::ext::shared_ptr<FxEqCommVolCalibrationInfo>& calibrationInfo() const { return calibrationInfo_; }
    //@}

private:
    CommodityVolatilityCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> volatility_;
    QuantLib::ext::shared_ptr<QuantExt::FutureExpiryCalculator> expCalc_;
    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention_;
    QuantLib::Calendar calendar_;
    QuantLib::DayCounter dayCounter_;
    QuantLib::ext::shared_ptr<FxEqCommVolCalibrationInfo> calibrationInfo_;
    QuantLib::Date maxExpiry_;

    // Populated for delta, moneyness and possibly absolute strike surfaces and left empty for others
    QuantLib::Handle<QuantExt::PriceTermStructure> pts_;
    QuantLib::Handle<QuantLib::YieldTermStructure> yts_;

    //! Build a volatility structure from a single constant volatility quote
    void buildVolatility(const QuantLib::Date& asof, const CommodityVolatilityConfig& vc,
                         const ConstantVolatilityConfig& cvc, const Loader& loader);

    //! Build a volatility curve from a 1-D curve of volatility quotes
    void buildVolatility(const QuantLib::Date& asof, const CommodityVolatilityConfig& vc,
                         const VolatilityCurveConfig& vcc, const Loader& loader);

    //! Build a volatility surface from a collection of expiry and absolute strike pairs.
    void buildVolatility(const QuantLib::Date& asof, CommodityVolatilityConfig& vc,
                         const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader);

    /*! Build a volatility surface from a collection of expiry and absolute strike pairs where the strikes and
        expiries are both explicitly configured i.e. where wild cards are not used for either the strikes or
        the expiries.
    */
    void buildVolatilityExplicit(const QuantLib::Date& asof, CommodityVolatilityConfig& vc,
                                 const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                 const std::vector<QuantLib::Real>& configuredStrikes);

    /*! Build a volatility surface from a collection of expiry and strike pairs where the strikes are defined in
        terms of option delta and ATM values.
    */
    void buildVolatility(const QuantLib::Date& asof, CommodityVolatilityConfig& vc,
                         const VolatilityDeltaSurfaceConfig& vdsc, const Loader& loader);

    /*! Build a volatility surface from a collection of expiry and strike pairs where the strikes are defined in
        terms of moneyness levels.
    */
    void buildVolatility(const QuantLib::Date& asof, CommodityVolatilityConfig& vc,
                         const VolatilityMoneynessSurfaceConfig& vmsc, const Loader& loader);

    /*! Build a future price APO surface using a given price term structure. Use the natural expiries of the future
        options as the expiry pillar points and use configured moneyness levels in the strike dimension.
    */
    void buildVolatility(const QuantLib::Date& asof, CommodityVolatilityConfig& vc,
                         const VolatilityApoFutureSurfaceConfig& vapo,
                         const QuantLib::Handle<QuantLib::BlackVolTermStructure>& baseVts,
                         const QuantLib::Handle<QuantExt::PriceTermStructure>& basePts);

    //! Build a volatility surface as a proxy from another volatility surface
    void buildVolatility(const QuantLib::Date& asof, const CommodityVolatilityCurveSpec& spec,
                         const CurveConfigurations& curveConfigs, const ProxyVolatilityConfig& pvc,
                         const map<string, QuantLib::ext::shared_ptr<CommodityCurve>>& comCurves,
                         const map<string, QuantLib::ext::shared_ptr<CommodityVolCurve>>& volCurves,
                         const map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVolCurves,
                         const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves,
                         const Market* fxIndices = nullptr);

    /*! Assume that the input price curve \p pts is a future price curve giving the price of a sequence of future
        contracts at the contract expiry. Create a copy of this input curve with additional pillar points at
        the future option contract expiries. The price returned when this curve is queried at the option contract
        expiry or the future contract expiry is the expected future contract price i.e. the quoted future price. We
        need this to allow option surface creation with our current infrastructure for options on futures in
        particular where the surface creation relies on knowing the "forward" price.
    */
    QuantLib::Handle<QuantExt::PriceTermStructure>
    correctFuturePriceCurve(const QuantLib::Date& asof, const std::string& contractName,
                            const QuantLib::ext::shared_ptr<QuantExt::PriceTermStructure>& pts,
                            const std::vector<QuantLib::Date>& optionExpiries) const;

    //! Get an explicit expiry date from a commodity option quote's Expiry
    QuantLib::Date getExpiry(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<Expiry>& expiry,
                             const std::string& name, QuantLib::Natural rollDays) const;

    //! Populate price curve, \p pts_, and yield curve, \p yts_.
    void populateCurves(const CommodityVolatilityConfig& config,
                        const std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                        const std::map<std::string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves,
                        bool searchYield, bool dontThrow = false);

    //! Check and return moneyness levels.
    std::vector<QuantLib::Real> checkMoneyness(const std::vector<std::string>& moneynessLevels) const;
};

} // namespace data
} // namespace ore
