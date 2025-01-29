/*
  Copyright (C) 2025 Quaternion Risk Management Ltd
  All rights reserved.
*/

/*! \file orepcapital/orea/saccrcrifgenerator.hpp
    \brief Class that generates a SA-CCR CRIF report
 */

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/engine/saccrcrifgenerator.hpp>
#include <orea/engine/saccrtradedata.hpp>
#include <orea/simm/crif.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace analytics {

using ore::analytics::Crif;
using ore::analytics::CrifRecord;
using ore::data::CollateralBalance;
using ore::data::CollateralBalances;
using ore::data::CSA;
using ore::data::StructuredConfigurationErrorMessage;
using ore::data::StructuredConfigurationWarningMessage;
using ore::data::StructuredTradeErrorMessage;
using QuantLib::Null;
using QuantLib::Real;
using QuantLib::Size;
using std::string;
using std::vector;
using AssetClass = SaccrTradeData::AssetClass;
using RiskType = CrifRecord::RiskType;

QuantLib::ext::shared_ptr<Crif> SaccrCrifGenerator::generateCrif() const {

    auto results = QuantLib::ext::make_shared<Crif>();
    Size failedTrades = 0;
    vector<CrifRecord> crifRecords;
    Size processedRecords = 0;

    // Add trade-specific CRIF records, i.e. notionals and PVs
    for (const auto& [tid, td] : tradeData_->data()) {
        try {
            // Produce SA-CCR CRIF records for trade
            auto records = generateTradeCrifRecords(td);

            for (const auto& record : records)
                results->addRecord(record, false, false);

            processedRecords += records.size();
        } catch (const std::exception& e) {
            ore::analytics::StructuredAnalyticsErrorMessage(
                "SA-CCR CRIF Generation", "Failed to generate CRIF records for trade", e.what(),
                {{"tradeId", tid}, {"tradeType", td->trade()->tradeType()}});
            failedTrades++;
        }
    }

    // Add collateral CRIF records
    // Categories:
    // MPOR - Label1
    // MTA - Amount
    // TA - Amount
    // DIRECTION? Label1 = Mutual, OneWayIn, OneWayOut
    // SETTLEMENTTYPE? Label1 = STM, NOM, OTH(?)
    // IM
    // VM
    //
    // Handle CounterpartyManager later in SaccrCalculator instead?
    //
    // Netting set manager - handle independent amount later

    // Collateral balances

    // CRIF records from netting set manager
    const auto& nettingSetManager = tradeData_->nettingSetManager();
    if (nettingSetManager && !nettingSetManager->empty()) {
        for (const NettingSetDetails& nsd : tradeData_->nettingSets()) {
            if (!nettingSetManager->has(nsd)) {
                StructuredConfigurationErrorMessage("Netting set definintions", ore::data::to_string(nsd),
                                                    "Capital CRIF Generation", "Netting set definition not found")
                    .log();
                continue;
            }

            CrifRecord baseRecord("", "", nsd, tradeData_->counterparty(nsd), CrifRecord::CapitalModel::SACCR,
                                  CrifRecord::SaccrRegulation::Basel, QuantLib::Settings::instance().evaluationDate());
            baseRecord.riskType = CrifRecord::RiskType::COLL;

            auto ndef = nettingSetManager->get(nsd);
            if (ndef->activeCsaFlag()) {
                const string& ccy = ndef->csaDetails()->csaCurrency();

                CrifRecord settlementTypeRecord = baseRecord;
                settlementTypeRecord.hedgingSet = "SettlementType";
                settlementTypeRecord.saccrLabel1 = "STM";
                results->addRecord(settlementTypeRecord);

                CrifRecord directionRecord = baseRecord;
                directionRecord.hedgingSet = "Direction";
                auto imType = ndef->csaDetails()->initialMarginType();
                if (imType == CSA::Type::Bilateral)
                    directionRecord.saccrLabel1 = "Mutual";
                else if (imType == CSA::Type::CallOnly)
                    directionRecord.saccrLabel1 = "OneWayIn";
                else if (imType == CSA::Type::PostOnly)
                    directionRecord.saccrLabel1 = "OneWayOut";
                else
                    QL_FAIL("Invalid CSA::Type '" << ore::data::to_string(imType) << "'");
                directionRecord.saccrLabel2 = "";
                results->addRecord(directionRecord);

                CrifRecord mporRecord = baseRecord;
                mporRecord.hedgingSet = "MPOR";
                QL_REQUIRE(ndef->csaDetails()->marginPeriodOfRisk().units() == QuantLib::Weeks,
                           "MPOR is expected in weeks");
                Size MPORinWeeks = (Size)QuantLib::weeks(ndef->csaDetails()->marginPeriodOfRisk());
                mporRecord.saccrLabel1 = MPORinWeeks * 5;
                results->addRecord(mporRecord);

                Real fxRateCSA = tradeData_->getFxRate(ccy + "USD");

                CrifRecord mtaRecord = baseRecord;
                mtaRecord.hedgingSet = "MTA";
                mtaRecord.amount = ndef->csaDetails()->mtaRcv();
                mtaRecord.amountCurrency = ccy;
                mtaRecord.amountUsd = mtaRecord.amount * fxRateCSA;
                results->addRecord(mtaRecord);

                CrifRecord taRecord = baseRecord;
                taRecord.hedgingSet = "TA";
                taRecord.amount = ndef->csaDetails()->thresholdRcv();
                taRecord.amountCurrency = ccy;
                taRecord.amountUsd = taRecord.amount * fxRateCSA;
                results->addRecord(taRecord);

                // FIXME/TODO: Not sure if this is right - the docs don't specify how to add IA to CRIF,
                // but given its similarity to IM, we add it there.
                CrifRecord iaRecord = baseRecord;
                iaRecord.hedgingSet = "IA";
                iaRecord.amount = ndef->csaDetails()->independentAmountHeld();
                iaRecord.amountCurrency = ccy;
                iaRecord.amountUsd = iaRecord.amount * fxRateCSA;
                results->addRecord(iaRecord);
            } else {
                CrifRecord settlementTypeRecord = baseRecord;
                settlementTypeRecord.hedgingSet = "SettlementType";
                settlementTypeRecord.saccrLabel1 = "NOM";
                results->addRecord(settlementTypeRecord);
            }
        }
    }

    // CRIF records from collateral balances
    // IM and VM
    for (const auto& nsd : tradeData_->nettingSets()) {
        // Records for initial margin
        auto ndef = tradeData_->nettingSetManager()->get(nsd);
        QuantLib::ext::shared_ptr<CollateralBalance> cb = nullptr;
        QuantLib::ext::shared_ptr<CollateralBalance> ccb = nullptr;

        if (tradeData_->collateralBalances()->has(nsd))
            cb = tradeData_->collateralBalances()->get(nsd);
        const bool hasCcb = tradeData_->calculatedCollateralBalances()->has(nsd);
        if (hasCcb)
            ccb = tradeData_->calculatedCollateralBalances()->get(nsd);
        Real initialMargin = Null<Real>();
        string imCurrency = tradeData_->baseCurrency();
        if (ndef->activeCsaFlag()) {
            if (ndef->csaDetails()->calculateIMAmount()) {
                // InitialMargin = SIMM-generated IM, unless an overriding balance was provided, in which case we use
                // the balance provided.
                if (cb && cb->initialMargin() != Null<Real>() &&
                    tradeData_->defaultIMBalances().find(nsd) == tradeData_->defaultIMBalances().end()) {
                    initialMargin = cb->initialMargin();
                    imCurrency = cb->currency();
                } else if (hasCcb) {
                    initialMargin = ccb->initialMargin();
                    imCurrency = ccb->currency();
                } else {
                    initialMargin = 0.0;
                }
            } else {
                // If no balance was provided, and calculateIMAmount=false, the calculation should fail
                if (!cb || cb->initialMargin() == Null<Real>()) {
                    auto msg = StructuredConfigurationErrorMessage(
                        "Collateral balances", ore::data::to_string(nsd), "Inconsistent netting set configurations",
                        "CalculateIMAmount was set to \'false\' in the netting set "
                        "definition, but no InitialMargin was "
                        "provided in the collateral balance.");
                    msg.log();
                    QL_FAIL(msg.msg());
                } else {
                    initialMargin = cb->initialMargin();
                    imCurrency = cb->currency();
                }
            }
        } else {
            // If netting set is uncollateralised
            initialMargin = 0.0;
        }

        CrifRecord baseRecord("", "", nsd, tradeData_->counterparty(nsd), CrifRecord::CapitalModel::SACCR,
                              CrifRecord::SaccrRegulation::Basel, QuantLib::Settings::instance().evaluationDate());
        baseRecord.riskType = CrifRecord::RiskType::COLL;

        CrifRecord imRecord = baseRecord;
        imRecord.hedgingSet = "IM";
        imRecord.saccrLabel2 = "Cash";
        imRecord.amount = initialMargin;
        imRecord.amountCurrency = imCurrency;
        imRecord.amountUsd = initialMargin * tradeData_->getFxRate(imCurrency + "USD");
        results->addRecord(imRecord);

        // Records for variation margin
        Real variationMargin = Null<Real>();
        string vmCurrency = tradeData_->baseCurrency();
        if (ndef->activeCsaFlag()) {
            if (ndef->csaDetails()->calculateVMAmount()) {
                // VariationMargin = NPV, unless an overriding balance was provided, in which case we use the balance
                // provided.
                if (cb && cb->variationMargin() != Null<Real>() &&
                    tradeData_->defaultVMBalances().find(nsd) == tradeData_->defaultVMBalances().end()) {
                    variationMargin = cb->variationMargin();
                    vmCurrency = cb->currency();
                } else {
                    variationMargin = tradeData_->NPV(nsd);
                }
            } else {
                // If no balance was provided, even though calculateVMAmount=false, then the calculation should fail
                if (!cb || cb->variationMargin() == Null<Real>()) {
                    auto msg = StructuredConfigurationErrorMessage(
                        "Collateral balances", ore::data::to_string(nsd), "Inconsistent netting set configurations",
                        "CalculateVMAmount was set to \'false\' in the netting set definition, but no VariationMargin "
                        "was provided in the collateral balance.");
                    msg.log();
                    QL_FAIL(msg.msg());
                } else {
                    variationMargin = cb->variationMargin();
                    vmCurrency = cb->currency();
                }
            }
        } else {
            variationMargin = 0.0;
        }

        CrifRecord vmRecord = baseRecord;
        vmRecord.hedgingSet = "VM";
        vmRecord.saccrLabel2 = "Cash";
        vmRecord.amount = variationMargin;
        vmRecord.amountCurrency = vmCurrency;
        vmRecord.amountUsd = variationMargin * tradeData_->getFxRate(vmCurrency + "USD");
        results->addRecord(vmRecord);
    }

    LOG("Processed SA-CCR trade data: " << processedRecords << " CRIF records produced, "
                                        << tradeData_->size() - failedTrades << " trades succeeded, " << failedTrades
                                        << " trades failed");
    return results;
}

vector<CrifRecord> SaccrCrifGenerator::generateTradeCrifRecords(
    const QuantLib::ext::shared_ptr<SaccrTradeData::Impl>& tradeDataImpl) const {
    vector<CrifRecord> records;

    auto createSaccrCrifRecord = [&]() {
        return CrifRecord(tradeDataImpl->trade()->id(), tradeDataImpl->trade()->tradeType(),
                          tradeDataImpl->trade()->envelope().nettingSetDetails(),
                          tradeDataImpl->trade()->envelope().counterparty(), CrifRecord::CapitalModel::SACCR,
                          CrifRecord::SaccrRegulation::Basel, QuantLib::Settings::instance().evaluationDate());
    };

    try {
        // Effective notional
        auto record = createSaccrCrifRecord();

        // Counterparty name and netting set number?

        // Trade-related risk types
        auto contributions = tradeDataImpl->getContributions();
        for (const auto& c : contributions) {
            // Risk type
            switch (c.underlyingData.saccrAssetClass) {
            case AssetClass::FX:
                record.riskType = RiskType::FX;
                break;
            case AssetClass::IR:
                record.riskType = RiskType::IR;
                break;
            case AssetClass::Commodity:
                record.riskType = RiskType::CO;
                break;
            case AssetClass::Credit:
                if (c.underlyingData.isIndex)
                    record.riskType = RiskType::CR_IX;
                else
                    record.riskType = RiskType::CR_SN;
                break;
            case AssetClass::Equity:
                if (c.underlyingData.isIndex)
                    record.riskType = RiskType::EQ_IX;
                else
                    record.riskType = RiskType::EQ_SN;
                break;
            default:
                QL_FAIL("Invalid SA-CCR asset class " << ore::data::to_string(c.underlyingData.saccrAssetClass));
            }

            // Hedging set/subset, qualifier
            record.hedgingSet = c.hedgingData.hedgingSet;
            // I don't think this is needed. Qualifier = HedgingSubset for all assetClasses != Commodity
            // and for assetClass = Commodity, bucket = HedgingSubset
            // record.hedgingSubset = c.hedgingData.hedgingSubset.get_value_or("");
            record.qualifier = c.underlyingData.qualifier;

            // Dates
            record.saccrEndDate = c.maturity;
            if (c.startDate.has_value())
                record.saccrLabel1 = c.startDate.get();
            if (c.endDate.has_value())
                record.saccrLabel2 = c.endDate.get();

            record.bucket = c.bucket;

            record.saccrLabel3 = c.delta;

            record.amount = c.adjustedNotional * c.delta * c.maturityFactor;
            record.amountCurrency = c.currency;
            record.amountUsd = record.amount * tradeDataImpl->getFxRate(c.currency + "USD");
            records.push_back(record);
        }

        // PV
        auto pvRecord = createSaccrCrifRecord();
        pvRecord.saccrLabel1 = Null<Real>();
        pvRecord.saccrLabel2 = Null<Real>();
        pvRecord.saccrEndDate = Null<Real>();
        pvRecord.riskType = RiskType::PV;
        pvRecord.amount = tradeDataImpl->NPV();
        pvRecord.amountCurrency = tradeDataImpl->trade()->npvCurrency();
        pvRecord.amountUsd = pvRecord.amount * tradeDataImpl->getFxRate(pvRecord.amountCurrency + "USD");
        records.push_back(pvRecord);
    } catch (const std::exception& e) {
        ore::data::StructuredTradeErrorMessage(tradeDataImpl->trade(), "Failed to generate SA-CCR trade data CRIF records", e.what())
            .log();
    }

    return records;
}

} // namespace analytics
} // namespace ore
