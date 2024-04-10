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

/*! \file ored/marketdata/defaultcurve.hpp
    \brief Wrapper class for building Default curves
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ql/termstructures/credit/interpolatedhazardratecurve.hpp>
#include <ql/termstructures/credit/piecewisedefaultcurve.hpp>
#include <qle/termstructures/creditcurve.hpp>

namespace ore {
namespace data {
using ore::data::Conventions;
using ore::data::CurveConfigurations;
using QuantLib::Date;

class YieldCurve;

//! Wrapper class for building Swaption volatility structures
/*!
  \ingroup curves
*/
class DefaultCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    DefaultCurve() : recoveryRate_(QuantLib::Null<QuantLib::Real>()) {}

    //! Detailed constructor
    DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                 map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                 map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& defaultCurves);
    //@}
    //! \name Inspectors
    //@{
    const DefaultCurveSpec& spec() const { return spec_; }
    const QuantLib::ext::shared_ptr<QuantExt::CreditCurve>& creditCurve() const { return curve_; }
    Real recoveryRate() { return recoveryRate_; }
    //@}
private:
    DefaultCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantExt::CreditCurve> curve_;
    Real recoveryRate_;

    //! Build a default curve from CDS spread quotes
    void buildCdsCurve(const std::string& curveID, const DefaultCurveConfig::Config& config, const QuantLib::Date& asof,
                       const DefaultCurveSpec& spec, const Loader& loader,
                       std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves);

    //! Build a default curve from hazard rate quotes
    void buildHazardRateCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                              const QuantLib::Date& asof, const DefaultCurveSpec& spec, const Loader& loader);

    //! Build a default curve implied from a spread over a benchmark curve
    void buildBenchmarkCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                             const QuantLib::Date& asof, const DefaultCurveSpec& spec, const Loader& loader,
                             std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves);

    //! Build a multi section curve
    void buildMultiSectionCurve(const std::string& curveID, const DefaultCurveConfig::Config& config, const Date& asof,
                                const DefaultCurveSpec& spec, const Loader& loader,
                                map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& defaultCurves);

    void buildTransitionMatrixCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                    const Date& asof, const DefaultCurveSpec& spec, const Loader& loader,
                                    map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& defaultCurves);

    //! Build a null curve (null rate, null recovery)
    void buildNullCurve(const std::string& curveID, const DefaultCurveConfig::Config& config, const Date& asof,
                        const DefaultCurveSpec& spec);
};

} // namespace data
} // namespace ore
