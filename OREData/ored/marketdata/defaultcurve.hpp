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
#include <ored/marketdata/yieldcurve.hpp>
#include <ql/termstructures/credit/interpolatedhazardratecurve.hpp>
#include <ql/termstructures/credit/piecewisedefaultcurve.hpp>

namespace ore {
namespace data {
using ore::data::Conventions;
using ore::data::CurveConfigurations;
using QuantLib::Date;

//! Wrapper class for building Swaption volatility structures
/*!
  \ingroup curves
*/
class DefaultCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    DefaultCurve() {}
    //! Detailed constructor
    DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                 const Conventions& conventions, map<string, boost::shared_ptr<YieldCurve>>& yieldCurves);
    //@}
    //! \name Inspectors
    //@{
    const DefaultCurveSpec& spec() const { return spec_; }
    const boost::shared_ptr<DefaultProbabilityTermStructure>& defaultTermStructure() { return curve_; }
    Real recoveryRate() { return recoveryRate_; }
    //@}
private:
    DefaultCurveSpec spec_;
    boost::shared_ptr<DefaultProbabilityTermStructure> curve_;
    Real recoveryRate_;

    //! Build a default curve from CDS spread quotes
    void buildCdsCurve(DefaultCurveConfig& config, const QuantLib::Date& asof, const DefaultCurveSpec& spec,
                       const Loader& loader, const Conventions& conventions,
                       std::map<std::string, boost::shared_ptr<YieldCurve>>& yieldCurves);

    //! Build a default curve from hazard rate quotes
    void buildHazardRateCurve(DefaultCurveConfig& config, const QuantLib::Date& asof, const DefaultCurveSpec& spec,
                              const Loader& loader, const Conventions& conventions);

    //! Build a default curve implied from a spread over a benchmark curve
    void buildBenchmarkCurve(DefaultCurveConfig& config, const QuantLib::Date& asof, const DefaultCurveSpec& spec,
                             const Loader& loader, const Conventions& conventions,
                             std::map<std::string, boost::shared_ptr<YieldCurve>>& yieldCurves);

    //! Get the quotes configured for a cds or hazard rate curve
    std::map<QuantLib::Period, QuantLib::Real>
    getConfiguredQuotes(DefaultCurveConfig& config, const QuantLib::Date& asof, const Loader& loader) const;

    //! Get all quotes matching the given regex string, \p strRegex
    std::map<QuantLib::Period, QuantLib::Real> getRegexQuotes(std::string strRegex, const std::string& configId,
                                                              DefaultCurveConfig::Type type, const QuantLib::Date& asof,
                                                              const Loader& loader) const;

    //! Get the explicit \p quotes
    std::map<QuantLib::Period, QuantLib::Real>
    getExplicitQuotes(const std::vector<std::pair<std::string, bool>>& quotes, const std::string& configId,
                      DefaultCurveConfig::Type type, const QuantLib::Date& asof, const Loader& loader) const;

    //! Add \p tenor and \p marketDatum value to \p quotes with check for duplicate tenors
    void addQuote(std::map<QuantLib::Period, QuantLib::Real>& quotes, const QuantLib::Period& tenor,
                  const MarketDatum& marketDatum, const std::string& configId) const;
};

} // namespace data
} // namespace ore
