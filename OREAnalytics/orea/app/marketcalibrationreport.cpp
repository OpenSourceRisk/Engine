/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/app/marketcalibrationreport.hpp>
#include <ored/marketdata/todaysmarket.hpp>

using namespace ore::data;

namespace {
// Convert "Yield/EUR/EUR-EONIA" to "EUR-EONIA"
std::string getCurveName(const std::string& spec) {
    auto pos = spec.rfind("/");
    if (pos != std::string::npos) {
        return spec.substr(pos + 1);
    } else {
        return spec;
    }
}
}

namespace ore {
namespace analytics {

MarketCalibrationReport::MarketCalibrationReport(const std::string& calibrationFilter) {
    calibrationFilters_ = CalibrationFilters(calibrationFilter);
}

void MarketCalibrationReport::populateReport(const boost::shared_ptr<ore::data::Market>& market,
                                             const boost::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
                                             const std::string& label) {
    initialise(label);
    if (market == nullptr)
        return;
    auto t = boost::dynamic_pointer_cast<TodaysMarket>(market);
    QL_REQUIRE(t, "MarketCalibrationReport::populateReport(): expected TodaysMarket (internal error)");
    auto calibrationInfo = t->calibrationInfo();
    if (calibrationFilters_.mdFilterCurves) {
        // First cut at adding curves

        // TODO simplify that and just loop over yield and dividend curve calibration info, the only change
        // would be that we would not be able to set the discountCurve flag any more, not sure if that is very
        // important? add to curves here Add discount curves first, so EUR-EONIA gets marked as a discount curve
        // even if it is an IndexCurve too
        if (todaysMarketParams->hasMarketObject(MarketObject::DiscountCurve)) {
            for (auto it : todaysMarketParams->mapping(MarketObject::DiscountCurve, Market::defaultConfiguration)) {
                auto yts = calibrationInfo->yieldCurveCalibrationInfo.find(it.second);
                if (yts != calibrationInfo->yieldCurveCalibrationInfo.end())
                    addYieldCurve(calibrationInfo->asof, yts->second, getCurveName(it.second), true, label);
            }
        }
        if (todaysMarketParams->hasMarketObject(MarketObject::YieldCurve)) {
            for (auto it : todaysMarketParams->mapping(MarketObject::YieldCurve, Market::defaultConfiguration)) {
                auto yts = calibrationInfo->yieldCurveCalibrationInfo.find(it.second);
                if (yts != calibrationInfo->yieldCurveCalibrationInfo.end())
                    addYieldCurve(calibrationInfo->asof, yts->second, getCurveName(it.second), false, label);
            }
        }
        if (todaysMarketParams->hasMarketObject(MarketObject::EquityCurve)) {
            for (auto it : todaysMarketParams->mapping(MarketObject::EquityCurve, Market::defaultConfiguration)) {
                auto yts = calibrationInfo->dividendCurveCalibrationInfo.find(it.second);
                if (yts != calibrationInfo->dividendCurveCalibrationInfo.end())
                    addYieldCurve(calibrationInfo->asof, yts->second, getCurveName(it.second), false, label);
            }
        }
        if (todaysMarketParams->hasMarketObject(MarketObject::IndexCurve)) {
            for (auto it : todaysMarketParams->mapping(MarketObject::IndexCurve, Market::defaultConfiguration)) {
                auto yts = calibrationInfo->yieldCurveCalibrationInfo.find(it.second);
                if (yts != calibrationInfo->yieldCurveCalibrationInfo.end())
                    addYieldCurve(calibrationInfo->asof, yts->second, getCurveName(it.second), false, label);
            }
        }
    }

    if (calibrationFilters_.mdFilterInfCurves) {
        // inflation curves
        for (auto const& c : calibrationInfo->inflationCurveCalibrationInfo) {
            addInflationCurve(calibrationInfo->asof, c.second, getCurveName(c.first), label);
        }
    }

    if (calibrationFilters_.mdFilterCommCurves) {
        // commodity curves
        for (auto const& c : calibrationInfo->commodityCurveCalibrationInfo) {
            addCommodityCurve(calibrationInfo->asof, c.second, getCurveName(c.first), label);
        }
    }

    if (calibrationFilters_.mdFilterFxVols) {
        // fx vol surfaces
        for (auto const& c : calibrationInfo->fxVolCalibrationInfo) {
            addFxVol(calibrationInfo->asof, c.second, getCurveName(c.first), label);
        }
    }

    if (calibrationFilters_.mdFilterEqVols) {
        // eq vol surfaces
        for (auto const& c : calibrationInfo->eqVolCalibrationInfo) {
            addEqVol(calibrationInfo->asof, c.second, getCurveName(c.first), label);
        }
    }

    if (calibrationFilters_.mdFilterCommVols){
        // commodity vols
        for (auto const& c: calibrationInfo->commVolCalibrationInfo){
            addCommVol(calibrationInfo->asof, c.second, getCurveName(c.first), label);
        }
    }

    if (calibrationFilters_.mdFilterIrVols) {
        // ir vol surfaces
        for (auto const& c : calibrationInfo->irVolCalibrationInfo) {
            addIrVol(calibrationInfo->asof, c.second, c.first, label);
        }
    }
}

} // namespace analytics
} // namespace ore
