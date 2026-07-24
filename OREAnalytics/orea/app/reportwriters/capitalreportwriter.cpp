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

#include <orea/app/reportwriters/capitalreportwriter.hpp>

#include <orea/engine/bacvacalculator.hpp>
#include <orea/engine/cvasensitivitycubestream.hpp>
#include <orea/engine/sacvasensitivityrecord.hpp>
#include <orea/engine/saccrtradedata.hpp>
#include <orea/simm/crif.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/utilities.hpp>

#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ostream>

using ore::data::to_string;
using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

void CapitalReportWriter::writeBaCvaReport(const QuantLib::ext::shared_ptr<BaCvaCalculator>& baCvaCalculator,
                                    ore::data::Report& reportOut) {

    reportOut.addColumn("Counterparty", string())
        .addColumn("NettingSet", string())
        .addColumn("Analytic", string())
        .addColumn("Value", double(), 6);

    std::map<std::string, std::set<std::string>> counterpartyNettingSets = baCvaCalculator->counterpartyNettingSets();

    reportOut.next().add("All").add("All").add("BA_CVA_CAPITAL").add(baCvaCalculator->cvaResult());

    for (auto it = counterpartyNettingSets.begin(); it != counterpartyNettingSets.end(); it++) {
        reportOut.next().add(it->first).add("All").add("sCVA").add(baCvaCalculator->counterpartySCVA(it->first));
        reportOut.next().add(it->first).add("All").add("RiskWeight").add(baCvaCalculator->riskWeight(it->first));
        for (auto n : it->second) {
            reportOut.next().add(it->first).add(n).add("EAD").add(baCvaCalculator->EAD(n));
            reportOut.next().add(it->first).add(n).add("EffMaturity").add(baCvaCalculator->effectiveMaturity(n));
            reportOut.next().add(it->first).add(n).add("DiscountFactor").add(baCvaCalculator->discountFactor(n));
        }
    }

    reportOut.end();
}

void CapitalReportWriter::writeCvaSensiReport(const QuantLib::ext::shared_ptr<CvaSensitivityCubeStream>& ss,
                                       ore::data::Report& reportOut) {

    LOG("Writing CVA Sensitivity Report");
    reportOut.addColumn("NettingSet", string())
        .addColumn("RiskFactor", string())
        .addColumn("ShiftType", string())
        .addColumn("ShiftSize", double(), 6)
        .addColumn("Currency", string())
        .addColumn("BaseCva", double(), 2)
        .addColumn("Delta", double(), 4);

    ss->reset();
    while (CvaSensitivityRecord sr = ss->next()) {
        reportOut.next();
        reportOut.add(sr.nettingSetId);
        reportOut.add(to_string(sr.key));
        reportOut.add(to_string(sr.shiftType));
        reportOut.add(sr.shiftSize);
        reportOut.add(sr.currency);
        reportOut.add(sr.baseCva);
        reportOut.add(sr.delta);
    }

    reportOut.end();
}

void CapitalReportWriter::writeCvaSensiReport(const std::vector<CvaSensitivityRecord>& records,
                                       ore::data::Report& reportOut) {

    LOG("Writing CVA Sensitivity Report");
    reportOut.addColumn("NettingSet", string())
        .addColumn("RiskFactor", string())
        .addColumn("ShiftType", string())
        .addColumn("ShiftSize", double(), 6)
        .addColumn("Currency", string())
        .addColumn("BaseCva", double(), 2)
        .addColumn("Delta", double(), 4);

    for (auto sr : records) {
        reportOut.next();
        reportOut.add(sr.nettingSetId);
        reportOut.add(to_string(sr.key));
        reportOut.add(to_string(sr.shiftType));
        reportOut.add(sr.shiftSize);
        reportOut.add(sr.currency);
        reportOut.add(sr.baseCva);
        reportOut.add(sr.delta);
    }

    reportOut.end();
}

void CapitalReportWriter::writeSaCvaSensiReport(const SaCvaNetSensitivities& sensis,
                                         ore::data::Report& reportOut) {

    LOG("Writing SA CVA Sensitivity Report");
    reportOut.addColumn("NettingSet", string())
        .addColumn("RiskType", string())
        .addColumn("CvaType", string())
        .addColumn("MarginType", string())
        .addColumn("RiskFactor", string())
        .addColumn("Bucket", string())
        .addColumn("Value", double(), 4);

    for (auto sr : sensis) {
        reportOut.next();
        reportOut.add(sr.nettingSetId);
        reportOut.add(to_string(sr.riskType));
        reportOut.add(to_string(sr.cvaType));
        reportOut.add(to_string(sr.marginType));
        reportOut.add(sr.riskFactor);
        reportOut.add(sr.bucket);
        reportOut.add(sr.value);
    }

    reportOut.end();
}

void CapitalReportWriter::writeSaccrTradeDetailReport(
    ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::analytics::SaccrTradeData>& tradeData) const {

    const auto& nettingSets = tradeData->nettingSets();
    bool hasNettingSetDetails = std::any_of(nettingSets.begin(), nettingSets.end(),
                                            [](const NettingSetDetails& nsd) { return !nsd.emptyOptionalFields(); });

    LOG("Writing SA-CCR trade detail report");
    report.addColumn("TradeId", string()).addColumn("TradeType", string()).addColumn("NettingSet", string());

    if (hasNettingSetDetails) {
        for (const auto& nettingSetField : NettingSetDetails::optionalFieldNames())
            report.addColumn(nettingSetField, string());
    }

    report.addColumn("NPV", Real(), 2)
        .addColumn("AssetClass", string())
        .addColumn("HedgingSet", string())
        .addColumn("HedgingSubset", string())
        .addColumn("Bucket", string())
        .addColumn("Qualifier", string())
        .addColumn("Currency", string())
        .addColumn("delta", Real(), 4)
        .addColumn("d", Real(), 4)
        .addColumn("MF", Real(), 7)
        .addColumn("M", Real(), 4)
        .addColumn("S", Real(), 4)
        .addColumn("E", Real(), 4)
        .addColumn("T", Real(), 4)
        .addColumn("SD", Real())
        .addColumn("CurrentPrice", Real(), 6)
        .addColumn("NumNominalFlows", Size())
        .addColumn("Price", Real(), 4)
        .addColumn("Strike", Real(), 4)
        .addColumn("Volatility", Real(), 4);

    for (const auto& [tid, timpl] : tradeData->data()) {
        for (const auto& c : timpl->getContributions()) {
            report.next().add(tid).add(timpl->trade()->tradeType());

            map<string, string> nettingSetMap = timpl->nettingSetDetails().mapRepresentation();
            for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails))
                report.add(nettingSetMap[fieldName]);

            string npvCcy = timpl->trade()->npvCurrency();
            Real npvFxRate = npvCcy.empty() ? Null<Real>() : tradeData->getFxRate(npvCcy + tradeData->baseCurrency());
            Real npvBase = timpl->NPV() * npvFxRate;

            report.add(npvBase)
                .add(ore::data::to_string(c.underlyingData.saccrAssetClass))
                .add(c.hedgingData.hedgingSet);

            Real adjNotional = c.adjustedNotional;
            Real notionalFxRate =
                c.currency.empty() ? Null<Real>() : tradeData->getFxRate(c.currency + tradeData->baseCurrency());

            report.add(c.hedgingData.hedgingSubset.value_or(""))
                .add(c.bucket)
                .add(c.underlyingData.qualifier)
                .add(tradeData->baseCurrency())
                .add(c.delta)
                .add(adjNotional * notionalFxRate)
                .add(c.maturityFactor)
                .add(c.maturity)
                .add(c.startDate.value_or(Null<Real>()))
                .add(c.endDate.value_or(Null<Real>()))
                .add(c.lastExerciseDate.value_or(Null<Real>()))
                .add(timpl->getSupervisoryDuration(c.underlyingData.saccrAssetClass, c.startDate, c.endDate)
                         .value_or(Null<Real>()));
            Real currentPrice = c.currentPrice.value_or(Null<Real>());
            if (currentPrice != Null<Real>())
                currentPrice *= notionalFxRate;
            report.add(currentPrice)
                .add(timpl->getNominalFlowCount().value_or(Null<Size>()))
                .add(c.optionDeltaPrice.value_or(Null<Real>()))
                .add(c.strike.value_or(Null<Real>()))
                .add(timpl->getSupervisoryOptionVolatility(c.underlyingData));
        }
    }
    report.end();
    LOG("Finished writing SA-CCR trade detail report");
}

void CapitalReportWriter::writeCapitalCrifReport(ore::data::Report& report,
                                              const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif,
                                              const std::string& baseCurrency, const char& csvQuoteChar) const {
    bool hasNettingSetDetails = crif ? crif->hasNettingSetDetails() : false;

    // Add report headers
    report.addColumn("TradeID", string())
        .addColumn("PortfolioID", string())
        .addColumn("CounterpartyName", string())
        .addColumn("CounterpartyID", string())
        .addColumn("NettingSetNumber", string())
        .addColumn("RiskType", string())
        .addColumn("HedgingSet", string())
        .addColumn("Qualifier", string())
        .addColumn("Bucket", string())
        .addColumn("Label1", string())
        .addColumn("Label2", string())
        .addColumn("AmountCurrency", string())
        .addColumn("Amount", double(), 2)
        .addColumn("AmountUSD", double(), 2)
        .addColumn("ValuationDate", Date())
        .addColumn("EndDate", double(), 3)
        .addColumn("Label3", double(), 5)
        .addColumn("Regulation", string())
        .addColumn("Model", string())
        .addColumn("TradeType", string());
    if (hasNettingSetDetails) {
        for (const string& optionalField : NettingSetDetails::optionalFieldNames())
            report.addColumn(optionalField, string());
    }

    // Write CRIF records
    if (crif) {
        for (const auto& srecord : *crif) {
            auto crifRecord = srecord.toCrifRecord();
            DLOG("Writing CRIF record to report: " << crifRecord);

            report.next()
                .add(crifRecord.tradeId)
                .add(crifRecord.nettingSetDetails.nettingSetId())
                .add(crifRecord.counterpartyName)
                .add(crifRecord.counterpartyId)
                .add(crifRecord.nettingSetNumber)
                .add(ore::data::to_string(crifRecord.riskType))
                .add(crifRecord.hedgingSet)
                .add(crifRecord.qualifier)
                .add(crifRecord.bucket);

            string labelStr;
            // Label 1 (SA-CCR)
            if (crifRecord.saccrLabel1.which() == 0) {
                Real value = boost::get<Real>(crifRecord.saccrLabel1);
                labelStr = value == Null<Real>() ? nullString_ : ore::data::to_string(value);
            } else if (crifRecord.saccrLabel1.which() == 1) {
                labelStr = boost::get<string>(crifRecord.saccrLabel1);
            } else if (crifRecord.saccrLabel1.which() == 2) {
                Size value = boost::get<Size>(crifRecord.saccrLabel1);
                labelStr = value == Null<Size>() ? nullString_ : ore::data::to_string(value);
            } else {
                QL_FAIL("Unexpected type (" << crifRecord.saccrLabel1.which() << ") for CrifRecord SA-CCR label1");
            }
            report.add(labelStr);

            // Label 2 (SA-CCR)
            if (crifRecord.saccrLabel2.which() == 0) {
                Real value = boost::get<Real>(crifRecord.saccrLabel2);
                labelStr = value == Null<Real>() ? nullString_ : ore::data::to_string(value);
            } else if (crifRecord.saccrLabel2.which() == 1) {
                labelStr = boost::get<string>(crifRecord.saccrLabel2);
            } else {
                QL_FAIL("Unexpected type (" << crifRecord.saccrLabel2.which() << ") for CrifRecord SA-CCR label2");
            }
            report.add(labelStr);

            report.add(crifRecord.amountCurrency)
                .add(crifRecord.amount)
                .add(crifRecord.amountUsd)
                .add(crifRecord.valuationDate)
                .add(crifRecord.saccrEndDate)
                .add(crifRecord.saccrLabel3)
                .add(ore::data::to_string(crifRecord.regulation))
                .add(ore::data::to_string(crifRecord.capitalModel))
                .add(crifRecord.tradeType);

            if (hasNettingSetDetails) {
                map<string, string> crNettingSetDetailsMap =
                    NettingSetDetails(crifRecord.nettingSetDetails).mapRepresentation();
                for (const string& optionalField : NettingSetDetails::optionalFieldNames())
                    report.add(crNettingSetDetailsMap[optionalField]);
            }
        }
    }

    report.end();
}

} // namespace analytics
} // namespace ore
