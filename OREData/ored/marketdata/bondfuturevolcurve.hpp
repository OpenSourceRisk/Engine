/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file ored/marketdata/bondfuturevolcurve.hpp
    \brief Wrapper class for building bond future volatility structures
    \ingroup curves
*/

#pragma once
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/bondfuturevolcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <qle/termstructures/bondfuturevolstripper.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace ore {
namespace data {

/**
 * Wrapper class for building bond future volatility structures
 * \ingroup curves
 */
class BondFutureVolCurve
{
public:

    //! \name Constructors
    //@{
    //! Default constructor
    BondFutureVolCurve() {}

    //! Detailed constructor
    using YieldCurveCache = std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>;
    BondFutureVolCurve(
        QuantLib::Date asof,
        BondFutureVolatilityCurveSpec spec,
        const Loader& loader,
        const CurveConfigurations& curveConfigs,
        const YieldCurveCache& yieldCurves);
    //@}

    //! \name Inspectors
    //@{
    const BondFutureVolatilityCurveSpec& spec() const { return spec_; }
    const QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure>& volTermStructure() const { return vol_; }
    //@}

private:
    BondFutureVolatilityCurveSpec spec_;
    QuantLib::ext::shared_ptr<BlackVolTermStructure> vol_;
    QuantLib::Calendar calendar_;
    QuantLib::DayCounter dayCounter_;
    QuantLib::ext::shared_ptr<VolatilityConfig> volatilityConfig_;

    // Build a volatility surface from premia on a collection of expiry and absolute strike pairs.
    void buildVolatilityFromPremia(const QuantLib::Date& asof, BondFutureVolatilityConfig& vc,
        const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader, const YieldCurveCache& yieldCurves);

    // Build a volatility surface from volatilities on a collection of expiry and absolute strike pairs.
    void buildVolatilityFromVolatilities(const QuantLib::Date& asof, BondFutureVolatilityConfig& vc,
        const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader);

    // Generate configured strikes and expiries.
    struct ConfiguredStrikesExpiries {
        std::vector<QuantLib::Date> expiries;
        std::vector<QuantLib::Real> strikes;
        bool expiryWildcard;
        bool strikeWildcard;
    };
    ConfiguredStrikesExpiries generateStrikesExpiries(const VolatilityStrikeSurfaceConfig& vssc,
        const BondFutureVolatilityConfig& vc) const;

    // Add the volatility premium quotes to the `quotes` container.
    using QuoteSurface = QuantExt::BondFutureVolStripper::QuoteSurface;
    void populateVolatilityQuotes(const QuantLib::Date& asof, const BondFutureVolatilityConfig& vc,
        const Loader& loader, const ConfiguredStrikesExpiries& strikesExpiries, QuoteSurface& quotes,
        const std::string& quoteType);

    // Get future price quote.
    QuantLib::Handle<QuantLib::Quote> getFuturePriceQuote(const QuantLib::Date& asof,
        const BondFutureVolatilityConfig& vc, const Loader& loader) const;
};

} // namespace data
} // namespace ore
