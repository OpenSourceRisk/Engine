/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2017 Aareal Bank AG

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

#include <orea/app/reportwriter.hpp>
#include <orea/orea.hpp>
#include <ored/ored.hpp>
#include <ostream>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/errors.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <stdio.h>

using std::string;
using std::vector;
using QuantLib::Date;

namespace ore {
namespace analytics {

void ReportWriter::writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                            boost::shared_ptr<Market> market, const std::string& configuration,
                            boost::shared_ptr<Portfolio> portfolio) {
    LOG("portfolio valuation");
    DayCounter dc = ActualActual();
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
        .addColumn("Notional(Base)", double(), 2)
        .addColumn("NettingSet", string())
        .addColumn("CounterParty", string());
    for (auto trade : portfolio->trades()) {
        string npvCcy = trade->npvCurrency();
        Real fx = 1.0;
        if (npvCcy != baseCurrency)
            fx = market->fxSpot(npvCcy + baseCurrency, configuration)->value();
        try {
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
                .add(trade->notional() * fx)
                .add(trade->envelope().nettingSetId())
                .add(trade->envelope().counterparty());
        } catch (std::exception& e) {
            ALOG("Exception during pricing trade " << trade->id() << ": " << e.what());
            Date maturity = trade->maturity();
            report.next()
                .add(trade->id())
                .add(trade->tradeType())
                .add(maturity)
                .add(maturity == QuantLib::Null<Date>() ? Null<Real>() : dc.yearFraction(today, maturity))
                .add(Null<Real>())
                .add("#NA")
                .add(Null<Real>())
                .add("#NA")
                .add(Null<Real>())
                .add(Null<Real>())
                .add("#NA")
                .add("#NA");
        }
    }
    report.end();
    LOG("NPV file written");
}

void ReportWriter::writeCashflow(ore::data::Report& report, boost::shared_ptr<Portfolio> portfolio) {
    Date asof = Settings::instance().evaluationDate();
    LOG("Writing cashflow report for " << asof);
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
        .addColumn("fixingDate", Date())
        .addColumn("fixingValue", double(), 10)
        .addColumn("Notional", double(), 4);

    const vector<boost::shared_ptr<Trade>>& trades = portfolio->trades();

    for (Size k = 0; k < trades.size(); k++) {
        if (trades[k]->tradeType() == "Swaption" || trades[k]->tradeType() == "CapFloor") {
            WLOG("cashflow for " << trades[k]->tradeType() << " " << trades[k]->id() << " skipped");
            continue;
        }
        try {
            const vector<Leg>& legs = trades[k]->legs();
            for (size_t i = 0; i < legs.size(); i++) {
                const QuantLib::Leg& leg = legs[i];
                bool payer = trades[k]->legPayers()[i];
                string ccy = trades[k]->legCurrencies()[i];
                for (size_t j = 0; j < leg.size(); j++) {
                    boost::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
                    Date payDate = ptrFlow->date();
                    if (payDate >= asof) {
                        Real amount = ptrFlow->amount();
                        string flowType = "";
                        if (payer)
                            amount *= -1.0;
                        std::string ccy = trades[k]->legCurrencies()[i];
                        boost::shared_ptr<QuantLib::Coupon> ptrCoupon =
                            boost::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                        Real coupon;
                        Real accrual;
                        Real notional;
                        if (ptrCoupon) {
                            coupon = ptrCoupon->rate();
                            accrual = ptrCoupon->accrualPeriod();
                            notional = ptrCoupon->nominal();
                            flowType = "Interest";
                        } else {
                            coupon = Null<Real>();
                            accrual = Null<Real>();
                            notional = Null<Real>();
                            flowType = "Notional";
                        }
                        // This BMA part here (and below) is necessary because the fixingDay() method of
                        // AverageBMACoupon returns an exception rather than the last fixing day of the period.
                        boost::shared_ptr<AverageBMACoupon> ptrBMA =
                            boost::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(ptrFlow);
                        boost::shared_ptr<QuantLib::FloatingRateCoupon> ptrFloat =
                            boost::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(ptrFlow);
                        boost::shared_ptr<QuantLib::InflationCoupon> ptrInfl =
                            boost::dynamic_pointer_cast<QuantLib::InflationCoupon>(ptrFlow);
                        boost::shared_ptr<QuantLib::IndexedCashFlow> ptrIndCf =
                            boost::dynamic_pointer_cast<QuantLib::IndexedCashFlow>(ptrFlow);
                        boost::shared_ptr<QuantExt::FXLinkedCashFlow> ptrFxlCf =
                            boost::dynamic_pointer_cast<QuantExt::FXLinkedCashFlow>(ptrFlow);
                        Date fixingDate;
                        Real fixingValue;
                        if (ptrBMA) {
                            // We return the last fixing inside the coupon period
                            fixingDate = ptrBMA->fixingDates().end()[-2];
                            fixingValue = ptrBMA->pricer()->swapletRate();
                            if (ptrBMA->index()->pastFixing(fixingDate) == Null<Real>())
                                flowType = "BMAaverage";
                        } else if (ptrFloat) {
                            fixingDate = ptrFloat->fixingDate();
                            fixingValue = ptrFloat->index()->fixing(fixingDate);
                            if (ptrFloat->index()->pastFixing(fixingDate) == Null<Real>())
                                flowType = "InterestProjected";
                        } else if (ptrInfl) {
                            fixingDate = ptrInfl->fixingDate();
                            fixingValue = ptrInfl->index()->fixing(fixingDate);
                            flowType = "Inflation";
                        } else if (ptrIndCf) {
                            fixingDate = ptrIndCf->fixingDate();
                            fixingValue = ptrIndCf->index()->fixing(fixingDate);
                            flowType = "Index";
                        } else if (ptrFxlCf) {
                            fixingDate = ptrFxlCf->fxFixingDate();
                            fixingValue = ptrFxlCf->fxRate();
                        } else {
                            fixingDate = Null<Date>();
                            fixingValue = Null<Real>();
                        }
                        report.next()
                            .add(trades[k]->id())
                            .add(trades[k]->tradeType())
                            .add(j + 1)
                            .add(i)
                            .add(payDate)
                            .add(flowType)
                            .add(amount)
                            .add(ccy)
                            .add(coupon)
                            .add(accrual)
                            .add(fixingDate)
                            .add(fixingValue)
                            .add(notional);
                    }
                }
            }
        } catch (std::exception& e) {
            LOG("Exception writing cashflow report : " << e.what());
        } catch (...) {
            LOG("Exception writing cashflow report : Unkown Exception");
        }
    }
    report.end();
    LOG("Cashflow report written");
}

void ReportWriter::writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                               const TodaysMarketParameters& marketConfig, const boost::shared_ptr<Market>& market) {
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
        report.addColumn(it.first, double(), 15);
        yieldCurves.push_back(market->discountCurve(it.first, configID));
    }
    for (auto it : YieldCurves) {
        DLOG("yield curve - " << it.first);
        report.addColumn(it.first, double(), 15);
        yieldCurves.push_back(market->yieldCurve(it.first, configID));
    }
    for (auto it : indexCurves) {
        DLOG("index curve - " << it.first);
        report.addColumn(it.first, double(), 15);
        yieldCurves.push_back(market->iborIndex(it.first, configID)->forwardingTermStructure());
    }
    for (auto it : zeroInflationIndices) {
        DLOG("inflation curve - " << it.first);
        report.addColumn(it.first, double(), 15);
        zeroInflationFixings.push_back(market->zeroInflationIndex(it.first, configID));
    }
    for (auto it : defaultCurves) {
        DLOG("default curve - " << it.first);
        report.addColumn(it.first, double(), 15);
        probabilityCurves.push_back(market->defaultCurve(it.first, configID));
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

void ReportWriter::writeTradeExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                       const string& tradeId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    const vector<Real>& epe = postProcess->tradeEPE(tradeId);
    const vector<Real>& ene = postProcess->tradeENE(tradeId);
    const vector<Real>& ee_b = postProcess->tradeEE_B(tradeId);
    const vector<Real>& eee_b = postProcess->tradeEEE_B(tradeId);
    const vector<Real>& pfe = postProcess->tradePFE(tradeId);
    const vector<Real>& aepe = postProcess->allocatedTradeEPE(tradeId);
    const vector<Real>& aene = postProcess->allocatedTradeENE(tradeId);
    report.addColumn("TradeId", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double())
        .addColumn("ENE", double())
        .addColumn("AllocatedEPE", double())
        .addColumn("AllocatedENE", double())
        .addColumn("PFE", double())
        .addColumn("BaselEE", double())
        .addColumn("BaselEEE", double());

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
        .add(eee_b[0]);
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
            .add(eee_b[j + 1]);
    }
    report.end();
}

void ReportWriter::writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                            const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    const vector<Real>& epe = postProcess->netEPE(nettingSetId);
    const vector<Real>& ene = postProcess->netENE(nettingSetId);
    const vector<Real>& ee_b = postProcess->netEE_B(nettingSetId);
    const vector<Real>& eee_b = postProcess->netEEE_B(nettingSetId);
    const vector<Real>& pfe = postProcess->netPFE(nettingSetId);
    const vector<Real>& ecb = postProcess->expectedCollateral(nettingSetId);
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double(), 2)
        .addColumn("ENE", double(), 2)
        .addColumn("PFE", double(), 2)
        .addColumn("ExpectedCollateral", double(), 2)
        .addColumn("BaselEE", double(), 2)
        .addColumn("BaselEEE", double(), 2);

    report.next()
        .add(nettingSetId)
        .add(today)
        .add(0.0)
        .add(epe[0])
        .add(ene[0])
        .add(pfe[0])
        .add(ecb[0])
        .add(ee_b[0])
        .add(eee_b[0]);
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
            .add(eee_b[j + 1]);
    }
    report.end();
}

void ReportWriter::writeXVA(ore::data::Report& report, const string& allocationMethod,
                            boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<PostProcess> postProcess) {
    const vector<Date> dates = postProcess->cube()->dates();
    DayCounter dc = ActualActual();
    report.addColumn("TradeId", string())
        .addColumn("NettingSetId", string())
        .addColumn("CVA", double(), 2)
        .addColumn("DVA", double(), 2)
        .addColumn("FBA", double(), 2)
        .addColumn("FCA", double(), 2)
        .addColumn("COLVA", double(), 2)
        .addColumn("MVA", double(), 2)
        .addColumn("CollateralFloor", double(), 2)
        .addColumn("AllocatedCVA", double(), 2)
        .addColumn("AllocatedDVA", double(), 2)
        .addColumn("AllocationMethod", string())
        .addColumn("BaselEPE", double(), 2)
        .addColumn("BaselEEPE", double(), 2);

    for (auto n : postProcess->nettingSetIds()) {
        report.next()
            .add("")
            .add(n)
            .add(postProcess->nettingSetCVA(n))
            .add(postProcess->nettingSetDVA(n))
            .add(postProcess->nettingSetFBA(n))
            .add(postProcess->nettingSetFCA(n))
            .add(postProcess->nettingSetCOLVA(n))
            .add(postProcess->nettingSetMVA(n))
            .add(postProcess->nettingSetCollateralFloor(n))
            .add(postProcess->nettingSetCVA(n))
            .add(postProcess->nettingSetDVA(n))
            .add(allocationMethod)
            .add(postProcess->netEPE_B(n))
            .add(postProcess->netEEPE_B(n));

        for (Size k = 0; k < portfolio->trades().size(); ++k) {
            string tid = portfolio->trades()[k]->id();
            string nid = portfolio->trades()[k]->envelope().nettingSetId();
            if (nid != n)
                continue;
            report.next()
                .add(tid)
                .add(nid)
                .add(postProcess->tradeCVA(tid))
                .add(postProcess->tradeDVA(tid))
                .add(postProcess->tradeFBA(tid))
                .add(postProcess->tradeFCA(tid))
                .add(Null<Real>())
                .add(Null<Real>())
                .add(Null<Real>())
                .add(postProcess->allocatedTradeCVA(tid))
                .add(postProcess->allocatedTradeDVA(tid))
                .add(allocationMethod)
                .add(postProcess->tradeEPE_B(tid))
                .add(postProcess->tradeEEPE_B(tid));
        }
    }
    report.end();
}

void ReportWriter::writeNettingSetColva(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                        const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual();
    const vector<Real>& collateral = postProcess->expectedCollateral(nettingSetId);
    const vector<Real>& colvaInc = postProcess->colvaIncrements(nettingSetId);
    const vector<Real>& floorInc = postProcess->collateralFloorIncrements(nettingSetId);
    Real colva = postProcess->nettingSetCOLVA(nettingSetId);
    Real floorValue = postProcess->nettingSetCollateralFloor(nettingSetId);

    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 4)
        .addColumn("CollateralBalance", double(), 4)
        .addColumn("COLVA Increment", double(), 4)
        .addColumn("COLVA", double(), 4)
        .addColumn("CollateralFloor Increment", double(), 4)
        .addColumn("CollateralFloor", double(), 4);

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
    report.end();
}

void ReportWriter::writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data) {
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

} // namespace analytics
} // namespace ore
