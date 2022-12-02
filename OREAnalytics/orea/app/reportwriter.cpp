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

// FIXME: including all is slow and bad
#include <boost/lexical_cast.hpp>
#include <orea/orea.hpp>
#include <ored/ored.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ostream>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <ql/errors.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/currencies/currencycomparator.hpp>
#include <qle/instruments/cashflowresults.hpp>
#include <stdio.h>

using ore::data::to_string;
using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

typedef std::map<Currency, Matrix, CurrencyComparator> result_type_matrix;
typedef std::map<Currency, std::vector<Real>, CurrencyComparator> result_type_vector;
typedef std::map<Currency, Real, CurrencyComparator> result_type_scalar;

void ReportWriter::writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                            boost::shared_ptr<Market> market, const std::string& configuration,
                            boost::shared_ptr<Portfolio> portfolio) {
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
    for (auto trade : portfolio->trades()) {
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
            ALOG(StructuredTradeErrorMessage(trade->id(), trade->tradeType(), "Error during trade pricing", e.what()));
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

void ReportWriter::writeCashflow(ore::data::Report& report, boost::shared_ptr<ore::data::Portfolio> portfolio,
                                 boost::shared_ptr<ore::data::Market> market, const std::string& configuration,
                                 const bool includePastCashflows) {

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
        .addColumn("AccrualStartDate", Date(), 4)
        .addColumn("AccrualEndDate", Date(), 4)
        .addColumn("AccruedAmount", double(), 4)
        .addColumn("fixingDate", Date())
        .addColumn("fixingValue", double(), 10)
        .addColumn("Notional", double(), 4)
        .addColumn("DiscountFactor", double(), 10)
        .addColumn("PresentValue", double(), 10)
        .addColumn("FloorStrike", double(), 6)
        .addColumn("CapStrike", double(), 6)
        .addColumn("FloorVolatility", double(), 6)
        .addColumn("CapVolatility", double(), 6);

    const vector<boost::shared_ptr<Trade>>& trades = portfolio->trades();

    for (Size k = 0; k < trades.size(); k++) {

        // if trade is marked as not having cashflows, we skip it

        if (!trades[k]->hasCashflows()) {
            WLOG("cashflow for " << trades[k]->tradeType() << " " << trades[k]->id() << " skipped");
            continue;
        }

        // if trade provides cashflows as additional results, we use that information instead of the legs

        bool useAdditionalResults = false;
        try {
            useAdditionalResults = trades[k]->instrument()->additionalResults().find("cashFlowResults") !=
                                   trades[k]->instrument()->additionalResults().end();
        } catch (...) {
        }

        try {

            const Real multiplier = trades[k]->instrument()->multiplier();

            if (!useAdditionalResults) {

                // leg based cashflow reporting

                const vector<Leg>& legs = trades[k]->legs();
                for (size_t i = 0; i < legs.size(); i++) {
                    const QuantLib::Leg& leg = legs[i];
                    bool payer = trades[k]->legPayers()[i];
                    string ccy = trades[k]->legCurrencies()[i];
                    Handle<YieldTermStructure> discountCurve;
                    if (market)
                        discountCurve = market->discountCurve(ccy, configuration);
                    for (size_t j = 0; j < leg.size(); j++) {
                        boost::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
                        Date payDate = ptrFlow->date();
                        if (!ptrFlow->hasOccurred(asof) || includePastCashflows) {
                            Real amount = ptrFlow->amount();
                            string flowType = "";
                            if (payer)
                                amount *= -1.0;
                            std::string ccy = trades[k]->legCurrencies()[i];
                            boost::shared_ptr<QuantLib::Coupon> ptrCoupon =
                                boost::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                            boost::shared_ptr<QuantExt::CommodityIndexedCashFlow> ptrCommCf =
                                boost::dynamic_pointer_cast<QuantExt::CommodityIndexedCashFlow>(ptrFlow);
                            Real coupon;
                            Real accrual;
                            Real notional;
                            Date accrualStartDate, accrualEndDate;
                            Real accruedAmount;
                            if (ptrCoupon) {
                                coupon = ptrCoupon->rate();
                                accrual = ptrCoupon->accrualPeriod();
                                notional = ptrCoupon->nominal();
                                accrualStartDate = ptrCoupon->accrualStartDate();
                                accrualEndDate = ptrCoupon->accrualEndDate();
                                accruedAmount = ptrCoupon->accruedAmount(asof);
                                if (payer)
                                    accruedAmount *= -1.0;
                                flowType = "Interest";
                            } else if (ptrCommCf) {
                                coupon = Null<Real>();
                                accrual = Null<Real>();
                                notional = ptrCommCf->quantity(); // this is measured in units, e.g. barrels for oil
                                accrualStartDate = accrualEndDate = Null<Date>();
                                accruedAmount = Null<Real>();
                                flowType = "Notional (units)";
                            } else {
                                coupon = Null<Real>();
                                accrual = Null<Real>();
                                notional = Null<Real>();
                                accrualStartDate = accrualEndDate = Null<Date>();
                                accruedAmount = Null<Real>();
                                flowType = "Notional";
                            }
                            // This BMA part here (and below) is necessary because the fixingDay() method of
                            // AverageBMACoupon returns an exception rather than the last fixing day of the period.

                            boost::shared_ptr<QuantLib::Coupon> cpn =
                                boost::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                            if (cpn) {
                                ptrFlow = unpackIndexedCoupon(cpn);
                            }
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
                            boost::shared_ptr<QuantExt::EquityCoupon> ptrEqCp =
                                boost::dynamic_pointer_cast<QuantExt::EquityCoupon>(ptrFlow);
                            Date fixingDate;
                            Real fixingValue;
                            if (ptrBMA) {
                                // We return the last fixing inside the coupon period
                                fixingDate = ptrBMA->fixingDates().end()[-2];
                                fixingValue = ptrBMA->pricer()->swapletRate();
                                if (fixingDate > asof)
                                    flowType = "BMAaverage";
                            } else if (ptrFloat) {
                                fixingDate = ptrFloat->fixingDate();
                                fixingValue = ptrFloat->index()->fixing(fixingDate);
                                if (fixingDate > asof)
                                    flowType = "InterestProjected";
                                // for ON coupons the fixing value is the compounded / averaged rate, not the last
                                // single ON fixing
                                if (auto on = boost::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(ptrFloat)) {
                                    fixingValue = (on->rate() - on->spread()) / on->gearing();
                                } else if (auto on = boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(
                                               ptrFloat)) {
                                    fixingValue = (on->rate() - on->effectiveSpread()) / on->gearing();
                                } else if (auto c = boost::dynamic_pointer_cast<
                                               QuantExt::CappedFlooredAverageONIndexedCoupon>(ptrFloat)) {
                                    fixingValue = (c->underlying()->rate() - c->underlying()->spread()) /
                                                  c->underlying()->gearing();
                                } else if (auto c = boost::dynamic_pointer_cast<
                                               QuantExt::CappedFlooredOvernightIndexedCoupon>(ptrFloat)) {
                                    fixingValue = (c->underlying()->rate() - c->underlying()->effectiveSpread()) /
                                                  c->underlying()->gearing();
                                }
                                // similar treatment of sub period coupons
                                if (auto sp = boost::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(ptrFloat)) {
                                    fixingValue = (sp->rate() - sp->spread()) / sp->gearing();
                                }
                            } else if (ptrInfl) {
                                fixingDate = ptrInfl->fixingDate();
                                fixingValue = ptrInfl->indexFixing();
                                flowType = "Inflation";
                            } else if (ptrIndCf) {
                                fixingDate = ptrIndCf->fixingDate();
                                fixingValue = ptrIndCf->indexFixing();
                                flowType = "Index";
                            } else if (ptrFxlCf) {
                                fixingDate = ptrFxlCf->fxFixingDate();
                                fixingValue = ptrFxlCf->fxRate();
                            } else if (ptrEqCp) {
                                fixingDate = ptrEqCp->fixingEndDate();
                                fixingValue = ptrEqCp->equityCurve()->fixing(fixingDate);
                            } else if (ptrCommCf) {
                                fixingDate = ptrCommCf->date();
                                fixingValue = ptrCommCf->index()->fixing(ptrCommCf->pricingDate());
                            } else {
                                fixingDate = Null<Date>();
                                fixingValue = Null<Real>();
                            }

                            Real effectiveAmount = Null<Real>();
                            Real discountFactor = Null<Real>();
                            Real presentValue = Null<Real>();
                            Real floorStrike = Null<Real>();
                            Real capStrike = Null<Real>();
                            Real floorVolatility = Null<Real>();
                            Real capVolatility = Null<Real>();

                            if (amount != Null<Real>())
                                effectiveAmount = amount * multiplier;

                            if (market) {
                                discountFactor = ptrFlow->hasOccurred(asof) ? 0.0 : discountCurve->discount(payDate);
				if(effectiveAmount != Null<Real>())
				    presentValue = discountFactor * effectiveAmount;

                                // scan for known capped / floored coupons and extract cap / floor strike and fixing
                                // date

                                // unpack stripped cap/floor coupon
                                boost::shared_ptr<CashFlow> c = ptrFlow;
                                if (auto tmp = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(ptrFlow)) {
                                    c = tmp->underlying();
                                }
                                Date volFixingDate;
                                std::string qlIndexName; // index used to retrieve vol
                                bool usesCapVol = false, usesSwaptionVol = false;
                                Period swaptionTenor;
                                if (auto tmp = boost::dynamic_pointer_cast<CappedFlooredCoupon>(c)) {
                                    floorStrike = tmp->effectiveFloor();
                                    capStrike = tmp->effectiveCap();
                                    volFixingDate = tmp->fixingDate();
                                    qlIndexName = tmp->index()->name();
                                    if (auto cms = boost::dynamic_pointer_cast<CmsCoupon>(tmp->underlying())) {
                                        swaptionTenor = cms->swapIndex()->tenor();
                                        qlIndexName = cms->swapIndex()->iborIndex()->name();
                                        usesSwaptionVol = true;
                                    } else if (auto ibor = boost::dynamic_pointer_cast<IborCoupon>(tmp->underlying())) {
                                        qlIndexName = ibor->index()->name();
                                        usesCapVol = true;
                                    }
                                } else if (auto tmp =
                                               boost::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c)) {
                                    floorStrike = tmp->effectiveFloor();
                                    capStrike = tmp->effectiveCap();
                                    volFixingDate = tmp->underlying()->fixingDates().front();
                                    qlIndexName = tmp->index()->name();
                                    usesCapVol = true;
                                } else if (auto tmp =
                                               boost::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(c)) {
                                    floorStrike = tmp->effectiveFloor();
                                    capStrike = tmp->effectiveCap();
                                    volFixingDate = tmp->underlying()->fixingDates().front();
                                    qlIndexName = tmp->index()->name();
                                    usesCapVol = true;
                                }

                                // get market volaility for cap / floor

                                if (volFixingDate != Date() && fixingDate > market->asofDate()) {
                                    volFixingDate = std::max(volFixingDate, market->asofDate() + 1);
                                    if (floorStrike != Null<Real>()) {
                                        if (usesSwaptionVol) {
                                            floorVolatility =
                                                market
                                                    ->swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, swaptionTenor, floorStrike);
                                        } else if (usesCapVol) {
                                            floorVolatility =
                                                market
                                                    ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, floorStrike);
                                        }
                                    }
                                    if (capStrike != Null<Real>()) {
                                        if (usesSwaptionVol) {
                                            capVolatility =
                                                market
                                                    ->swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, swaptionTenor, capStrike);
                                        } else if (usesCapVol) {
                                            capVolatility =
                                                market
                                                    ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, capStrike);
                                        }
                                    }
                                }
                            }

                            report.next()
                                .add(trades[k]->id())
                                .add(trades[k]->tradeType())
                                .add(j + 1)
                                .add(i)
                                .add(payDate)
                                .add(flowType)
                                .add(effectiveAmount)
                                .add(ccy)
                                .add(coupon)
                                .add(accrual)
                                .add(accrualStartDate)
                                .add(accrualEndDate)
                                .add(accruedAmount * (accruedAmount == Null<Real>() ? 1.0 : multiplier))
                                .add(fixingDate)
                                .add(fixingValue)
                                .add(notional * (notional == Null<Real>() ? 1.0 : multiplier))
                                .add(discountFactor)
                                .add(presentValue)
                                .add(floorStrike)
                                .add(capStrike)
                                .add(floorVolatility)
                                .add(capVolatility);
                        }
                    }
                }

            } else {

                // additional result based cashflow reporting

                auto tmp = trades[k]->instrument()->additionalResults().find("cashFlowResults");
                QL_REQUIRE(
                    tmp != trades[k]->instrument()->additionalResults().end(),
                    "internal error: expected cashFlowResults in additional results when writing cashflow report");
                QL_REQUIRE(tmp->second.type() == typeid(std::vector<CashFlowResults>),
                           "cashflowResults type not handlded");
                std::vector<CashFlowResults> cfResults = boost::any_cast<std::vector<CashFlowResults>>(tmp->second);
                std::map<Size, Size> cashflowNumber;
                for (auto const& cf : cfResults) {
                    string ccy = "";
                    if (!cf.currency.empty()) {
                        ccy = cf.currency;
                    } else if (trades[k]->legCurrencies().size() > cf.legNumber) {
                        ccy = trades[k]->legCurrencies()[cf.legNumber];
                    } else {
                        ccy = trades[k]->npvCurrency();
                    }

                    Real effectiveAmount = Null<Real>();
                    Real discountFactor = Null<Real>();
                    Real presentValue = Null<Real>();
                    Real floorStrike = Null<Real>();
                    Real capStrike = Null<Real>();
                    Real floorVolatility = Null<Real>();
                    Real capVolatility = Null<Real>();

                    if (cf.amount != Null<Real>())
                        effectiveAmount = cf.amount * multiplier;
                    if (cf.discountFactor != Null<Real>())
                        discountFactor = cf.discountFactor;
                    else if (!cf.currency.empty() && cf.payDate != Null<Date>() && market) {
                        discountFactor =
                            cf.payDate < asof ? 0.0 : market->discountCurve(cf.currency)->discount(cf.payDate);
                    }
                    if (cf.presentValue != Null<Real>()) {
                        presentValue = cf.presentValue * multiplier;
                    } else if (effectiveAmount != Null<Real>() && discountFactor != Null<Real>()) {
                        presentValue = effectiveAmount * discountFactor;
                    }
                    if (cf.floorStrike != Null<Real>())
                        floorStrike = cf.floorStrike;
                    if (cf.capStrike != Null<Real>())
                        capStrike = cf.capStrike;
                    if (cf.floorVolatility != Null<Real>())
                        floorVolatility = cf.floorVolatility;
                    if (cf.capVolatility != Null<Real>())
                        capVolatility = cf.capVolatility;

                    report.next()
                        .add(trades[k]->id())
                        .add(trades[k]->tradeType())
                        .add(++cashflowNumber[cf.legNumber])
                        .add(cf.legNumber)
                        .add(cf.payDate)
                        .add(cf.type)
                        .add(effectiveAmount)
                        .add(ccy)
                        .add(cf.rate)
                        .add(cf.accrualPeriod)
                        .add(cf.accrualStartDate)
                        .add(cf.accrualEndDate)
                        .add(cf.accruedAmount * (cf.accruedAmount == Null<Real>() ? 1.0 : multiplier))
                        .add(cf.fixingDate)
                        .add(cf.fixingValue)
                        .add(cf.notional * (cf.notional == Null<Real>() ? 1.0 : multiplier))
                        .add(discountFactor)
                        .add(presentValue)
                        .add(floorStrike)
                        .add(capStrike)
                        .add(floorVolatility)
                        .add(capVolatility);
                }
            }

        } catch (std::exception& e) {
            ALOG("Exception writing cashflow report : " << e.what());
        } catch (...) {
            ALOG("Exception writing cashflow report : Unknown Exception");
        }
    }
    report.end();
    LOG("Cashflow report written");
}

void ReportWriter::writeCashflowNpv(ore::data::Report& report,
                                    const ore::data::InMemoryReport& cashflowReport,
                                    boost::shared_ptr<ore::data::Market> market,
                                    const std::string& configuration,
                                    const std::string& baseCcy,
                                    const Date& horizon)  {
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
        string tradeId = boost::get<string>(cashflowReport.data(tradeIdColumn).at(i));
        string tradeType = boost::get<string>(cashflowReport.data(tradeTypeColumn).at(i));
        Date payDate = boost::get<Date>(cashflowReport.data(payDateColumn).at(i));
        string ccy = boost::get<string>(cashflowReport.data(ccyColumn).at(i));
        Real pv = boost::get<Real>(cashflowReport.data(pvColumn).at(i));
        Real fx = 1.0;
	// There shouldn't be entries in the cf report without ccy. We assume ccy = baseCcy in this case and log an error.
        if (ccy.empty()) {
            ALOG(StructuredTradeErrorMessage(tradeId, tradeType, "Error during CashflowNpv calculation.",
                                             "Cashflow in row " + std::to_string(i) +
                                                 " has no ccy. Assuming ccy = baseCcy = " + baseCcy + "."));
        }
        if (!ccy.empty() && ccy != baseCcy)
            fx = market->fxSpot(ccy + baseCcy, configuration)->value();
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
        report.next().add(r.first).add(r.second).add(baseCcy).add(horizon < Date::maxDate() ? to_string(horizon) : "infinite");

    report.end();
    LOG("Cashflow NPV report written");
}

void ReportWriter::writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                               const TodaysMarketParameters& marketConfig, const boost::shared_ptr<Market>& market,
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

void ReportWriter::writeTradeExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
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

void addNettingSetExposure(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
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
}

void ReportWriter::writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                            const string& nettingSetId) {
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double(), 2)
        .addColumn("ENE", double(), 2)
        .addColumn("PFE", double(), 2)
        .addColumn("ExpectedCollateral", double(), 2)
        .addColumn("BaselEE", double(), 2)
        .addColumn("BaselEEE", double(), 2);
    addNettingSetExposure(report, postProcess, nettingSetId);
    report.end();
}

void ReportWriter::writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess) {
    report.addColumn("NettingSet", string())
        .addColumn("Date", Date())
        .addColumn("Time", double(), 6)
        .addColumn("EPE", double(), 2)
        .addColumn("ENE", double(), 2)
        .addColumn("PFE", double(), 2)
        .addColumn("ExpectedCollateral", double(), 2)
        .addColumn("BaselEE", double(), 2)
        .addColumn("BaselEEE", double(), 2);

    for (auto n : postProcess->nettingSetIds()) {
        addNettingSetExposure(report, postProcess, n);
    }
    report.end();
}

void ReportWriter::writeNettingSetCvaSensitivities(ore::data::Report& report,
                                                   boost::shared_ptr<PostProcess> postProcess,
                                                   const string& nettingSetId) {
    const vector<Real> grid = postProcess->spreadSensitivityTimes();
    const vector<Real>& sensiHazardRate = postProcess->netCvaHazardRateSensitivity(nettingSetId);
    const vector<Real>& sensiCdsSpread = postProcess->netCvaSpreadSensitivity(nettingSetId);
    report.addColumn("NettingSet", string())
        .addColumn("Time", double(), 6)
        .addColumn("CvaHazardRateSensitivity", double(), 6)
        .addColumn("CvaSpreadSensitivity", double(), 6);

    if (sensiHazardRate.size() == 0 || sensiCdsSpread.size() == 0)
        return;

    for (Size j = 0; j < grid.size(); ++j) {
        report.next().add(nettingSetId).add(grid[j]).add(sensiHazardRate[j]).add(sensiCdsSpread[j]);
    }
    report.end();
}

void ReportWriter::writeXVA(ore::data::Report& report, const string& allocationMethod,
                            boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<PostProcess> postProcess) {
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

    for (auto n : postProcess->nettingSetIds()) {
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
        }
    }
    report.end();
}

void ReportWriter::writeNettingSetColva(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                        const string& nettingSetId) {
    const vector<Date> dates = postProcess->cube()->dates();
    Date today = Settings::instance().evaluationDate();
    DayCounter dc = ActualActual(ActualActual::ISDA);
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

void ReportWriter::writeScenarioReport(ore::data::Report& report,
                                       const boost::shared_ptr<SensitivityCube>& sensitivityCube,
                                       Real outputThreshold) {

    LOG("Writing Scenario report");

    report.addColumn("TradeId", string());
    report.addColumn("Factor", string());
    report.addColumn("Up/Down", string());
    report.addColumn("Base NPV", double(), 2);
    report.addColumn("Scenario NPV", double(), 2);
    report.addColumn("Difference", double(), 2);

    auto scenarioDescriptions = sensitivityCube->scenarioDescriptions();
    auto tradeIds = sensitivityCube->tradeIds();
    auto npvCube = sensitivityCube->npvCube();

    for (Size i = 0; i < tradeIds.size(); i++) {
        Real baseNpv = npvCube->getT0(i);
        auto tradeId = tradeIds[i];

        for (Size j = 0; j < scenarioDescriptions.size(); j++) {
            auto scenarioDescription = scenarioDescriptions[j];

            Real scenarioNpv = npvCube->get(i, j);
            Real difference = scenarioNpv - baseNpv;

            if (fabs(difference) > outputThreshold) {
                report.next();
                report.add(tradeId);
                report.add(prettyPrintInternalCurveName(scenarioDescription.factors()));
                report.add(scenarioDescription.typeString());
                report.add(baseNpv);
                report.add(scenarioNpv);
                report.add(difference);
            } else if (!std::isfinite(difference)) {
                // TODO: is this needed?
                ALOG("sensitivity scenario for trade " << tradeId << ", factor " << scenarioDescription.factors()
                                                       << " is not finite (" << difference << ")");
            }
        }
    }

    report.end();
    LOG("Scenario report finished");
}

void ReportWriter::writeSensitivityReport(Report& report, const boost::shared_ptr<SensitivityStream>& ss,
                                          Real outputThreshold, Size outputPrecision) {

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

    // Make sure that we are starting from the start
    ss->reset();
    while (SensitivityRecord sr = ss->next()) {
        if (fabs(sr.delta) > outputThreshold || (sr.gamma != Null<Real>() && fabs(sr.gamma) > outputThreshold)) {
            report.next();
            report.add(sr.tradeId);
            report.add(to_string(sr.isPar));
            report.add(prettyPrintInternalCurveName(reconstructFactor(sr.key_1, sr.desc_1)));
            report.add(sr.shift_1);
            report.add(prettyPrintInternalCurveName(reconstructFactor(sr.key_2, sr.desc_2)));
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

template <class T>
void addMapResults(boost::any resultMap, const std::string& tradeId, const std::string& resultName, Report& report) {
    T map = boost::any_cast<T>(resultMap);
    for (auto it : map) {
        std::string name = resultName + "_" + it.first.code();
        boost::any tmp = it.second;
        auto p = parseBoostAny(tmp);
        report.next().add(tradeId).add(name).add(p.first).add(p.second);
    }
}

void ReportWriter::writeAdditionalResultsReport(Report& report, boost::shared_ptr<Portfolio> portfolio,
                                                boost::shared_ptr<Market> market, const std::string& baseCurrency) {

    LOG("Writing AdditionalResults report");

    report.addColumn("TradeId", string())
        .addColumn("ResultId", string())
        .addColumn("ResultType", string())
        .addColumn("ResultValue", string());

    for (auto trade : portfolio->trades()) {
        try {
            // we first add any additional trade data.
            string tradeId = trade->id();
            string tradeType = trade->tradeType();
            Real notional2 = Null<Real>();
            string notional2Ccy = "";
            // Get the additional data for the current instrument.
            auto additionalData = trade->additionalData();
            for (const auto& kv : additionalData) {
                auto p = parseBoostAny(kv.second, 6);
                report.next().add(tradeId).add(kv.first).add(p.first).add(p.second);
            }
            // if the 'notional[2]' has been provided convert it to base currency
            if (additionalData.count("notional[2]") != 0 && additionalData.count("notionalCurrency[2]") != 0) {
                notional2 = trade->additionalDatum<Real>("notional[2]");
                notional2Ccy = trade->additionalDatum<string>("notionalCurrency[2]");
            }

            auto additionalResults = trade->instrument()->additionalResults();
            if (additionalResults.count("notional[2]") != 0 && additionalResults.count("notionalCurrency[2]") != 0) {
                notional2 = trade->instrument()->qlInstrument()->result<Real>("notional[2]");
                notional2Ccy = trade->instrument()->qlInstrument()->result<string>("notionalCurrency[2]");
            }

            if (notional2 != Null<Real>() && notional2Ccy != "") {
                Real fx = 1.0;
                if (notional2Ccy != baseCurrency)
                    fx = market->fxRate(notional2Ccy + baseCurrency)->value();
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(8) << notional2 * fx;
                // report.next().add(tradeId).add("notionalInBaseCurrency[2]").add("double").add(oss.str());
            }

            // Just use the unadjusted trade ID in the additional results report for the main instrument.
            // If we have one or more additional instruments, use "_i" as suffix where i = 1, 2, 3, ... for each
            // additional instrument in turn and underscore as prefix to reduce risk of ID clash. We also add the
            // multiplier as an extra additional result if additional results exist.
            auto instruments = trade->instrument()->additionalInstruments();
            auto multipliers = trade->instrument()->additionalMultipliers();
            QL_REQUIRE(instruments.size() == multipliers.size(),
                       "Expected the number of "
                           << "additional instruments (" << instruments.size() << ") to equal the number of "
                           << "additional multipliers (" << multipliers.size() << ").");

            for (Size i = 0; i <= instruments.size(); ++i) {

                if (i > 0 && instruments[i - 1] == nullptr)
                    continue;

                std::map<std::string, boost::any> thisAddResults =
                    i == 0 ? additionalResults : instruments[i - 1]->additionalResults();

                // Trade ID suffix for additional instruments. Put underscores to reduce risk of clash with other IDs in
                // the portfolio (still a risk).
                tradeId = i == 0 ? trade->id() : ("_" + trade->id() + "_" + to_string(i));

                // Add the multiplier if there are additional results.
                // Check on 'instMultiplier' already existing is probably unnecessary.
                if (!thisAddResults.empty() && thisAddResults.count("instMultiplier") == 0) {
                    thisAddResults["instMultiplier"] = i == 0 ? trade->instrument()->multiplier() : multipliers[i - 1];
                }

                // Write current instrument's additional results.
                for (const auto& kv : thisAddResults) {
                    // some results are stored as maps. We loop over these so that there is one result per line
                    if (kv.second.type() == typeid(result_type_matrix)) {
                        addMapResults<result_type_matrix>(kv.second, tradeId, kv.first, report);
                    } else if (kv.second.type() == typeid(result_type_vector)) {
                        addMapResults<result_type_vector>(kv.second, tradeId, kv.first, report);
                    } else if (kv.second.type() == typeid(result_type_scalar)) {
                        addMapResults<result_type_scalar>(kv.second, tradeId, kv.first, report);
                    } else {
                        auto p = parseBoostAny(kv.second, 6);
                        report.next().add(tradeId).add(kv.first).add(p.first).add(p.second);
                    }
                }
            }
        } catch (const std::exception& e) {
            ALOG(StructuredTradeErrorMessage(trade->id(), trade->tradeType(),
                                             "Error during trade pricing (additional results)", e.what()));
        }
    }

    report.end();

    LOG("AdditionalResults report written");
}

namespace {
void addRowMktCalReport(ore::data::Report& report, const std::string& moType, const std::string& moId,
                        const std::string& resId, const std::string& key1, const std::string& key2,
                        const std::string& key3, const boost::any& value) {
    auto p = parseBoostAny(value);
    report.next().add(moType).add(moId).add(resId).add(key1).add(key2).add(key3).add(p.first).add(p.second);
}

void addYieldCurveCalibrationInfo(ore::data::Report& report, const std::string& id,
                                  boost::shared_ptr<YieldCurveCalibrationInfo> info) {

    if (info == nullptr)
        return;

    // common results
    addRowMktCalReport(report, "yieldCurve", id, "dayCounter", "", "", "", info->dayCounter);
    addRowMktCalReport(report, "yieldCurve", id, "currency", "", "", "", info->currency);

    for (Size i = 0; i < info->pillarDates.size(); ++i) {
        std::string key1 = ore::data::to_string(info->pillarDates[i]);
        addRowMktCalReport(report, "yieldCurve", id, "time", key1, "", "", info->times.at(i));
        addRowMktCalReport(report, "yieldCurve", id, "zeroRate", key1, "", "", info->zeroRates.at(i));
        addRowMktCalReport(report, "yieldCurve", id, "discountFactor", key1, "", "", info->discountFactors.at(i));
    }

    // fitted bond curve results
    auto y = boost::dynamic_pointer_cast<FittedBondCurveCalibrationInfo>(info);
    if (y) {
        addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.fittingMethod", "", "", "", y->fittingMethod);
        for (Size k = 0; k < y->solution.size(); ++k) {
            addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.solution", std::to_string(k), "", "",
                               y->solution[k]);
        }
        addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.iterations", "", "", "", y->iterations);
        addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.costValue", "", "", "", y->costValue);
        for (Size i = 0; i < y->securities.size(); ++i) {
            addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.bondMaturity", y->securities.at(i), "", "",
                               y->securityMaturityDates.at(i));
            addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.marketPrice", y->securities.at(i), "", "",
                               y->marketPrices.at(i));
            addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.modelPrice", y->securities.at(i), "", "",
                               y->modelPrices.at(i));
            addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.marketYield", y->securities.at(i), "", "",
                               y->marketYields.at(i));
            addRowMktCalReport(report, "yieldCurve", id, "fittedBondCurve.modelYield", y->securities.at(i), "", "",
                               y->modelYields.at(i));
        }
    }
}

void addInflationCurveCalibrationInfo(ore::data::Report& report, const std::string& id,
                                      boost::shared_ptr<InflationCurveCalibrationInfo> info) {
    if (info == nullptr)
        return;

    // common results
    addRowMktCalReport(report, "inflationCurve", id, "dayCounter", "", "", "", info->dayCounter);
    addRowMktCalReport(report, "inflationCurve", id, "calendar", "", "", "", info->calendar);
    addRowMktCalReport(report, "inflationCurve", id, "baseDate", "", "", "", info->baseDate);

    // zero inflation
    auto z = boost::dynamic_pointer_cast<ZeroInflationCurveCalibrationInfo>(info);
    if (z) {
        addRowMktCalReport(report, "inflationCurve", id, "baseCpi", "", "", "", z->baseCpi);
        for (Size i = 0; i < z->pillarDates.size(); ++i) {
            std::string key1 = ore::data::to_string(z->pillarDates[i]);
            addRowMktCalReport(report, "inflationCurve", id, "time", key1, "", "", z->times.at(i));
            addRowMktCalReport(report, "inflationCurve", id, "zeroRate", key1, "", "", z->zeroRates.at(i));
            addRowMktCalReport(report, "inflationCurve", id, "cpi", key1, "", "", z->forwardCpis.at(i));
        }
    }

    // yoy inflation
    auto y = boost::dynamic_pointer_cast<YoYInflationCurveCalibrationInfo>(info);
    if (y) {
        for (Size i = 0; i < y->pillarDates.size(); ++i) {
            std::string key1 = ore::data::to_string(y->pillarDates[i]);
            addRowMktCalReport(report, "inflationCurve", id, "time", key1, "", "", y->times.at(i));
            addRowMktCalReport(report, "inflationCurve", id, "yoyRate", key1, "", "", y->yoyRates.at(i));
        }
    }
}

void addCommodityCurveCalibrationInfo(ore::data::Report& report, const std::string& id,
                                      boost::shared_ptr<CommodityCurveCalibrationInfo> const& info) {
    if(info == nullptr)
        return ;
    addRowMktCalReport(report, "commodityCurve", id, "calendar", "", "", "", info->calendar);
    addRowMktCalReport(report, "commodityCurve", id, "dayCounter", "", "", "", info->dayCounter);
    addRowMktCalReport(report, "commodityCurve", id, "currenct", "", "", "", info->currency);
    addRowMktCalReport(report, "commodityCurve", id, "interpolationMethod", "", "", "", info->interpolationMethod);

    for (Size i = 0; i < info->pillarDates.size(); ++i){
        auto date = to_string(info->pillarDates.at(i));
        addRowMktCalReport(report, "commodityCurve", id, "time", date, "", "", info->times.at(i));
        addRowMktCalReport(report, "commodityCurve", id, "price", date, "", "", info->futurePrices.at(i));
    }
}

void addFxEqVolCalibrationInfo(ore::data::Report& report, const std::string& type, const std::string& id,
                               boost::shared_ptr<FxEqVolCalibrationInfo> info) {
    if (info == nullptr)
        return;

    addRowMktCalReport(report, type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowMktCalReport(report, type, id, "calendar", "", "", "", info->calendar);
    addRowMktCalReport(report, type, id, "atmType", "", "", "", info->atmType);
    addRowMktCalReport(report, type, id, "deltaType", "", "", "", info->deltaType);
    addRowMktCalReport(report, type, id, "longTermAtmType", "", "", "", info->longTermAtmType);
    addRowMktCalReport(report, type, id, "longTermDeltaType", "", "", "", info->longTermDeltaType);
    addRowMktCalReport(report, type, id, "switchTenor", "", "", "", info->switchTenor);
    addRowMktCalReport(report, type, id, "riskReversalInFavorOf", "", "", "", info->riskReversalInFavorOf);
    addRowMktCalReport(report, type, id, "butterflyStyle", "", "", "", info->butterflyStyle);
    addRowMktCalReport(report, type, id, "isArbitrageFree", "", "", "", info->isArbitrageFree);
    for (Size i = 0; i < info->messages.size(); ++i)
        addRowMktCalReport(report, type, id, "message_" + std::to_string(i), "", "", "", info->messages[i]);

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        addRowMktCalReport(report, type, id, "expiry", tStr, "", "", info->expiryDates.at(i));
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size j = 0; j < info->deltas.size(); ++j) {
            std::string dStr = info->deltas.at(j);
            addRowMktCalReport(report, type, id, "forward", tStr, dStr, "", info->forwards.at(i));
            addRowMktCalReport(report, type, id, "strike", tStr, dStr, "", info->deltaGridStrikes.at(i).at(j));
            addRowMktCalReport(report, type, id, "vol", tStr, dStr, "", info->deltaGridImpliedVolatility.at(i).at(j));
            addRowMktCalReport(report, type, id, "prob", tStr, dStr, "", info->deltaGridProb.at(i).at(j));
            addRowMktCalReport(report, type, id, "callSpreadArb", tStr, dStr, "",
                               static_cast<bool>(info->deltaGridCallSpreadArbitrage.at(i).at(j)));
            addRowMktCalReport(report, type, id, "butterflyArb", tStr, dStr, "",
                               static_cast<bool>(info->deltaGridButterflyArbitrage.at(i).at(j)));
        }
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size j = 0; j < info->moneyness.size(); ++j) {
            std::string mStr = std::to_string(info->moneyness.at(j));
            addRowMktCalReport(report, type, id, "forward", tStr, mStr, "", info->forwards.at(i));
            addRowMktCalReport(report, type, id, "strike", tStr, mStr, "", info->moneynessGridStrikes.at(i).at(j));
            addRowMktCalReport(report, type, id, "vol", tStr, mStr, "",
                               info->moneynessGridImpliedVolatility.at(i).at(j));
            addRowMktCalReport(report, type, id, "prob", tStr, mStr, "", info->moneynessGridProb.at(i).at(j));
            addRowMktCalReport(report, type, id, "callSpreadArb", tStr, mStr, "",
                               static_cast<bool>(info->moneynessGridCallSpreadArbitrage.at(i).at(j)));
            addRowMktCalReport(report, type, id, "butterflyArb", tStr, mStr, "",
                               static_cast<bool>(info->moneynessGridButterflyArbitrage.at(i).at(j)));
            addRowMktCalReport(report, type, id, "calendarArb", tStr, mStr, "",
                               static_cast<bool>(info->moneynessGridCalendarArbitrage.at(i).at(j)));
        }
    }
}

void addIrVolCalibrationInfo(ore::data::Report& report, const std::string& type, const std::string& id,
                             boost::shared_ptr<IrVolCalibrationInfo> info) {
    if (info == nullptr)
        return;

    addRowMktCalReport(report, type, id, "dayCounter", "", "", "", info->dayCounter);
    addRowMktCalReport(report, type, id, "calendar", "", "", "", info->calendar);
    addRowMktCalReport(report, type, id, "isArbitrageFree", "", "", "", info->isArbitrageFree);
    addRowMktCalReport(report, type, id, "volatilityType", "", "", "", info->volatilityType);
    for (Size i = 0; i < info->messages.size(); ++i)
        addRowMktCalReport(report, type, id, "message_" + std::to_string(i), "", "", "", info->messages[i]);

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        addRowMktCalReport(report, type, id, "expiry", tStr, "", "", info->expiryDates.at(i));
    }

    for (Size i = 0; i < info->underlyingTenors.size(); ++i) {
        addRowMktCalReport(report, type, id, "tenor", std::to_string(i), "", "",
                           ore::data::to_string(info->underlyingTenors.at(i)));
    }

    for (Size i = 0; i < info->times.size(); ++i) {
        std::string tStr = std::to_string(info->times.at(i));
        for (Size u = 0; u < info->underlyingTenors.size(); ++u) {
            std::string uStr = ore::data::to_string(info->underlyingTenors[u]);
            for (Size j = 0; j < info->strikes.size(); ++j) {
                std::string kStr = std::to_string(info->strikes.at(j));
                addRowMktCalReport(report, type, id, "forward", tStr, kStr, uStr, info->forwards.at(i).at(u));
                addRowMktCalReport(report, type, id, "strike", tStr, kStr, uStr,
                                   info->strikeGridStrikes.at(i).at(u).at(j));
                addRowMktCalReport(report, type, id, "vol", tStr, kStr, uStr,
                                   info->strikeGridImpliedVolatility.at(i).at(u).at(j));
                addRowMktCalReport(report, type, id, "prob", tStr, kStr, uStr, info->strikeGridProb.at(i).at(u).at(j));
                addRowMktCalReport(report, type, id, "callSpreadArb", tStr, kStr, uStr,
                                   static_cast<bool>(info->strikeGridCallSpreadArbitrage.at(i).at(u).at(j)));
                addRowMktCalReport(report, type, id, "butterflyArb", tStr, kStr, uStr,
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
                addRowMktCalReport(report, type, id, "forward", tStr, kStr, uStr, info->forwards.at(i).at(u));
                addRowMktCalReport(report, type, id, "strike", tStr, kStr, uStr,
                                   info->strikeSpreadGridStrikes.at(i).at(u).at(j));
                addRowMktCalReport(report, type, id, "vol", tStr, kStr, uStr,
                                   info->strikeSpreadGridImpliedVolatility.at(i).at(u).at(j));
                addRowMktCalReport(report, type, id, "prob", tStr, kStr, uStr,
                                   info->strikeSpreadGridProb.at(i).at(u).at(j));
                addRowMktCalReport(report, type, id, "callSpreadArb", tStr, kStr, uStr,
                                   static_cast<bool>(info->strikeSpreadGridCallSpreadArbitrage.at(i).at(u).at(j)));
                addRowMktCalReport(report, type, id, "butterflyArb", tStr, kStr, uStr,
                                   static_cast<bool>(info->strikeSpreadGridButterflyArbitrage.at(i).at(u).at(j)));
            }
        }
    }
}

} // namespace

void ReportWriter::writeTodaysMarketCalibrationReport(
    ore::data::Report& report, boost::shared_ptr<ore::data::TodaysMarketCalibrationInfo> calibrationInfo) {
    LOG("Writing TodaysMarketCalibration report");

    report.addColumn("MarketObjectType", string())
        .addColumn("MarketObjectId", string())
        .addColumn("ResultId", string())
        .addColumn("ResultKey1", string())
        .addColumn("ResultKey2", string())
        .addColumn("ResultKey3", string())
        .addColumn("ResultType", string())
        .addColumn("ResultValue", string());

    // yield curve results

    for (auto const& r : calibrationInfo->yieldCurveCalibrationInfo) {
        addYieldCurveCalibrationInfo(report, r.first, r.second);
    }

    for (auto const& r : calibrationInfo->dividendCurveCalibrationInfo) {
        addYieldCurveCalibrationInfo(report, r.first, r.second);
    }

    // inflation curve results

    for (auto const& r : calibrationInfo->inflationCurveCalibrationInfo) {
        addInflationCurveCalibrationInfo(report, r.first, r.second);
    }

    // commodity curve results

    for (auto const& r :calibrationInfo->commodityCurveCalibrationInfo) {
        addCommodityCurveCalibrationInfo(report, r.first, r.second);
    }

    // fx vol results
    for (auto const& r : calibrationInfo->fxVolCalibrationInfo) {
        addFxEqVolCalibrationInfo(report, "fxVol", r.first, r.second);
    }

    // eq vol results
    for (auto const& r : calibrationInfo->eqVolCalibrationInfo) {
        addFxEqVolCalibrationInfo(report, "eqVol", r.first, r.second);
    }

    // ir vol results
    for (auto const& r : calibrationInfo->irVolCalibrationInfo) {
        addIrVolCalibrationInfo(report, "irVol", r.first, r.second);
    }

    report.end();
    LOG("TodaysMktCalibration report written");
}

void ReportWriter::addMarketDatum(Report& report, const ore::data::MarketDatum& md) {
    report.next().add(md.asofDate()).add(md.name()).add(md.quote()->value());
}

void ReportWriter::writeMarketData(Report& report, const boost::shared_ptr<Loader>& loader, const Date& asof,
                                   const set<string>& quoteNames, bool returnAll) {

    LOG("Writing MarketData report");

    report.addColumn("datumDate", Date()).addColumn("datumId", string()).addColumn("datumValue", double(), 10);

    if (returnAll) {
        for (const auto& md : loader->loadQuotes(asof)) {
            addMarketDatum(report, *md);
        }
        return;
    }

    set<string> names;
    set<string> regexStrs;
    partitionQuotes(quoteNames, names, regexStrs);

    vector<std::regex> regexes;
    regexes.reserve(regexStrs.size());
    for (auto regexStr : regexStrs) {
        regexes.push_back(regex(regexStr));
    }

    for (const auto& md : loader->loadQuotes(asof)) {
        const auto& mdName = md->name();

        if (names.find(mdName) != names.end()) {
            addMarketDatum(report, *md);
            continue;
        }

        // This could be slow
        for (const auto& regex : regexes) {
            if (regex_match(mdName, regex)) {
                addMarketDatum(report, *md);
                break;
            }
        }
    }

    report.end();
    LOG("MarketData report written");
}

void ReportWriter::writeFixings(Report& report, const boost::shared_ptr<Loader>& loader) {

    LOG("Writing Fixings report");

    report.addColumn("fixingDate", Date()).addColumn("fixingId", string()).addColumn("fixingValue", double(), 10);

    for (const auto& f : loader->loadFixings()) {
        report.next().add(f.date).add(f.name).add(f.fixing);
    }

    report.end();
    LOG("Fixings report written");
}

void ReportWriter::writeDividends(Report& report, const boost::shared_ptr<Loader>& loader) {

    LOG("Writing Dividends report");

    report.addColumn("dividendDate", Date())
        .addColumn("equityId", string())
        .addColumn("dividendRate", double(), 10);

    for (const auto& f : loader->loadDividends()) {
        report.next().add(f.date).add(f.name).add(f.fixing);
    }

    report.end();
    LOG("Dividends report written");
}

void ReportWriter::writePricingStats(ore::data::Report& report, const boost::shared_ptr<Portfolio>& portfolio) {

    LOG("Writing Pricing stats report");

    report.addColumn("TradeId", string())
        .addColumn("TradeType", string())
        .addColumn("NumberOfPricings", Size())
        .addColumn("CumulativeTiming", Size())
        .addColumn("AverageTiming", Size());

    for (auto const& t : portfolio->trades()) {
        std::size_t num = t->getNumberOfPricings();
        Size cumulative = t->getCumulativePricingTime() / 1000;
        Size average = num > 0 ? cumulative / num : 0;
        report.next().add(t->id()).add(t->tradeType()).add(num).add(cumulative).add(average);
    }

    report.end();
    LOG("Pricing stats report written");
}

} // namespace analytics
} // namespace ore
