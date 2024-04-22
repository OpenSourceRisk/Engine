/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/fxvolcurve.hpp
    \brief Wrapper class for building FX volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/correlationcurve.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/utilities/wildcard.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::BlackVolTermStructure;
using QuantLib::Date;

//! Wrapper class for building FX volatility structures
/*!
  \ingroup curves
*/
class FXVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FXVolCurve() {}
    //! Detailed constructor
    FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
               const ore::data::FXTriangulation& fxSpots,
               const std::map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
               const std::map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVols,
               const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves,
               const bool buildCalibrationInfo);
    //@}

    //! \name Inspectors
    //@{
    const FXVolatilityCurveSpec& spec() const { return spec_; }
    const QuantLib::ext::shared_ptr<BlackVolTermStructure>& volTermStructure() { return vol_; }
    QuantLib::ext::shared_ptr<FxEqCommVolCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}
private:
    FXVolatilityCurveSpec spec_;
    QuantLib::ext::shared_ptr<BlackVolTermStructure> vol_;
    Handle<Quote> fxSpot_;
    Handle<YieldTermStructure> domYts_, forYts_;
    string sourceCcy_, targetCcy_;
    std::vector<string> expiriesNoDuplicates_;
    std::vector<Period> expiries_;
    boost::optional<Wildcard> expiriesWildcard_;
    Size spotDays_;
    Calendar spotCalendar_;
    QuantLib::Period switchTenor_;
    QuantLib::DeltaVolQuote::AtmType atmType_;
    QuantLib::DeltaVolQuote::DeltaType deltaType_;
    QuantLib::DeltaVolQuote::AtmType longTermAtmType_;
    QuantLib::DeltaVolQuote::DeltaType longTermDeltaType_;
    QuantLib::Option::Type riskReversalInFavorOf_;
    bool butterflyIsBrokerStyle_;

    QuantLib::ext::shared_ptr<FxEqCommVolCalibrationInfo> calibrationInfo_;

    void init(Date asof, FXVolatilityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
              const FXTriangulation& fxSpots, const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
              const std::map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVols,
              const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves,
              const bool buildCalibrationInfo);

    void buildATMTriangulated(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                              QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                              const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                              const std::map<std::string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVols,
                              const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves);

    void buildSmileDeltaCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                              QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                              const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves);

    void buildSmileBfRrCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                             QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                             const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves);

    void buildVannaVolgaOrATMCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                   QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                                   const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves);

    void buildSmileAbsoluteCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                 QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                                 const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves);

};
} // namespace data
} // namespace ore
