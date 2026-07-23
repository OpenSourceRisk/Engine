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

#include <orea/app/reportwriters/xvareportwriter.hpp>

#include <orea/aggregation/postprocess.hpp>
#include <orea/app/analytics/xvaexplainanalytic.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <ostream>

using ore::data::to_string;
using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

namespace {

void addTradeExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                       const string& tradeId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual(ActualActual::ISDA);
    const vector<Real>& epe = postProcess->tradeEPE(tradeId);
    const vector<Real>& ene = postProcess->tradeENE(tradeId);
    const vector<Real>& ee_b = postProcess->tradeEE_B(tradeId);
    const vector<Real>& eee_b = postProcess->tradeEEE_B(tradeId);
    const vector<Real>& pfe = postProcess->tradePFE(tradeId);
    const vector<Real>& aepe = postProcess->allocatedTradeEPE(tradeId);
    const vector<Real>& aene = postProcess->allocatedTradeENE(tradeId);
    const vector<Real>& epe_b = postProcess->tradeEPE_B_timeWeighted(tradeId);
    const vector<Real>& eepe_b = postProcess->tradeEEPE_B_timeWeighted(tradeId);
    report.next()
        .add(tradeId)
        .add(today)
        .add(0.0)
        .add(epe[0])
        .add(ene[0])
        .add(aepe[0])
        .add(aene[0])
        .add(pfe[0])
        .add(ee_b[0])
        .add(eee_b[0])
        .add(epe_b[0])
        .add(eepe_b[0]);
    for (Size j = 0; j < dates.size(); ++j) {

        Time time = dc.yearFraction(today, dates[j]);
        report.next()
            .add(tradeId)
            .add(dates[j])
            .add(time)
            .add(epe[j + 1])
            .add(ene[j + 1])
            .add(aepe[j + 1])
            .add(aene[j + 1])
            .add(pfe[j + 1])
            .add(ee_b[j + 1])
            .add(eee_b[j + 1])
            .add(epe_b[j + 1])
            .add(eepe_b[j + 1]);
    }
}

void addNettingSetExposure(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                           const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual(ActualActual::ISDA);
    const vector<Real>& epe = postProcess->netEPE(nettingSetId);
    const vector<Real>& ene = postProcess->netENE(nettingSetId);
    const vector<Real>& ee_b = postProcess->netEE_B(nettingSetId);
    const vector<Real>& eee_b = postProcess->netEEE_B(nettingSetId);
    const vector<Real>& pfe = postProcess->netPFE(nettingSetId);
    const vector<Real>& ecb = postProcess->expectedCollateral(nettingSetId);
    const vector<Real>& epe_b = postProcess->netEPE_B_timeWeighted(nettingSetId);
    const vector<Real>& eepe_b = postProcess->netEEPE_B_timeWeighted(nettingSetId);
    report.next()
        .add(nettingSetId)
        .add(today)
        .add(0.0)
        .add(epe[0])
        .add(ene[0])
        .add(pfe[0])
        .add(ecb[0])
        .add(ee_b[0])
        .add(eee_b[0])
        .add(epe_b[0])
        .add(eepe_b[0]);
    for (Size j = 0; j < dates.size(); ++j) {
        Real time = dc.yearFraction(today, dates[j]);
        report.next()
            .add(nettingSetId)
            .add(dates[j])
            .add(time)
            .add(epe[j + 1])
            .add(ene[j + 1])
            .add(pfe[j + 1])
            .add(ecb[j + 1])
            .add(ee_b[j + 1])
            .add(eee_b[j + 1])
            .add(epe_b[j + 1])
            .add(eepe_b[j + 1]);
    }
}

void addNettingSetCvaSensitivities(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                   const string& nettingSetId) {
    const vector<Real> grid = postProcess->spreadSensitivityTimes();
    const vector<Real>& sensiHazardRate = postProcess->netCvaHazardRateSensitivity(nettingSetId);
    const vector<Real>& sensiCdsSpread = postProcess->netCvaSpreadSensitivity(nettingSetId);

    if (sensiHazardRate.size() == 0 || sensiCdsSpread.size() == 0)
        return;

    for (Size j = 0; j < grid.size(); ++j) {
        report.next().add(nettingSetId).add(grid[j]).add(sensiHazardRate[j]).add(sensiCdsSpread[j]);
    }
}

void addNettingSetColva(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                        const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual(ActualActual::ISDA);
    const vector<Real>& collateral = postProcess->expectedCollateral(nettingSetId);
    const vector<Real>& colvaInc = postProcess->colvaIncrements(nettingSetId);
    const vector<Real>& floorInc = postProcess->collateralFloorIncrements(nettingSetId);
    Real colva = postProcess->nettingSetCOLVA(nettingSetId);
    Real floorValue = postProcess->nettingSetCollateralFloor(nettingSetId);
    report.next()
        .add(nettingSetId)
        .add(Null<Date>())
        .add(Null<Real>())
        .add(Null<Real>())
        .add(Null<Real>())
        .add(colva)
        .add(Null<Real>())
        .add(floorValue);
    Real colvaSum = 0.0;
    Real floorSum = 0.0;
    for (Size j = 0; j < dates.size(); ++j) {
        Real time = dc.yearFraction(today, dates[j]);
        colvaSum += colvaInc[j + 1];
        floorSum += floorInc[j + 1];
        report.next()
            .add(nettingSetId)
            .add(dates[j])
            .add(time)
            .add(collateral[j + 1])
            .add(colvaInc[j + 1])
            .add(colvaSum)
            .add(floorInc[j + 1])
            .add(floorSum);
    }
}

} // namespace

void XvaReportWriter::writeTradeExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                       const string& tradeId) {
    report.addColumn("TradeId", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double())
        .addColumn("ENE", double())
        .addColumn("AllocatedEPE", double())
        .addColumn("AllocatedENE", double())
        .addColumn("PFE", double())
        .addColumn("BaselEE", double())
        .addColumn("BaselEEE", double())
        .addColumn("TimeWeightedBaselEPE", double(), 2)
        .addColumn("TimeWeightedBaselEEPE", double(), 2);
    
    addTradeExposures(report, postProcess, tradeId);
    report.end();
}

void XvaReportWriter::writeTradeExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess) {
    report.addColumn("TradeId", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double())
        .addColumn("ENE", double())
        .addColumn("AllocatedEPE", double())
        .addColumn("AllocatedENE", double())
        .addColumn("PFE", double())
        .addColumn("BaselEE", double())
        .addColumn("BaselEEE", double())
        .addColumn("TimeWeightedBaselEPE", double(), 2)
        .addColumn("TimeWeightedBaselEEPE", double(), 2);

    for (const auto& [tradeId, _] : postProcess->tradeIds()) {
        try {
            addTradeExposures(report, postProcess, tradeId);
        }
        catch (const std::exception& e) {
            QuantLib::ext::shared_ptr<Trade> failedTrade = postProcess->portfolio()->trades().find(tradeId)->second;
            map<string, string> subfields;
            subfields.insert({"tradeId", tradeId});
            subfields.insert({"tradeType", failedTrade->tradeType()});
            StructuredAnalyticsErrorMessage("Trade Exposure Report", "Error processing trade.", e.what(), subfields).log();
            report.end();
        }
    }
    report.end();
}

void XvaReportWriter::writeNettingSetExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                            const string& nettingSetId) {
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double(), 2)
        .addColumn("ENE", double(), 2)
        .addColumn("PFE", double(), 2)
        .addColumn("ExpectedCollateral", double(), 2)
        .addColumn("BaselEE", double(), 2)
        .addColumn("BaselEEE", double(), 2)
        .addColumn("TimeWeightedBaselEPE", double(), 2)
        .addColumn("TimeWeightedBaselEEPE", double(), 2);

    addNettingSetExposure(report, postProcess, nettingSetId);
    report.end();
}

void XvaReportWriter::writeNettingSetExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess) {
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double(), 2)
        .addColumn("ENE", double(), 2)
        .addColumn("PFE", double(), 2)
        .addColumn("ExpectedCollateral", double(), 2)
        .addColumn("BaselEE", double(), 2)
        .addColumn("BaselEEE", double(), 2)
        .addColumn("TimeWeightedBaselEPE", double(), 2)
        .addColumn("TimeWeightedBaselEEPE", double(), 2);

    for (const auto& [nettingSetId, _] : postProcess->nettingSetIds()) {
        try {
            addNettingSetExposure(report, postProcess, nettingSetId);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("Netting Set Exposure Report", "Error processing netting set.", e.what(),
                                            {{"nettingSetId", nettingSetId}}).log();
            report.end();
        }
    }
    report.end();
}

void XvaReportWriter::writeNettingSetCvaSensitivities(ore::data::Report& report,
                                                   QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                                   const string& nettingSetId) {
    report.addColumn("NettingSet", string())
        .addColumn("Time", double(), 6)
        .addColumn("CvaHazardRateSensitivity", double(), 6)
        .addColumn("CvaSpreadSensitivity", double(), 6);

    addNettingSetCvaSensitivities(report, postProcess, nettingSetId);
    report.end();
}

void XvaReportWriter::writeNettingSetCvaSensitivities(ore::data::Report& report,
                                                   QuantLib::ext::shared_ptr<PostProcess> postProcess) {
    report.addColumn("NettingSet", string())
        .addColumn("Time", double(), 6)
        .addColumn("CvaHazardRateSensitivity", double(), 6)
        .addColumn("CvaSpreadSensitivity", double(), 6);

    for (const auto& [nettingSetId, _] : postProcess->nettingSetIds()) {
        try {
            addNettingSetCvaSensitivities(report, postProcess, nettingSetId);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("Cva Sensi Report", "Error processing netting set.", e.what(),
                                            {{"nettingSetId", nettingSetId}}).log();
            report.end();
        }
    }
    report.end();
}

void XvaReportWriter::writeNettingSetColva(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                        const string& nettingSetId) {
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 4)
        .addColumn("CollateralBalance", double(), 4)
        .addColumn("COLVA Increment", double(), 4)
        .addColumn("COLVA", double(), 4)
        .addColumn("CollateralFloor Increment", double(), 4)
        .addColumn("CollateralFloor", double(), 4);

    addNettingSetColva(report, postProcess, nettingSetId);
    report.end();
}

void XvaReportWriter::writeNettingSetColva(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess) {
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 4)
        .addColumn("CollateralBalance", double(), 4)
        .addColumn("COLVA Increment", double(), 4)
        .addColumn("COLVA", double(), 4)
        .addColumn("CollateralFloor Increment", double(), 4)
        .addColumn("CollateralFloor", double(), 4);

    for (const auto& [nettingSetId, _] : postProcess->nettingSetIds()) {
        try {
            addNettingSetColva(report, postProcess, nettingSetId);
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("Netting Set Colva Report", "Error processing netting set.", e.what(),
                                            {{"nettingSetId", nettingSetId}}).log();
            report.end();
        }
    }
    report.end();
}

void XvaReportWriter::writeXVA(ore::data::Report& report, const string& allocationMethod,
                            QuantLib::ext::shared_ptr<Portfolio> portfolio, QuantLib::ext::shared_ptr<PostProcess> postProcess) {
    const vector<Date> dates = postProcess->cube()->dates();
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Size precision = 2;
    report.addColumn("TradeId", string())
        .addColumn("NettingSetId", string())
        .addColumn("CVA", double(), precision)
        .addColumn("DVA", double(), precision)
        .addColumn("FBA", double(), precision)
        .addColumn("FCA", double(), precision)
        .addColumn("FBAexOwnSP", double(), precision)
        .addColumn("FCAexOwnSP", double(), precision)
        .addColumn("FBAexAllSP", double(), precision)
        .addColumn("FCAexAllSP", double(), precision)
        .addColumn("COLVA", double(), precision)
        .addColumn("MVA", double(), precision)
        .addColumn("OurKVACCR", double(), precision)
        .addColumn("TheirKVACCR", double(), precision)
        .addColumn("OurKVACVA", double(), precision)
        .addColumn("TheirKVACVA", double(), precision)
        .addColumn("CollateralFloor", double(), precision)
        .addColumn("AllocatedCVA", double(), precision)
        .addColumn("AllocatedDVA", double(), precision)
        .addColumn("AllocationMethod", string())
        .addColumn("BaselEPE", double(), precision)
        .addColumn("BaselEEPE", double(), precision);

    for (const auto& [n, _] : postProcess->nettingSetIds()) {
        try {
            postProcess->nettingSetCVA(n);
            report.next()
                .add("")
                .add(n)
                .add(postProcess->nettingSetCVA(n))
                .add(postProcess->nettingSetDVA(n))
                .add(postProcess->nettingSetFBA(n))
                .add(postProcess->nettingSetFCA(n))
                .add(postProcess->nettingSetFBA_exOwnSP(n))
                .add(postProcess->nettingSetFCA_exOwnSP(n))
                .add(postProcess->nettingSetFBA_exAllSP(n))
                .add(postProcess->nettingSetFCA_exAllSP(n))
                .add(postProcess->nettingSetCOLVA(n))
                .add(postProcess->nettingSetMVA(n))
                .add(postProcess->nettingSetOurKVACCR(n))
                .add(postProcess->nettingSetTheirKVACCR(n))
                .add(postProcess->nettingSetOurKVACVA(n))
                .add(postProcess->nettingSetTheirKVACVA(n))
                .add(postProcess->nettingSetCollateralFloor(n))
                .add(postProcess->nettingSetCVA(n))
                .add(postProcess->nettingSetDVA(n))
                .add(allocationMethod)
                .add(postProcess->netEPE_B(n))
                .add(postProcess->netEEPE_B(n));
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("XVA Report", "Error during writing xva for netting set.", e.what(),
                                            {{"nettingSetId", n}})
                .log();
        }

        for (auto& [tid, trade] : portfolio->trades()) {

            string nid = trade->envelope().nettingSetId();
            if (nid != n)
                continue;
            try {
                postProcess->tradeCVA(tid);
                report.next()
                    .add(tid)
                    .add(nid)
                    .add(postProcess->tradeCVA(tid))
                    .add(postProcess->tradeDVA(tid))
                    .add(postProcess->tradeFBA(tid))
                    .add(postProcess->tradeFCA(tid))
                    .add(postProcess->tradeFBA_exOwnSP(tid))
                    .add(postProcess->tradeFCA_exOwnSP(tid))
                    .add(postProcess->tradeFBA_exAllSP(tid))
                    .add(postProcess->tradeFCA_exAllSP(tid))
                    .add(Null<Real>())
                    .add(Null<Real>())
                    .add(Null<Real>())
                    .add(Null<Real>())
                    .add(Null<Real>())
                    .add(Null<Real>())
                    .add(Null<Real>())
                    .add(postProcess->allocatedTradeCVA(tid))
                    .add(postProcess->allocatedTradeDVA(tid))
                    .add(allocationMethod)
                    .add(postProcess->tradeEPE_B(tid))
                    .add(postProcess->tradeEEPE_B(tid));
            } catch (const std::exception& e) {
                StructuredAnalyticsErrorMessage("XVA Report", "Error during writing xva for trade.", e.what(),
                                                {{"tradeId", n}})
                    .log();
            }
        }
    }
    report.end();
}

void XvaReportWriter::writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data) {
    report.addColumn("Date", Size()).addColumn("Scenario", Size());
    for (auto const& k : data.keys()) {
        std::string tmp = ore::data::to_string(k.first) + k.second;
        report.addColumn(tmp.c_str(), double(), 8);
    }
    for (Size d = 0; d < data.dimDates(); ++d) {
        for (Size s = 0; s < data.dimSamples(); ++s) {
            report.next();
            report.add(d).add(s);
            for (auto const& k : data.keys()) {
                report.add(data.get(d, s, k.first, k.second));
            }
        }
    }
    report.end();
}

void XvaReportWriter::writeXvaSensitivityReport(Report& report, const QuantLib::ext::shared_ptr<SensitivityStream>& ssTrades,
        const QuantLib::ext::shared_ptr<SensitivityStream>& ssNettingSets, const std::map<std::string, std::string>& tradeNettingSetMap, Real outputThreshold,
                                             Size outputPrecision) {

    LOG("Writing XVA Sensitivity report");

    Size shiftSizePrecision = outputPrecision < 6 ? 6 : outputPrecision;
    Size amountPrecision = outputPrecision < 2 ? 2 : outputPrecision;

    report.addColumn("NettingSetId", string());
    report.addColumn("TradeId", string());
    report.addColumn("IsPar", string());
    report.addColumn("Factor_1", string());
    report.addColumn("ShiftSize_1", double(), shiftSizePrecision);
    report.addColumn("Factor_2", string());
    report.addColumn("ShiftSize_2", double(), shiftSizePrecision);
    report.addColumn("Currency", string());
    report.addColumn("Base XVA", double(), amountPrecision);
    report.addColumn("Delta", double(), amountPrecision);
    report.addColumn("Gamma", double(), amountPrecision);

    // Make sure that we are starting from the start
    ssTrades->reset();
    while (SensitivityRecord sr = ssTrades->next()) {
        if ((outputThreshold == Null<Real>()) ||
            (fabs(sr.delta) > outputThreshold || (sr.gamma != Null<Real>() && fabs(sr.gamma) > outputThreshold))) {
            auto it = tradeNettingSetMap.find(sr.tradeId);
            report.next();
            report.add(it != tradeNettingSetMap.end() ? it->second : "");
            report.add(sr.tradeId);
            report.add(ore::data::to_string(sr.isPar));
            report.add(prettyPrintInternalCurveName(QuantExt::reconstructFactor(sr.key_1, sr.desc_1)));
            report.add(sr.shift_1);
            report.add(prettyPrintInternalCurveName(QuantExt::reconstructFactor(sr.key_2, sr.desc_2)));
            report.add(sr.shift_2);
            report.add(sr.currency);
            report.add(sr.baseNpv);
            report.add(sr.delta);
            report.add(sr.gamma);
        } else if (!std::isfinite(sr.delta) || !std::isfinite(sr.gamma)) {
            // TODO: Again, is this needed?
            ALOG("sensitivity record has infinite values: " << sr);
        }
    }

    ssNettingSets->reset();
    while (SensitivityRecord sr = ssNettingSets->next()) {
        if ((outputThreshold == Null<Real>()) ||
            (fabs(sr.delta) > outputThreshold || (sr.gamma != Null<Real>() && fabs(sr.gamma) > outputThreshold))) {
            report.next();
            report.add(sr.tradeId);
            report.add("");
            report.add(ore::data::to_string(sr.isPar));
            report.add(prettyPrintInternalCurveName(QuantExt::reconstructFactor(sr.key_1, sr.desc_1)));
            report.add(sr.shift_1);
            report.add(prettyPrintInternalCurveName(QuantExt::reconstructFactor(sr.key_2, sr.desc_2)));
            report.add(sr.shift_2);
            report.add(sr.currency);
            report.add(sr.baseNpv);
            report.add(sr.delta);
            report.add(sr.gamma);
        } else if (!std::isfinite(sr.delta) || !std::isfinite(sr.gamma)) {
            // TODO: Again, is this needed?
            ALOG("sensitivity record has infinite values: " << sr);
        }
    }

    report.end();
    LOG("Sensitivity report finished");
}

void XvaReportWriter::writeTimeAveragedNettedExposure(
    ore::data::Report& report,
    const std::map<std::string, std::vector<NettedExposureCalculator::TimeAveragedExposure>>& data) {

    LOG("Writing time averaged netted exposure");

    report.addColumn("NettingSetId", string())
        .addColumn("Sample", Size())
        .addColumn("PosExpNoColl", double(), 4)
        .addColumn("NegExpNoColl", double(), 4)
        .addColumn("PosExpWithColl", double(), 4)
        .addColumn("NegExpWithColl", double(), 4);

    for (auto const& [n, avg] : data) {
        for (Size k = 0; k < avg.size(); ++k) {
            report.next()
                .add(n)
                .add(k)
                .add(avg[k].positiveExposureBeforeCollateral)
                .add(avg[k].negativeExposureBeforeCollateral)
                .add(avg[k].positiveExposureAfterCollateral)
                .add(avg[k].negativeExposureAfterCollateral);
        }
    }

    report.end();
}

void XvaReportWriter::writeXvaExplainReport(ore::data::Report& report, const ore::analytics::XvaExplainResults& xvaData) {
    try {
        report.addColumn("RiskFactor", string())
            .addColumn("TradeId", string())
            .addColumn("NettingSetId", string())
            .addColumn("CVA_Base", double(), 4)
            .addColumn("CVA", double(), 4)
            .addColumn("Change", double(), 4);

        // Add full reval report
        for (const auto& [key, baseValue] : xvaData.baseCvaData()) {
            report.next();
            auto it = xvaData.fullRevalCva().find(key);
            auto scenarioValue = it == xvaData.fullRevalCva().end() ? 0.0 : it->second;
            report.add("ALL")
                .add(key.tradeId)
                .add(key.nettingSet)
                .add(baseValue)
                .add(scenarioValue)
                .add(scenarioValue - baseValue);
        }

        for (const auto& [rfKey, scenarioValues] : xvaData.fullRevalScenarioCva()) {
            for (const auto& [key, baseValue] : xvaData.baseCvaData()) {
                report.next();
                auto it = scenarioValues.find(key);
                auto scenarioValue = it == scenarioValues.end() ? 0.0 : it->second;
                report.add(to_string(rfKey))
                    .add(key.tradeId)
                    .add(key.nettingSet)
                    .add(baseValue)
                    .add(scenarioValue)
                    .add(scenarioValue - baseValue);
            }
        }
    } catch (const std::exception& e) {
        ALOG("Failed to write xva explain report, got " << e.what());
    }
}

void XvaReportWriter::writeXvaExplainSummary(ore::data::Report& report, const ore::analytics::XvaExplainResults& xvaData) {
    try {
        std::map<XvaExplainResults::XvaReportKey, std::map<RiskFactorKey::KeyType, double>> aggData;

        for (auto& [rfKey, scenarioValues] : xvaData.fullRevalScenarioCva()) {
            for (const auto& [key, baseValue] : xvaData.baseCvaData()) {
                auto it = scenarioValues.find(key);
                auto scenarioValue = it == scenarioValues.end() ? 0.0 : it->second;
                auto diff = scenarioValue - baseValue;
                aggData[key][rfKey.keytype] += diff;
            }
        }

        report.addColumn("TradeId", string())
            .addColumn("NettingSet", string())
            .addColumn("CVA_Base", double(), 4)
            .addColumn("CVA", double(), 4)
            .addColumn("Change", double(), 4);

        for (const auto& keyType : xvaData.keyTypes()) {
            report.addColumn(to_string(keyType), double(), 4);
        }

        // Add full reval report
        for (const auto& [key, baseValue] : xvaData.baseCvaData()) {
            report.next();
            auto it = xvaData.fullRevalCva().find(key);
            auto scenarioValue = it == xvaData.fullRevalCva().end() ? 0.0 : it->second;
            report.add(key.tradeId)
                .add(key.nettingSet)
                .add(baseValue)
                .add(scenarioValue)
                .add(scenarioValue - baseValue);
            for (const auto& keyType : xvaData.keyTypes()) {
                report.add(aggData[key][keyType]);
            }
        }
    } catch (const std::exception& e) {
        ALOG("Error during xva explain report generation, got " << e.what());
    }
}

void XvaReportWriter::writePcaReport(const std::string& ccy, const Array& eigenValue, const Matrix& eigenVector,
                             const Size& principalComponent, ore::data::Report& reportOut){
    QL_REQUIRE((eigenValue.size() == eigenVector.columns() && eigenVector.rows() == eigenVector.columns()),
                "EigenVector and EigenValue size not match.");
    reportOut.addColumn("PrincipalComponent", Size())
        .addColumn("EigenValue", double(), 15);
    for (Size i = 0; i < eigenValue.size(); ++i) {
        reportOut.addColumn("EigenVector_" + std::to_string(i), double(), 15);
    }
    for (Size i = 0; i < principalComponent; ++i) {
        reportOut.next().add(i).add(eigenValue[i]);
        for (Size j = 0; j < eigenValue.size(); ++j) {
            reportOut.add(eigenVector[j][i]);
        }
    }
    reportOut.end();
}

void XvaReportWriter::writeMeanReversionReport(const Matrix& v, const Matrix& kappa, ore::data::Report& reportOut) {
    QL_REQUIRE(v.rows() == kappa.rows(), "v and kappa must have same rows.");
    reportOut.addColumn("PrincipalComponent", Size());
    for (Size i = 0; i < v.columns(); ++i) {
        reportOut.addColumn("v_" + std::to_string(i), double(), 15);
    }
    for (Size i = 0; i < kappa.columns(); ++i) {
        reportOut.addColumn("kappa_" + std::to_string(i), double(), 15);
    }
    for (Size i = 0; i < v.rows(); ++i) {
        reportOut.next().add(i);
        for (Size j = 0; j < v.columns(); ++j) {
            reportOut.add(v[i][j]);
        }
        for (Size j = 0; j < kappa.columns(); ++j) {
            reportOut.add(kappa[i][j]);
        }
    }
    reportOut.end();
}

} // namespace analytics
} // namespace ore
