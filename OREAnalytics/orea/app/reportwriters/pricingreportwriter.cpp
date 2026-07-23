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

#include <orea/app/reportwriters/pricingreportwriter.hpp>

#include <orea/cube/sensitivitycube.hpp>
#include <orea/engine/sensitivitystream.hpp>

#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/cashflowutils.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>

#include <boost/lexical_cast.hpp>

#include <ostream>
#include <stdio.h>

using ore::data::to_string;
using ore::data::StructuredTradeErrorMessage;
using ore::data::DateGrid;
using ore::data::Market;
using ore::data::TodaysMarketParameters;
using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

void PricingReportWriter::writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                            QuantLib::ext::shared_ptr<Market> market, const std::string& configuration,
                            QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio) {
    LOG("portfolio valuation");
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Date today = Settings::instance().evaluationDate();
    report.addColumn("TradeId", string())
        .addColumn("TradeType", string())
        .addColumn("Maturity", Date())
        .addColumn("MaturityTime", double(), 6)
        .addColumn("NPV", double(), 6)
        .addColumn("NpvCurrency", string())
        .addColumn("NPV(Base)", double(), 6)
        .addColumn("BaseCurrency", string())
        .addColumn("Notional", double(), 2)
        .addColumn("NotionalCurrency", string())
        .addColumn("Notional(Base)", double(), 2)
        .addColumn("NettingSet", string())
        .addColumn("CounterParty", string());
    for (auto & [tradeId, trade] : portfolio->trades()) {
        try {
            string npvCcy = trade->npvCurrency();
            Real fx = 1.0, fxNotional = 1.0;
            if (npvCcy != baseCurrency)
                fx = market->fxRate(npvCcy + baseCurrency, configuration)->value();
            if (trade->notionalCurrency() != "" && trade->notionalCurrency() != baseCurrency)
                fxNotional = market->fxRate(trade->notionalCurrency() + baseCurrency, configuration)->value();
            Real npv = trade->instrument()->NPV();
            QL_REQUIRE(std::isfinite(npv), "npv is not finite (" << npv << ")");
            Date maturity = trade->maturity();
            report.next()
                .add(trade->id())
                .add(trade->tradeType())
                .add(maturity)
                .add(maturity == QuantLib::Null<Date>() ? Null<Real>() : dc.yearFraction(today, maturity))
                .add(npv)
                .add(npvCcy)
                .add(npv * fx)
                .add(baseCurrency)
                .add(trade->notional())
                .add(trade->notionalCurrency() == "" ? nullString_ : trade->notionalCurrency())
                .add(trade->notional() == Null<Real>() || trade->notionalCurrency() == ""
                         ? Null<Real>()
                         : trade->notional() * fxNotional)
                .add(trade->envelope().nettingSetId())
                .add(trade->envelope().counterparty());
        } catch (std::exception& e) {
            StructuredTradeErrorMessage(trade->id(), trade->tradeType(), "Error during trade pricing", e.what()).log();
            Date maturity = trade->maturity();
            report.next()
                .add(trade->id())
                .add(trade->tradeType())
                .add(maturity)
                .add(maturity == QuantLib::Null<Date>() ? Null<Real>() : dc.yearFraction(today, maturity))
                .add(Null<Real>())
                .add(nullString_)
                .add(Null<Real>())
                .add(nullString_)
                .add(Null<Real>())
                .add(nullString_)
                .add(Null<Real>())
                .add(nullString_)
                .add(nullString_);
        }
    }
    report.end();
    LOG("NPV file written");
}

namespace {

void addCashflowReportColumns(ore::data::Report& report) {
    report.addColumn("TradeId", string())
        .addColumn("Type", string())
        .addColumn("CashflowNo", Size())
        .addColumn("LegNo", Size())
        .addColumn("PayDate", Date())
        .addColumn("FlowType", string())
        .addColumn("Amount", double(), 4)
        .addColumn("Currency", string())
        .addColumn("Coupon", double(), 10)
        .addColumn("Accrual", double(), 10)
        .addColumn("AccrualStartDate", Date(), 4)
        .addColumn("AccrualEndDate", Date(), 4)
        .addColumn("AccruedAmount", double(), 4)
        .addColumn("fixingDate", Date())
        .addColumn("fixingValue", double(), 10)
        .addColumn("Notional", double(), 4)
        .addColumn("DiscountFactor", double(), 10)
        .addColumn("PresentValue", double(), 10)
        .addColumn("FXRate(Local-Base)", double(), 10)
        .addColumn("PresentValue(Base)", double(), 10)
        .addColumn("BaseCurrency", string())
        .addColumn("FloorStrike", double(), 10)
        .addColumn("CapStrike", double(), 10)
        .addColumn("FloorVolatility", double(), 10)
        .addColumn("CapVolatility", double(), 10)
        .addColumn("EffectiveFloorVolatility", double(), 10)
        .addColumn("EffectiveCapVolatility", double(), 10)
        .addColumn("Amount(Base)", double(), 4)
        .addColumn("DiscountFactor(Base)", double(), 10);
}

void addTradeCashflowRows(ore::data::Report& report, const ext::shared_ptr<ore::data::Trade>& trade,
                          const std::vector<ore::data::TradeCashflowReportData>& data) {
    for (auto const& d : data) {
        report.next()
            .add(trade->id())
            .add(trade->tradeType())
            .add(d.cashflowNo)
            .add(d.legNo)
            .add(d.payDate)
            .add(d.flowType)
            .add(d.amount)
            .add(d.currency)
            .add(d.coupon)
            .add(d.accrual)
            .add(d.accrualStartDate)
            .add(d.accrualEndDate)
            .add(d.accruedAmount)
            .add(d.fixingDate)
            .add(d.fixingValue)
            .add(d.notional)
            .add(d.discountFactor)
            .add(d.presentValue)
            .add(d.fxRateLocalBase)
            .add(d.presentValueBase)
            .add(d.baseCurrency)
            .add(d.floorStrike)
            .add(d.capStrike)
            .add(d.floorVolatility)
            .add(d.capVolatility)
            .add(d.effectiveFloorVolatility)
            .add(d.effectiveCapVolatility)
            .add(d.baseAmount)
            .add(d.discountFactorBase);
    }
}

} // namespace

void PricingReportWriter::writeCashflow(ore::data::Report& report, const std::string& baseCurrency,
                                 QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
                                 QuantLib::ext::shared_ptr<ore::data::Market> market, const std::string& configuration,
                                 const bool includePastCashflows) {

    addCashflowReportColumns(report);

    for (auto [tradeId, trade] : portfolio->trades()) {
        try {
            auto data = trade->cashflows(baseCurrency, market, configuration, includePastCashflows);
            addTradeCashflowRows(report, trade, data);
        } catch (std::exception& e) {
            StructuredTradeErrorMessage(trade->id(), trade->tradeType(), "Error during cashflow report generation",
                                        e.what())
                .log();
        }
    }

    report.end();
    LOG("Cashflow report written");
}

void PricingReportWriter::writeCashflow(
    ore::data::Report& report, QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
    const std::map<std::string, std::vector<ore::data::TradeCashflowReportData>>& tradeCashflows) {

    LOG("Writing cashflow report from precomputed cashflows");

    addCashflowReportColumns(report);

    for (auto [tradeId, trade] : portfolio->trades()) {
        auto it = tradeCashflows.find(tradeId);
        if (it == tradeCashflows.end()){
            WLOG("Trade " << tradeId << " not found in precomputed cashflows, skipping.");
            continue;
        }
        addTradeCashflowRows(report, trade, it->second);
    }

    report.end();
    LOG("Cashflow report written");
}


void PricingReportWriter::writeCashflowNpv(ore::data::Report& report, const ore::data::InMemoryReport& cashflowReport,
                                    QuantLib::ext::shared_ptr<Market> market, const std::string& configuration,
                                    const std::string& baseCcy, const QuantLib::Date& horizon) {
    // Pick the following fields form the in memory report:
    // - tradeId 
    // - payment date 
    // - currency 
    // - present value 
    // Then convert PVs into base currency, aggrate per trade if payment date is within the horizon
    // Write the resulting aggregate PV per trade into the report.

    Size tradeIdColumn = 0;
    Size tradeTypeColumn = 1;
    Size payDateColumn = 4;
    Size ccyColumn = 7;
    Size pvColumn = 17;
    QL_REQUIRE(cashflowReport.header(tradeIdColumn) == "TradeId", "incorrect trade id column " << tradeIdColumn);
    QL_REQUIRE(cashflowReport.header(tradeTypeColumn) == "Type", "incorrect trade type column " << tradeTypeColumn);
    QL_REQUIRE(cashflowReport.header(payDateColumn) == "PayDate", "incorrect payment date column " << payDateColumn);
    QL_REQUIRE(cashflowReport.header(ccyColumn) == "Currency", "incorrect currency column " << ccyColumn);
    QL_REQUIRE(cashflowReport.header(pvColumn) == "PresentValue", "incorrect pv column " << pvColumn);
    
    map<string, Real> npvMap;
    Date asof = Settings::instance().evaluationDate();
    for (Size i = 0; i < cashflowReport.rows(); ++i) {
        string tradeId = boost::get<string>(cashflowReport.data(tradeIdColumn, i));
        string tradeType = boost::get<string>(cashflowReport.data(tradeTypeColumn, i));
        Date payDate = boost::get<Date>(cashflowReport.data(payDateColumn, i));
        string ccy = boost::get<string>(cashflowReport.data(ccyColumn, i));
        Real pv = boost::get<Real>(cashflowReport.data(pvColumn, i));
        Real fx = 1.0;
    // There shouldn't be entries in the cf report without ccy. We assume ccy = baseCcy in this case and log an error.
        if (ccy.empty()) {
            StructuredTradeErrorMessage(tradeId, tradeType, "Error during CashflowNpv calculation.",
                                        "Cashflow in row " + std::to_string(i) +
                                            " has no ccy. Assuming ccy = baseCcy = " + baseCcy + ".")
                .log();
        }
        if (!ccy.empty() && ccy != baseCcy)
            fx = market->fxRate(ccy + baseCcy, configuration)->value();
        if (npvMap.find(tradeId) == npvMap.end())
            npvMap[tradeId] = 0.0;
        if (payDate > asof && payDate <= horizon) {
            npvMap[tradeId] += pv * fx;
            DLOG("Cashflow NPV for trade " << tradeId << ": pv " << pv << " fx " << fx << " sum " << npvMap[tradeId]);
        }   
    }   

    LOG("Writing cashflow NPV report for " << asof);
    report.addColumn("TradeId", string())
        .addColumn("PresentValue", double(), 10)
        .addColumn("BaseCurrency", string())
        .addColumn("Horizon", string());        

    for (auto r: npvMap)
        report.next().add(r.first).add(r.second).add(baseCcy).add(horizon < Date::maxDate() ? ore::data::to_string(horizon) : "infinite");

    report.end();
    LOG("Cashflow NPV report written");
}

void PricingReportWriter::writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                               const TodaysMarketParameters& marketConfig, const QuantLib::ext::shared_ptr<Market>& market,
                               const bool continueOnError) {
    LOG("Write curves... ");

    QL_REQUIRE(marketConfig.hasConfiguration(configID), "curve configuration " << configID << " not found");

    map<string, string> discountCurves = marketConfig.mapping(MarketObject::DiscountCurve, configID);
    map<string, string> YieldCurves = marketConfig.mapping(MarketObject::YieldCurve, configID);
    map<string, string> indexCurves = marketConfig.mapping(MarketObject::IndexCurve, configID);
    map<string, string> zeroInflationIndices, defaultCurves;
    if (marketConfig.hasMarketObject(MarketObject::ZeroInflationCurve))
        zeroInflationIndices = marketConfig.mapping(MarketObject::ZeroInflationCurve, configID);
    if (marketConfig.hasMarketObject(MarketObject::DefaultCurve))
        defaultCurves = marketConfig.mapping(MarketObject::DefaultCurve, configID);

    vector<Handle<YieldTermStructure>> yieldCurves;
    vector<Handle<ZeroInflationIndex>> zeroInflationFixings;
    vector<Handle<DefaultProbabilityTermStructure>> probabilityCurves;

    report.addColumn("Tenor", Period()).addColumn("Date", Date());

    for (auto it : discountCurves) {
        DLOG("discount curve - " << it.first);
        try {
            yieldCurves.push_back(market->discountCurve(it.first, configID));
            report.addColumn(it.first, double(), 15);
        } catch (const std::exception& e) {
            if (continueOnError) {
                WLOG("skip this curve: " << e.what());
            } else {
                QL_FAIL(e.what());
            }
        }
    }
    for (auto it : YieldCurves) {
        DLOG("yield curve - " << it.first);
        try {
            yieldCurves.push_back(market->yieldCurve(it.first, configID));
            report.addColumn(it.first, double(), 15);
        } catch (const std::exception& e) {
            if (continueOnError) {
                WLOG("skip this curve: " << e.what());
            } else {
                QL_FAIL(e.what());
            }
        }
    }
    for (auto it : indexCurves) {
        DLOG("index curve - " << it.first);
        try {
            yieldCurves.push_back(market->iborIndex(it.first, configID)->forwardingTermStructure());
            report.addColumn(it.first, double(), 15);
        } catch (const std::exception& e) {
            if (continueOnError) {
                WLOG("skip this curve: " << e.what());
            } else {
                QL_FAIL(e.what());
            }
        }
    }
    for (auto it : zeroInflationIndices) {
        DLOG("inflation curve - " << it.first);
        try {
            zeroInflationFixings.push_back(market->zeroInflationIndex(it.first, configID));
            report.addColumn(it.first, double(), 15);
        } catch (const std::exception& e) {
            if (continueOnError) {
                WLOG("skip this curve: " << e.what());
            } else {
                QL_FAIL(e.what());
            }
        }
    }
    for (auto it : defaultCurves) {
        DLOG("default curve - " << it.first);
        try {
            probabilityCurves.push_back(market->defaultCurve(it.first, configID)->curve());
            report.addColumn(it.first, double(), 15);
        } catch (const std::exception& e) {
            if (continueOnError) {
                WLOG("skip this curve: " << e.what());
            } else {
                QL_FAIL(e.what());
            }
        }
    }

    for (Size j = 0; j < grid.size(); ++j) {
        Date date = grid[j];
        report.next().add(grid.tenors()[j]).add(date);
        for (Size i = 0; i < yieldCurves.size(); ++i)
            report.add(yieldCurves[i]->discount(date));
        for (Size i = 0; i < zeroInflationFixings.size(); ++i)
            report.add(zeroInflationFixings[i]->fixing(date));
        for (Size i = 0; i < probabilityCurves.size(); ++i)
            report.add(probabilityCurves[i]->survivalProbability(date));
    }
    report.end();
}

void PricingReportWriter::writeSensitivityReport(Report& report, const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                                          Real outputThreshold, const QuantLib::ext::shared_ptr<Market>& market,
                                          const std::string& configuration, Size outputPrecision) {

    LOG("Writing Sensitivity report");

    Size shiftSizePrecision = outputPrecision < 6 ? 6 : outputPrecision;
    Size amountPrecision = outputPrecision < 2 ? 2 : outputPrecision;

    report.addColumn("TradeId", string());
    report.addColumn("IsPar", string());
    report.addColumn("Factor_1", string());
    report.addColumn("ShiftSize_1", double(), shiftSizePrecision);
    report.addColumn("Factor_2", string());
    report.addColumn("ShiftSize_2", double(), shiftSizePrecision);
    report.addColumn("Currency", string());
    report.addColumn("Base NPV", double(), amountPrecision);
    report.addColumn("Delta", double(), amountPrecision);
    report.addColumn("Gamma", double(), amountPrecision);
    report.addColumn("Currency(Trade)", string());
    report.addColumn("Base NPV(Trade)", double(), amountPrecision);
    report.addColumn("Delta(Trade)", double(), amountPrecision);
    report.addColumn("Gamma(Trade)", double(), amountPrecision);

    // Make sure that we are starting from the start
    ss->reset();
    while (SensitivityRecord sr = ss->next()) {
        if ((outputThreshold == Null<Real>()) ||
            (fabs(sr.delta) > outputThreshold || (sr.gamma != Null<Real>() && fabs(sr.gamma) > outputThreshold))) {

            Real fx = 1.0;
            std::string tradeCcy;
            if (market && !sr.tradeCurrency.empty()) {
                tradeCcy = sr.tradeCurrency;
                if (sr.tradeCurrency != sr.currency)
                    fx = market->fxRate(sr.currency + sr.tradeCurrency, configuration)->value();
            } else
                tradeCcy = sr.currency;

            report.next();
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
            report.add(tradeCcy);
            report.add(sr.baseNpv * fx);
            report.add(sr.delta * fx);
            if (sr.gamma == Null<Real>()) {
                report.add(sr.gamma);
            } else {
                report.add(sr.gamma * fx);
            }
        } else if (!std::isfinite(sr.delta) || !std::isfinite(sr.gamma)) {
            // TODO: Again, is this needed?
            ALOG("sensitivity record has infinite values: " << sr);
        }
    }

    report.end();
    LOG("Sensitivity report finished");
}

void PricingReportWriter::writeSensitivityConfigReport(ore::data::Report& report,
                                                const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes,
                                                const std::map<RiskFactorKey, QuantLib::Real>& baseValues,
                                                const std::map<RiskFactorKey, std::string>& keyToFactor) {
    LOG("Writing Sensitivity Config report");

    report.addColumn("Key", string())
        .addColumn("Factor", string())
        .addColumn("BaseValue", double(), 8)
        .addColumn("ShiftSize", double(), 8);

    for (auto const& [key, shift] : shiftSizes) {
        report.next();
        std::string keyStr = "na", factorStr = "na";
        Real baseValue = Null<Real>();
        keyStr = ore::data::to_string(key);
        if (auto it = keyToFactor.find(key); it != keyToFactor.end())
            factorStr = it->second;
        if (auto it = baseValues.find(key); it != baseValues.end())
            baseValue = it->second;
        report.add(keyStr).add(factorStr).add(baseValue).add(shift);
    }

    report.end();
    LOG("Sensitivity Config report finished.");
}

} // namespace analytics
} // namespace ore
