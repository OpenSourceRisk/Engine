/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <ored/marketdata/volsurfacebuilder.hpp>
#include <ored/utilities/log.hpp>

#include <qle/termstructures/blackvolsurfacesvi.hpp>
#include <qle/termstructures/svimodeltraits.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> buildSviSurface(
    const std::string& curveLabel,
    const QuantLib::Date& asof,
    const std::vector<QuantLib::Date>& dates,
    const std::vector<std::vector<QuantLib::Real>>& strikes,
    const std::vector<std::vector<QuantLib::Real>>& quotes,
    const std::vector<std::vector<QuantLib::Option::Type>>& optionTypes,
    const QuantLib::DayCounter& dayCounter,
    const QuantLib::Calendar& calendar,
    const QuantLib::Handle<QuantLib::Quote>& spot,
    QuantLib::Size spotDays,
    const QuantLib::Calendar& spotCalendar,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve1,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve2,
    QuantExt::SviParametricVolatility::ModelVariant sviModelVariant,
    const QuantLib::ext::optional<ParametricSmileConfiguration>& parametricSmileConfiguration,
    const std::string& interpolationModel,
    QuantExt::ParametricVolatility::MarketQuoteType inputMarketQuoteType,
    bool logPerStrikeFit) {

    std::map<std::pair<QuantLib::Real, QuantLib::Real>,
             std::vector<std::pair<QuantLib::Real, QuantExt::ParametricVolatility::ParameterCalibration>>>
        modelParameters;
    QuantLib::Size maxCalibrationAttempts = 10;
    QuantLib::Real exitEarlyErrorThreshold = 0.005;
    QuantLib::Real maxAcceptableError = 0.05;

    if (parametricSmileConfiguration) {
        auto const& psc = *parametricSmileConfiguration;
        auto const& params = psc.parameters();
        QuantLib::Size expectedSize = QuantExt::SviModelTraits::expectedParametersSize(sviModelVariant);
        QL_REQUIRE(params.size() == expectedSize,
                   curveLabel << ": ParametricSmileConfiguration has "
                       << params.size() << " parameters, but model variant " << interpolationModel
                       << " expects " << expectedSize);
        for (auto const& p : params) {
            QL_REQUIRE(p.initialValue.size() == 1 || p.initialValue.size() == dates.size(),
                       curveLabel << ": ParametricSmileConfiguration parameter '"
                           << p.name << "' has " << p.initialValue.size()
                           << " initial values, expected 1 or " << dates.size());
        }
        for (QuantLib::Size j = 0; j < dates.size(); ++j) {
            std::vector<std::pair<QuantLib::Real, QuantExt::ParametricVolatility::ParameterCalibration>> paramVec;
            for (auto const& p : params) {
                QuantLib::Real val = p.initialValue.size() == 1 ? p.initialValue.front() : p.initialValue[j];
                paramVec.push_back(std::make_pair(val, p.calibration));
            }
            QuantLib::Real t = dayCounter.yearFraction(asof, dates[j]);
            modelParameters[std::make_pair(t, QuantLib::Null<QuantLib::Real>())] = paramVec;
        }
        maxCalibrationAttempts = psc.calibration().maxCalibrationAttempts;
        exitEarlyErrorThreshold = psc.calibration().exitEarlyErrorThreshold;
        maxAcceptableError = psc.calibration().maxAcceptableError;
    }

    auto result = QuantLib::ext::make_shared<QuantExt::BlackVolatilitySurfaceSvi>(
        asof, dates, strikes, quotes, dayCounter, calendar, spot, spotDays, spotCalendar, curve1, curve2,
        sviModelVariant, inputMarketQuoteType, optionTypes, modelParameters,
        maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);

    // Force calibration by triggering one evaluation, then log calibration RMSE
    try {
        if (!dates.empty() && !strikes.empty() && !strikes.front().empty())
            result->blackVol(1.0, strikes.front().front());
        auto sviPV = QuantLib::ext::dynamic_pointer_cast<QuantExt::SviParametricVolatility>(result->parametricVolatility());
        if (sviPV) {
            auto const& rmseVol = sviPV->volRmseShiftedLognormal();
            for (QuantLib::Size j = 0; j < rmseVol.columns(); ++j) {
                for (QuantLib::Size i = 0; i < rmseVol.rows(); ++i) {
                    QuantLib::Real r = rmseVol(i, j);
                    if (r != QuantLib::Null<QuantLib::Real>()) {
                        QuantLib::Size nStrikes = j < strikes.size() ? strikes[j].size() : 0;
                        DLOG(curveLabel << " SVI (" << interpolationModel << ") expiry[" << j
                             << "] vol RMSE = " << r << " (normalised by max vol, " << nStrikes << " strikes)");
                    }
                }
            }
            QuantLib::Real gv = sviPV->globalVolRmseShiftedLognormal();
            QuantLib::Real gp = sviPV->globalVolRmsePrice();
            QuantLib::Real gt = sviPV->globalVolRmseTotalVariance();
            if (gv != QuantLib::Null<QuantLib::Real>()) {
                QuantLib::Size nTotalStrikes = 0;
                for (auto const& s : strikes)
                    nTotalStrikes += s.size();
                DLOG(curveLabel << " SVI (" << interpolationModel << ") global RMSE"
                     << " vol=" << gv << " price=" << gp << " totalVar=" << gt
                     << " (" << dates.size() << " expiries, " << nTotalStrikes << " strikes)");
            }
            if (logPerStrikeFit) {
                for (QuantLib::Size j = 0; j < dates.size() && j < strikes.size(); ++j) {
                    QuantLib::Real t = dayCounter.yearFraction(asof, dates[j]);
                    for (QuantLib::Size k = 0; k < strikes[j].size() && k < quotes[j].size(); ++k) {
                        QuantLib::Real mktVol = quotes[j][k];
                        QuantLib::Real fitVol = QuantLib::Null<QuantLib::Real>();
                        try { fitVol = result->blackVol(t, strikes[j][k]); } catch (...) {}
                        DLOG(curveLabel << " SVI fit expiry[" << j << "] strike=" << strikes[j][k]
                             << " mktVol=" << mktVol << " fitVol=" << fitVol);
                    }
                }
            }
        }
    } catch (...) {}

    return result;
}

} // namespace data
} // namespace ore
