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
#include <ored/utilities/to_string.hpp>

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

MarketCalibrationReportBase::MarketCalibrationReportBase(const std::string& calibrationFilter) {
    calibrationFilters_ = CalibrationFilters(calibrationFilter);
}

void MarketCalibrationReportBase::populateReport(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                                             const QuantLib::ext::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
                                             const std::string& label) {
    initialise(label);
    if (market == nullptr)
        return;
    auto t = QuantLib::ext::dynamic_pointer_cast<TodaysMarket>(market);
    if (!t) {
        DLOG("MarketCalibrationReportBase::populateReport() expected TodaysMarket");
        return;
    }

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
                try {
                    auto index = market->iborIndex(it.first);
                    if (yts != calibrationInfo->yieldCurveCalibrationInfo.end())
                        addYieldCurve(calibrationInfo->asof, yts->second, getCurveName(it.second), false, label, index);
                } catch (...) {
                    if (yts != calibrationInfo->yieldCurveCalibrationInfo.end())
                        addYieldCurve(calibrationInfo->asof, yts->second, getCurveName(it.second), false, label);
                }
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

    if (calibrationFilters_.mdFilterCpiVols) {
        // cpi vols
        for (auto const& c : calibrationInfo->cpiVolCalibrationInfo) {
            addCpiVol(calibrationInfo->asof, c.second, getCurveName(c.first), label);
        }
    }
}

MarketCalibrationReport::MarketCalibrationReport(const std::string& calibrationFilter, 
    const QuantLib::ext::shared_ptr<ore::data::Report>& report)
    : ore::analytics::MarketCalibrationReportBase(calibrationFilter), report_(report) {
    report_->addColumn("MarketObjectType", string())
        .addColumn("MarketObjectId", string())
        .addColumn("ResultId", string())
        .addColumn("ResultKey1", string())
        .addColumn("ResultKey2", string())
        .addColumn("ResultKey3", string())
        .addColumn("ResultType", string())
        .addColumn("ResultValue", string());
}

QuantLib::ext::shared_ptr<Report> MarketCalibrationReport::outputCalibrationReport() { 
    report_->end();
    return report_;
}

void MarketCalibrationReport::addRowReport(const std::string& moType, const std::string& moId,
                        const std::string& resId, const std::string& key1, const std::string& key2,
                        const std::string& key3, const boost::any& value) {
    auto p = parseBoostAny(value);
    report_->next().add(moType).add(moId).add(resId).add(key1).add(key2).add(key3).add(p.first).add(p.second);
}

const bool MarketCalibrationReportBase::checkCalibrations(string label, string type, string id) const {
    bool hasCalibration = false;
    if (calibrations_.find(label) != calibrations_.end() && calibrations_.at(label).find(type) != calibrations_.at(label).end()) {
        auto curves = calibrations_.at(label).at(type);
        if (curves.find(id) != curves.end())
            hasCalibration = true;
    }
    return hasCalibration;
}


void MarketCalibrationReport::addYieldCurveImpl(const QuantLib::Date& refdate,
    QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo> info,
    const std::string& id, bool isDiscount, const std::string& label,
    const string& type, QuantLib::Handle<QuantLib::IborIndex> iborIndex) {
    // common results
    addRowReport(type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowReport(type, id, "currency", "", "", "", info->currency);

    for (Size i = 0; i < info->pillarDates.size(); ++i) {
        std::string key1 = to_string(info->pillarDates[i]);
        addRowReport(type, id, "time", key1, "", "", info->times.at(i));
        addRowReport(type, id, "zeroRate", key1, "", "", info->zeroRates.at(i));
        addRowReport(type, id, "discountFactor", key1, "", "", info->discountFactors.at(i));
        if (!iborIndex.empty())
            addRowReport(type, id, "forwardRate", key1, "", "",
                         iborIndex->fixing(iborIndex->fixingCalendar().adjust(info->pillarDates[i], Preceding)));
    }

    // fitted bond curve results
    auto y = QuantLib::ext::dynamic_pointer_cast<FittedBondCurveCalibrationInfo>(info);
    if (y) {
        addRowReport(type, id, "fittedBondCurve.fittingMethod", "", "", "", y->fittingMethod);
        for (Size k = 0; k < y->solution.size(); ++k) {
            addRowReport(type, id, "fittedBondCurve.solution", std::to_string(k), "", "", y->solution[k]);
        }
        addRowReport(type, id, "fittedBondCurve.iterations", "", "", "", y->iterations);
        addRowReport(type, id, "fittedBondCurve.costValue", "", "", "", y->costValue);
        for (Size i = 0; i < y->securities.size(); ++i) {
            addRowReport(type, id, "fittedBondCurve.bondMaturity", y->securities.at(i), "", "",
                         y->securityMaturityDates.at(i));
            addRowReport(type, id, "fittedBondCurve.marketPrice", y->securities.at(i), "", "",
                         y->marketPrices.at(i));
            addRowReport(type, id, "fittedBondCurve.modelPrice", y->securities.at(i), "", "",
                         y->modelPrices.at(i));
            addRowReport(type, id, "fittedBondCurve.marketYield", y->securities.at(i), "", "",
                         y->marketYields.at(i));
            addRowReport(type, id, "fittedBondCurve.modelYield", y->securities.at(i), "", "", y->modelYields.at(i));
        }
    }
}

void MarketCalibrationReportBase::addYieldCurve(const QuantLib::Date& refdate,
                                            QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo> info,
                                            const std::string& id, bool isDiscount, const std::string& label,
                                            QuantLib::Handle<QuantLib::IborIndex> iborIndex) {
    if (info == nullptr)
        return;

    string yieldStr = "yieldCurve";

    // check if we have already processed this curve
    if (checkCalibrations(label, yieldStr, id)) {
        LOG("Skipping curve " << id << " for label " << label << " as it has already been added");
        return;
    }
    addYieldCurveImpl(refdate, info, id, isDiscount, label, yieldStr, iborIndex);
    
    calibrations_[label][yieldStr].insert(id);
}

void MarketCalibrationReport::addInflationCurveImpl(
    const QuantLib::Date& refdate, QuantLib::ext::shared_ptr<ore::data::InflationCurveCalibrationInfo> info,
    const std::string& id, const std::string& label, const std::string& type) {

    // common results
    addRowReport(type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowReport(type, id, "calendar", "", "", "", info->calendar);
    addRowReport(type, id, "baseDate", "", "", "", info->baseDate);

    // zero inflation
    auto z = QuantLib::ext::dynamic_pointer_cast<ZeroInflationCurveCalibrationInfo>(info);
    if (z) {
        addRowReport(type, id, "baseCpi", "", "", "", z->baseCpi);
        for (Size i = 0; i < z->pillarDates.size(); ++i) {
            std::string key1 = ore::data::to_string(z->pillarDates[i]);
            addRowReport(type, id, "time", key1, "", "", z->times.at(i));
            addRowReport(type, id, "zeroRate", key1, "", "", z->zeroRates.at(i));
            addRowReport(type, id, "cpi", key1, "", "", z->forwardCpis.at(i));
        }
    }

    // yoy inflation
    auto y = QuantLib::ext::dynamic_pointer_cast<YoYInflationCurveCalibrationInfo>(info);
    if (y) {
        for (Size i = 0; i < y->pillarDates.size(); ++i) {
            std::string key1 = ore::data::to_string(y->pillarDates[i]);
            addRowReport(type, id, "time", key1, "", "", y->times.at(i));
            addRowReport(type, id, "yoyRate", key1, "", "", y->yoyRates.at(i));
        }
    }
}

// Add inflation curve data to array
void MarketCalibrationReportBase::addInflationCurve(const QuantLib::Date& refdate,
                                                QuantLib::ext::shared_ptr<ore::data::InflationCurveCalibrationInfo> info,
                                                const std::string& id, const std::string& label) {
    if (info == nullptr)
        return;

    const string inflationStr = "inflationCuve";

    // check if we have already processed this curve
    if (checkCalibrations(label, inflationStr, id)) {
        DLOG("Skipping curve " << id << " for label " << label << " as it has already been added");
        return;
    }
    addInflationCurveImpl(refdate, info, id, label, inflationStr);

    calibrations_[label][inflationStr].insert(id);
}

// Add commodity curve data to array
void MarketCalibrationReport::addCommodityCurveImpl(
    const QuantLib::Date& refdate, QuantLib::ext::shared_ptr<ore::data::CommodityCurveCalibrationInfo> info,
    const std::string& id, const std::string& label, const std::string& type) {

    addRowReport(type, id, "calendar", "", "", "", info->calendar);
    addRowReport(type, id, "currenct", "", "", "", info->currency);
    addRowReport(type, id, "interpolationMethod", "", "", "", info->interpolationMethod);

    for (Size i = 0; i < info->pillarDates.size(); ++i) {
        auto date = to_string(info->pillarDates.at(i));
        addRowReport(type, id, "time", date, "", "", info->times.at(i));
        addRowReport(type, id, "price", date, "", "", info->futurePrices.at(i));
    }
}

void MarketCalibrationReportBase::addCommodityCurve(const QuantLib::Date& refdate,
                                                QuantLib::ext::shared_ptr<ore::data::CommodityCurveCalibrationInfo> info,
                                                const std::string& id, const std::string& label) {
    if (info == nullptr)
        return;

    const string commodityStr = "commodityCuve";

    // check if we have already processed this curve
    if (checkCalibrations(label, commodityStr, id)) {
        DLOG("Skipping curve " << id << " for label " << label << " as it has already been added");
        return;
    }
    addCommodityCurveImpl(refdate, info, id, label, commodityStr);
    calibrations_[label][commodityStr].insert(id);
}

void MarketCalibrationReport::addEqFxVolImpl(const string& type,
                                             QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo> info,
                                             const string& id, const string& label) {
    addRowReport(type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowReport(type, id, "calendar", "", "", "", info->calendar);
    addRowReport(type, id, "atmType", "", "", "", info->atmType);
    addRowReport(type, id, "deltaType", "", "", "", info->deltaType);
    addRowReport(type, id, "longTermAtmType", "", "", "", info->longTermAtmType);
    addRowReport(type, id, "longTermDeltaType", "", "", "", info->longTermDeltaType);
    addRowReport(type, id, "switchTenor", "", "", "", info->switchTenor);
    addRowReport(type, id, "riskReversalInFavorOf", "", "", "", info->riskReversalInFavorOf);
    addRowReport(type, id, "butterflyStyle", "", "", "", info->butterflyStyle);
    addRowReport(type, id, "isArbitrageFree", "", "", "", info->isArbitrageFree);
    for (Size i = 0; i < info->messages.size(); ++i)
        addRowReport(type, id, "message_" + std::to_string(i), "", "", "", info->messages[i]);

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        addRowReport(type, id, "expiry", tStr, "", "", info->expiryDates.at(i));
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size j = 0; j < info->deltas.size(); ++j) {
            std::string dStr = info->deltas.at(j);
            addRowReport(type, id, "forward", tStr, dStr, "", info->forwards.at(i));
            addRowReport(type, id, "strike", tStr, dStr, "", info->deltaGridStrikes.at(i).at(j));
            addRowReport(type, id, "vol", tStr, dStr, "", info->deltaGridImpliedVolatility.at(i).at(j));
            addRowReport(type, id, "prob", tStr, dStr, "", info->deltaGridProb.at(i).at(j));
            addRowReport(type, id, "call_premium", tStr, dStr, "", info->deltaCallPrices.at(i).at(j));
            addRowReport(type, id, "put_premium", tStr, dStr, "", info->deltaPutPrices.at(i).at(j));
            addRowReport(type, id, "callSpreadArb", tStr, dStr, "",
                         static_cast<bool>(info->deltaGridCallSpreadArbitrage.at(i).at(j)));
            addRowReport(type, id, "butterflyArb", tStr, dStr, "",
                         static_cast<bool>(info->deltaGridButterflyArbitrage.at(i).at(j)));
        }
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size j = 0; j < info->moneyness.size(); ++j) {
            std::string mStr = std::to_string(info->moneyness.at(j));
            addRowReport(type, id, "forward", tStr, mStr, "", info->forwards.at(i));
            addRowReport(type, id, "strike", tStr, mStr, "", info->moneynessGridStrikes.at(i).at(j));
            addRowReport(type, id, "vol", tStr, mStr, "", info->moneynessGridImpliedVolatility.at(i).at(j));
            addRowReport(type, id, "call_premium", tStr, mStr, "", info->moneynessCallPrices.at(i).at(j));
            addRowReport(type, id, "put_premium", tStr, mStr, "", info->moneynessPutPrices.at(i).at(j));
            addRowReport(type, id, "prob", tStr, mStr, "", info->moneynessGridProb.at(i).at(j));
            addRowReport(type, id, "callSpreadArb", tStr, mStr, "",
                         static_cast<bool>(info->moneynessGridCallSpreadArbitrage.at(i).at(j)));
            addRowReport(type, id, "butterflyArb", tStr, mStr, "",
                         static_cast<bool>(info->moneynessGridButterflyArbitrage.at(i).at(j)));
            addRowReport(type, id, "calendarArb", tStr, mStr, "",
                         static_cast<bool>(info->moneynessGridCalendarArbitrage.at(i).at(j)));
        }
    }
}

void MarketCalibrationReportBase::addEqFxVol(const string& type,
                                         QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo> info,
                                         const string& id, const string& label) {
    if (info == nullptr)
        return;

    // check if we have already processed this curve
    if (checkCalibrations(label, type, id)) {
        DLOG("Skipping curve " << id << " for label " << label << " as it has already been added");
        return;
    }
    addEqFxVolImpl(type, info, id, label);
    calibrations_[label][type].insert(id);
}

// Add fx/eq/comm vol curve data to array
void MarketCalibrationReportBase::addFxVol(const QuantLib::Date& refdate,
                                       QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo> info,
                                       const std::string& id, const std::string& label) {
    addEqFxVol("fxVol", info, id, label);
}

void MarketCalibrationReportBase::addEqVol(const QuantLib::Date& refdate,
                                       QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo> info,
                                       const std::string& id, const std::string& label) {
    addEqFxVol("eqVol", info, id, label);
}

void MarketCalibrationReportBase::addCommVol(const QuantLib::Date& refdate,
                                         QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo> info,
                                         const std::string& id, const std::string& label) {
    addEqFxVol("commVol", info, id, label);
}


void MarketCalibrationReport::addIrVolImpl(const QuantLib::Date& refdate,
                                       QuantLib::ext::shared_ptr<ore::data::IrVolCalibrationInfo> info,
                                       const std::string& id, const std::string& label, const std::string& type) {
    addRowReport(type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowReport(type, id, "calendar", "", "", "", info->calendar);
    addRowReport(type, id, "isArbitrageFree", "", "", "", info->isArbitrageFree);
    addRowReport(type, id, "volatilityType", "", "", "", info->volatilityType);
    for (Size i = 0; i < info->messages.size(); ++i)
        addRowReport(type, id, "message_" + std::to_string(i), "", "", "", info->messages[i]);

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        addRowReport(type, id, "expiry", tStr, "", "", info->expiryDates.at(i));
    }

    for (Size i = 0; i < info->underlyingTenors.size(); ++i) {
        addRowReport(type, id, "tenor", std::to_string(i), "", "", ore::data::to_string(info->underlyingTenors.at(i)));
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size u = 0; u < info->underlyingTenors.size(); ++u) {
            std::string uStr = ore::data::to_string(info->underlyingTenors[u]);
            for (Size j = 0; j < info->strikes.size(); ++j) {
                std::string kStr = std::to_string(info->strikes.at(j));
                addRowReport(type, id, "forward", tStr, kStr, uStr, info->forwards.at(i).at(u));
                addRowReport(type, id, "strike", tStr, kStr, uStr, info->strikeGridStrikes.at(i).at(u).at(j));
                addRowReport(type, id, "vol", tStr, kStr, uStr, info->strikeGridImpliedVolatility.at(i).at(u).at(j));
                addRowReport(type, id, "prob", tStr, kStr, uStr, info->strikeGridProb.at(i).at(u).at(j));
                addRowReport(type, id, "callSpreadArb", tStr, kStr, uStr,
                             static_cast<bool>(info->strikeGridCallSpreadArbitrage.at(i).at(u).at(j)));
                addRowReport(type, id, "butterflyArb", tStr, kStr, uStr,
                             static_cast<bool>(info->strikeGridButterflyArbitrage.at(i).at(u).at(j)));
            }
        }
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size u = 0; u < info->underlyingTenors.size(); ++u) {
            std::string uStr = ore::data::to_string(info->underlyingTenors[u]);
            for (Size j = 0; j < info->strikeSpreads.size(); ++j) {
                std::string kStr = std::to_string(info->strikeSpreads.at(j));
                addRowReport(type, id, "forward", tStr, kStr, uStr, info->forwards.at(i).at(u));
                addRowReport(type, id, "strike", tStr, kStr, uStr, info->strikeSpreadGridStrikes.at(i).at(u).at(j));
                addRowReport(type, id, "vol", tStr, kStr, uStr,
                             info->strikeSpreadGridImpliedVolatility.at(i).at(u).at(j));
                addRowReport(type, id, "prob", tStr, kStr, uStr, info->strikeSpreadGridProb.at(i).at(u).at(j));
                addRowReport(type, id, "callSpreadArb", tStr, kStr, uStr,
                             static_cast<bool>(info->strikeSpreadGridCallSpreadArbitrage.at(i).at(u).at(j)));
                addRowReport(type, id, "butterflyArb", tStr, kStr, uStr,
                             static_cast<bool>(info->strikeSpreadGridButterflyArbitrage.at(i).at(u).at(j)));
            }
        }
    }
}

// Add ir vol curve data to array
void MarketCalibrationReportBase::addIrVol(const QuantLib::Date& refdate,
                                       QuantLib::ext::shared_ptr<ore::data::IrVolCalibrationInfo> info, 
                                       const std::string& id, const std::string& label) {
    if (info == nullptr)
        return;

    string type = "irVol";
    
    // check if we have already processed this curve
    if (checkCalibrations(label, type, id)) {
        DLOG("Skipping curve " << id << " for label " << label << " as it has already been added");
        return;
    }
    addIrVolImpl(refdate, info, id, label, type);    

    calibrations_[label][type].insert(id);
}

void MarketCalibrationReport::addCpiVolImpl(const QuantLib::Date& refdate,
                                        QuantLib::ext::shared_ptr<ore::data::CpiVolCalibrationInfo> info,
                                        const std::string& id, const std::string& label, const string& type) {
    addRowReport(type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowReport(type, id, "calendar", "", "", "", info->calendar);
    addRowReport(type, id, "isArbitrageFree", "", "", "", info->isArbitrageFree);

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        addRowReport(type, id, "expiry", tStr, "", "", info->expiryDates.at(i));
        addRowReport(type, id, "optionObservation", tStr, "", "", info->optionObservationDates.at(i));
        addRowReport(type, id, "optionLifeTime", tStr, "", "", info->optionLifeTimes.at(i));
        addRowReport(type, id, "forward", tStr, "", "", info->forwards.at(i));
        addRowReport(type, id, "forwardCPI", tStr, "", "", info->forwardCPI.at(i));
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size j = 0; j < info->strikes.size(); ++j) {
            std::string kStr = std::to_string(info->strikes.at(j));
            addRowReport(type, id, "forward", tStr, kStr, "", info->forwards.at(i));
            addRowReport(type, id, "vol", tStr, kStr, "", info->strikeGridImpliedVolatility.at(i).at(j));
            addRowReport(type, id, "prob", tStr, kStr, "", info->strikeGridProb.at(i).at(j));
            addRowReport(type, id, "strikeCPI", tStr, kStr, "", info->strikeCPI.at(i).at(j));
            addRowReport(type, id, "callSpreadArb", tStr, kStr, "",
                         static_cast<bool>(info->strikeGridCallSpreadArbitrage.at(i).at(j)));
            addRowReport(type, id, "butterflyArb", tStr, kStr, "",
                         static_cast<bool>(info->strikeGridButterflyArbitrage.at(i).at(j)));
        }
    }
}

// Add cpi vol curve data to array
void MarketCalibrationReportBase::addCpiVol(const QuantLib::Date& refdate,
    QuantLib::ext::shared_ptr<ore::data::CpiVolCalibrationInfo> info,
    const std::string& id, const std::string& label) {

    if (info == nullptr)
        return;

    string type = "cpiVol";

    // check if we have already processed this curve
    if (checkCalibrations(label, type, id)) {
        DLOG("Skipping curve " << id << " for label " << label << " as it has already been added");
        return;
    }
    addCpiVolImpl(refdate, info, id, label, type);

    calibrations_[label][type].insert(id);
}
} // namespace analytics
} // namespace ore
