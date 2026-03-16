/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/cdsvolcurve.hpp
    \brief Class for building cds volatility structures.
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/defaultcurve.hpp>
#include <ored/marketdata/loader.hpp>
#include <qle/termstructures/creditvolcurve.hpp>

namespace ore {
namespace data {

/*! Class for building CDS option volatility structures
    \ingroup curves
*/
class CDSVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CDSVolCurve() {}

    //! Detailed constructor
    CDSVolCurve(QuantLib::Date asof, CDSVolatilityCurveSpec spec, const Loader& loader,
                const CurveConfigurations& curveConfigs,
                const std::map<std::string, QuantLib::ext::shared_ptr<CDSVolCurve>>& requiredCdsVolCurves = {},
                const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves = {});
    //@}

    //! \name Inspectors
    //@{
    const CDSVolatilityCurveSpec& spec() const { return spec_; }
    const QuantLib::ext::shared_ptr<QuantExt::CreditVolCurve>& volTermStructure() { return vol_; }
    //@}

private:
    CDSVolatilityCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantExt::CreditVolCurve> vol_;
    QuantLib::Calendar calendar_;
    QuantLib::DayCounter dayCounter_;
    QuantExt::CreditVolCurve::Type strikeType_;

    //! Build a volatility structure from a single constant volatility quote
    void buildVolatility(const QuantLib::Date& asof, const CDSVolatilityCurveConfig& vc,
                         const ConstantVolatilityConfig& cvc, const Loader& loader);

    //! Build a volatility curve from a 1-D curve of volatility quotes
    void buildVolatility(const QuantLib::Date& asof, const CDSVolatilityCurveConfig& vc,
                         const VolatilityCurveConfig& vcc, const Loader& loader);

    //! Build a volatility surface from a collection of expiry and absolute strike pairs.
    void buildVolatility(const QuantLib::Date& asof, CDSVolatilityCurveConfig& vc,
                         const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                         const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves);

    void buildVolatility(const Date& asof, const CDSVolatilityCurveSpec& spec, const CDSVolatilityCurveConfig& vc,
                         const CDSProxyVolatilityConfig& pvc,
                         const std::map<std::string, QuantLib::ext::shared_ptr<CDSVolCurve>>& requiredCdsVolCurves,
                         const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves);

    /*! Build a volatility surface from a collection of expiry and absolute strike pairs where the strikes and
        expiries are both explicitly configured i.e. where wild cards are not used for either the strikes or
        the expiries.
    */
    void buildVolatilityExplicit(const QuantLib::Date& asof, CDSVolatilityCurveConfig& vc,
                                 const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                 const std::vector<QuantLib::Real>& configuredStrikes,
                                 const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves);

    //! Get an explicit expiry date from a CDS option quote's Expiry
    QuantLib::Date getExpiry(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<Expiry>& expiry) const;
};

} // namespace data
} // namespace ore
