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

#include <orea/app/reportwriters/simmreportwriter.hpp>
#include <orea/simm/crif.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <orea/simm/simmresults.hpp>
#include <orea/simm/utilities.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/math/comparison.hpp>
#include <stdio.h>

using ore::data::to_string;
using ore::data::Report;
using QuantLib::Date;
using std::string;
using std::vector;
using std::pair;
using QuantLib::Real;
using QuantLib::Size;
using QuantLib::close_enough;

namespace ore {
namespace analytics {

// Ease notation again
typedef CrifRecord::ProductClass ProductClass;
typedef SimmConfiguration::RiskClass RiskClass;
typedef SimmConfiguration::MarginType MarginType;
typedef SimmConfiguration::SimmSide SimmSide;

void SimmReportWriter::writeSIMMReport(
    const map<SimmSide, map<NettingSetDetails, pair<CrifRecord::Regulation, SimmResults>>>& finalSimmResultsMap,
    const QuantLib::ext::shared_ptr<Report> report, const bool hasNettingSetDetails, const string& simmResultCcy,
    const string& simmCalcCcyCall, const string& simmCalcCcyPost, const string& reportCcy, Real fxSpot,
    Real outputThreshold) {

    // Transform final SIMM results
    map<SimmSide, map<NettingSetDetails, map<set<CrifRecord::Regulation>, SimmResults>>> finalSimmResults;
    for (const auto& sv : finalSimmResultsMap) {
        const SimmSide& side = sv.first;
        for (const auto& nv : sv.second) {
            const NettingSetDetails& nettingSetDetails = nv.first;
            set<CrifRecord::Regulation> regulation({nv.second.first});
            const SimmResults& simmResults = nv.second.second;

            finalSimmResults[side][nettingSetDetails][regulation] = simmResults;
        }
    }

    writeSIMMReport(finalSimmResults, report, hasNettingSetDetails, simmResultCcy, simmCalcCcyCall, simmCalcCcyPost,
                    reportCcy, true, fxSpot, outputThreshold);
}

void SimmReportWriter::writeSIMMReport(
    const map<SimmSide, map<NettingSetDetails, map<set<CrifRecord::Regulation>, SimmResults>>>& simmResultsMap,
    const QuantLib::ext::shared_ptr<Report> report, const bool hasNettingSetDetails, const string& simmResultCcy,
    const string& simmCalcCcyCall, const string& simmCalcCcyPost, const string& reportCcy, const bool isFinalSimm,
    Real fxSpot, Real outputThreshold) {

    if (isFinalSimm) {
        LOG("Writing SIMM results report.");
    } else {
        LOG("Writing full SIMM results report.");
    }

    // netting set headers
    report->addColumn("Portfolio", string());
    if (hasNettingSetDetails) {
        for (const string& field : NettingSetDetails::optionalFieldNames())
            report->addColumn(field, string());
    }

    report->addColumn("ProductClass", string())
        .addColumn("RiskClass", string())
        .addColumn("MarginType", string())
        .addColumn("Bucket", string())
        .addColumn("SimmSide", string())
        .addColumn("Regulation", string())
        .addColumn("InitialMargin", double(), 9)
        .addColumn("Currency", string())
        .addColumn("CalculationCurrency", string());
    if (!reportCcy.empty()) {
        report->addColumn("InitialMargin(Report)", double(), 2).addColumn("ReportCurrency", string());
    }

    // Ensure that fxSpot is 1 if no reporting currency provided
    if (reportCcy.empty() && fxSpot != 1.0)
        fxSpot = 1.0;

    const vector<SimmSide> sides({SimmSide::Call, SimmSide::Post});
    for (const SimmSide side : sides) {
        const string& sideString = ore::data::to_string(side);

        // Variable to hold sum of initial margin over all portfolios
        Real sumSidePortfolios = 0.0;
        Real sumSidePortfoliosReporting = 0.0;

        set<CrifRecord::Regulation> winningRegs;
        if (simmResultsMap.find(side) != simmResultsMap.end()) {
            for (const auto& nv : simmResultsMap.at(side)) {
                const NettingSetDetails& portfolioId = nv.first;
                const auto& simmResultsMap = nv.second;

                if (isFinalSimm)
                    QL_REQUIRE(simmResultsMap.size() <= 1,
                               "Final SIMM results should only have one (winning) regulation per netting set.");

                for (const auto& rv : simmResultsMap) {
                    const set<CrifRecord::Regulation>& regulations = rv.first;
                    const SimmResults& results = rv.second;

                    if (isFinalSimm)
                        winningRegs.insert(regulations.begin(), regulations.end());

                    QL_REQUIRE(results.resultCurrency() == simmResultCcy,
                               "writeSIMMReport(): SIMM results ("
                                   << results.resultCurrency()
                                   << ") should be denominated in the SIMM result currency (" << simmResultCcy << ").");

                    // Loop over the results for this portfolio
                    for (const auto& result : results.data()) {
                        // Get the key values
                        const auto& key = result.first;
                        ProductClass pc = get<0>(key);
                        RiskClass rc = get<1>(key);
                        MarginType mt = get<2>(key);
                        string b = get<3>(key);
                        Real im = result.second;
                        Real simmReporting = 0.0;

                        // Write row if IM not negligible relative to outputThreshold.
                        if (fabs(im) >= outputThreshold ||
                            (pc == ProductClass::All && rc == RiskClass::All && mt == MarginType::All)) {
                            report->next();
                            map<string, string> nettingSetMap = portfolioId.mapRepresentation();
                            for (const string& field : NettingSetDetails::fieldNames(hasNettingSetDetails)) {
                                report->add(nettingSetMap[field]);
                            }
                            report->add(ore::data::to_string(pc))
                                .add(ore::data::to_string(rc))
                                .add(ore::data::to_string(mt))
                                .add(b)
                                .add(sideString)
                                .add(regulationsToString(regulations))
                                .add(result.second)
                                .add(results.resultCurrency())
                                .add(results.calculationCurrency());
                            if (!reportCcy.empty()) {
                                simmReporting = result.second * fxSpot;
                                report->add(simmReporting).add(reportCcy);
                            }
                            // Update aggregate portfolio IM value if necessary
                            // SimmResults should always contain an entry with this key - it is the portfolio IM
                            if (isFinalSimm &&
                                (pc == ProductClass::All && rc == RiskClass::All && mt == MarginType::All)) {
                                sumSidePortfolios += result.second;
                                sumSidePortfoliosReporting += simmReporting;
                            }
                        }
                    }
                }
            }
        }

        // Write out a row for the aggregate IM over all portfolios
        // We only write out this row if either reporting ccy was provided or if currency of all the results is the same
        if (isFinalSimm) {
            string finalWinningReg =
                winningRegs.size() > 1 || winningRegs.empty() ? "" : ore::data::to_string(*winningRegs.begin());

            // Write out common columns
            report->next();
            Size numNettingSetFields = NettingSetDetails::fieldNames(hasNettingSetDetails).size();
            for (Size t = 0; t < numNettingSetFields; t++)
                report->add("All");
            report->add("All")
                .add("All")
                .add("All")
                .add("All")
                .add(sideString)
                .add(finalWinningReg)
                .add(sumSidePortfolios)
                .add(simmResultCcy)
                .add(side == SimmSide::Call ? simmCalcCcyCall : simmCalcCcyPost);

            // Write out SIMM in reporting currency if we can
            if (!reportCcy.empty())
                report->add(sumSidePortfoliosReporting).add(reportCcy);
        }

        report->end();

        LOG("SIMM results report written.");
    }
}

void SimmReportWriter::writeSIMMData(const ore::analytics::Crif& simmData,
                                 const QuantLib::ext::shared_ptr<Report>& dataReport, const bool hasNettingSetDetails) {

    LOG("Writing SIMM data report.");

    // Add the headers to the report

    bool hasRegulations = false;
    for (auto scr = simmData.cbegin(); scr != simmData.cend(); scr++) {
        CrifRecord cr = scr->toCrifRecord();
        if (!cr.collectRegulations.empty() || !cr.postRegulations.empty()) {
            hasRegulations = true;
            break;
        }
    }

    // netting set headers
    dataReport->addColumn("Portfolio", string());
    if (hasNettingSetDetails) {
        for (const string& field : NettingSetDetails::optionalFieldNames())
            dataReport->addColumn(field, string());
    }

    dataReport->addColumn("RiskType", string())
        .addColumn("ProductClass", string())
        .addColumn("Bucket", string())
        .addColumn("Qualifier", string())
        .addColumn("Label1", string())
        .addColumn("Label2", string())
        .addColumn("AmountCurrency", string())
        .addColumn("Amount", double(), 2)
        .addColumn("AmountUSD", double(), 2);

    if (hasRegulations)
        dataReport->addColumn("collect_regulations", string()).addColumn("post_regulations", string());

    // Write the report body by looping over the netted CRIF records
    for (auto scr = simmData.cbegin(); scr != simmData.cend(); scr++) {
        CrifRecord cr = scr->toCrifRecord();

        // Skip to next netted CRIF record if 'AmountUSD' is negligible
        if (close_enough(cr.amountUsd, 0.0))
            continue;

        // Skip Schedule IM records
        if (cr.imModel == CrifRecord::IMModel::Schedule)
            continue;

        // Skip if the CRIF Record type is not SIMM
        if (cr.type() != CrifRecord::RecordType::SIMM)
            continue;

        // Same check as above, but for backwards compatibility, if im_model is not used
        // but Risk::Type is PV or Notional
        if (cr.imModel == CrifRecord::IMModel::Empty &&
            (cr.riskType == CrifRecord::RiskType::Notional || cr.riskType == CrifRecord::RiskType::PV))
            continue;

        // Write current netted CRIF record
        dataReport->next();
        map<string, string> nettingSetMap = cr.nettingSetDetails.mapRepresentation();
        for (const string& field : NettingSetDetails::fieldNames(hasNettingSetDetails))
            dataReport->add(nettingSetMap[field]);
        dataReport->add(ore::data::to_string(cr.riskType))
            .add(ore::data::to_string(cr.productClass))
            .add(cr.bucket)
            .add(cr.qualifier)
            .add(cr.label1)
            .add(cr.label2)
            .add(cr.amountCurrency)
            .add(cr.amount)
            .add(cr.amountUsd);

        if (hasRegulations) {
            string collectRegString = escapeCommaSeparatedList(regulationsToString(cr.collectRegulations), '\0');
            string postRegString = escapeCommaSeparatedList(regulationsToString(cr.postRegulations), '\0');

            dataReport->add(collectRegString).add(postRegString);
        }
    }

    dataReport->end();

    LOG("SIMM data report written.");
}

void SimmReportWriter::writeCrifReport(const QuantLib::ext::shared_ptr<Report>& report,
                                   const QuantLib::ext::shared_ptr<Crif>& crif) {

    if (crif) {
        // If we have SIMM parameters, check if at least one of them uses netting set details optional field/s
        // It is easier to check here than to pass the flag from other places, since otherwise we'd have to handle
        // certain edge cases e.g. SIMM parameters use optional NSDs, but trades don't. So SIMM report should not
        // display NSDs, but CRIF report still should.
        bool hasNettingSetDetails = false;
        for (const auto& scr : *crif) {
            CrifRecord cr = scr.toCrifRecord();

            if (!cr.nettingSetDetails.emptyOptionalFields())
                hasNettingSetDetails = true;
        }

        std::vector<string> addFields;
        bool hasCollectRegulations = false;
        bool hasPostRegulations = false;
        bool hasScheduleTrades = false;
        for (const auto& scr : *crif) {
            CrifRecord cr = scr.toCrifRecord();

            // Check which additional fields are being used/populated
            for (const auto& af : cr.additionalFields) {
                if (std::find(addFields.begin(), addFields.end(), af.first) == addFields.end()) {
                    addFields.push_back(af.first);
                }
            }

            // Check if regulations are being used
            if (!hasCollectRegulations)
                hasCollectRegulations = !cr.collectRegulations.empty();
            if (!hasPostRegulations)
                hasPostRegulations = !cr.postRegulations.empty();

            // Check if there are Schedule trades
            if (!hasScheduleTrades) {
                try {
                    hasScheduleTrades = cr.imModel == CrifRecord::IMModel::Schedule;
                } catch (std::exception&) {
                }
            }
        }

        // Add report headers

        report->addColumn("TradeID", string()).addColumn("PortfolioID", string());

        // Add additional netting set fields if netting set details are being used instead of just the netting set ID
        if (hasNettingSetDetails) {
            for (const string& optionalField : NettingSetDetails::optionalFieldNames())
                report->addColumn(optionalField, string());
        }

        report->addColumn("ProductClass", string())
            .addColumn("RiskType", string())
            .addColumn("Qualifier", string())
            .addColumn("Bucket", string())
            .addColumn("Label1", string())
            .addColumn("Label2", string())
            .addColumn("AmountCurrency", string())
            .addColumn("Amount", double(), 2)
            .addColumn("AmountUSD", double(), 2)
            .addColumn("IMModel", string())
            .addColumn("TradeType", string());

        if (hasScheduleTrades || crif->type() == Crif::CrifType::FRTB)
            report->addColumn("end_date", string());

        if (crif->type() == Crif::CrifType::FRTB) {
            report->addColumn("Label3", string())
                .addColumn("CreditQuality", string())
                .addColumn("LongShortInd", string())
                .addColumn("CoveredBondInd", string())
                .addColumn("TrancheThickness", string())
                .addColumn("BB_RW", string());
        }

        if (hasCollectRegulations)
            report->addColumn("collect_regulations", string());

        if (hasPostRegulations)
            report->addColumn("post_regulations", string());

        // Add additional CRIF fields
        for (const string& f : addFields) {
            report->addColumn(f, string());
        }

        // Write individual CRIF records
        for (const auto& scr : *crif) {
            CrifRecord cr = scr.toCrifRecord();

            report->next().add(cr.tradeId).add(cr.nettingSetDetails.nettingSetId());

            if (hasNettingSetDetails) {
                map<string, string> crNettingSetDetailsMap =
                    NettingSetDetails(cr.nettingSetDetails).mapRepresentation();
                for (const string& optionalField : NettingSetDetails::optionalFieldNames())
                    report->add(crNettingSetDetailsMap[optionalField]);
            }

            report->add(ore::data::to_string(cr.productClass))
                .add(ore::data::to_string(cr.riskType))
                .add(cr.qualifier)
                .add(cr.bucket)
                .add(cr.label1)
                .add(cr.label2)
                .add(cr.amountCurrency)
                .add(cr.amount)
                .add(cr.amountUsd)
                .add(ore::data::to_string(cr.imModel))
                .add(cr.tradeType);

            if (hasScheduleTrades || crif->type() == Crif::CrifType::FRTB)
                report->add(cr.endDate);

            if (crif->type() == Crif::CrifType::FRTB) {
                report->add(cr.label3)
                    .add(cr.creditQuality)
                    .add(cr.longShortInd)
                    .add(cr.coveredBondInd)
                    .add(cr.trancheThickness)
                    .add(cr.bb_rw);
            }

            if (hasCollectRegulations) {
                string regString = escapeCommaSeparatedList(regulationsToString(cr.collectRegulations), '\0');
                report->add(regString);
            }

            if (hasPostRegulations) {
                string regString = escapeCommaSeparatedList(regulationsToString(cr.postRegulations), '\0');
                report->add(regString);
            }

            for (const string& af : addFields) {
                if (cr.additionalFields.find(af) == cr.additionalFields.end())
                    report->add("");
                else
                    report->add(cr.getAdditionalFieldAsStr(af));
            }
        }
    }

    report->end();
}

} // namespace analytics
} // namespace ore
