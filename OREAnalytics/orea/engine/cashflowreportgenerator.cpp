/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

#include <orea/engine/cashflowreportgenerator.hpp>

#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/durationadjustedcmscoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/typedcashflow.hpp>
#include <qle/currencies/currencycomparator.hpp>
#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/errors.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>

#include <vector>

namespace ore {
namespace analytics {

using namespace ore::data;
using namespace QuantLib;
using namespace QuantExt;

namespace {

void populateReportDataFromAdditionalResults(
    std::vector<TradeCashflowReportData>& result, std::map<Size, Size>& cashflowNumber,
    const std::map<std::string, boost::any>& addResults, const Real multiplier, const std::string& baseCurrency,
    const std::vector<std::string>& legCurrencies, const std::string& npvCurrency,
    QuantLib::ext::shared_ptr<ore::data::Market> market, const Handle<YieldTermStructure>& specificDiscountCurve,
    const std::string& configuration, const bool includePastCashflows) {

    Date asof = Settings::instance().evaluationDate();

    // ensures all cashFlowResults from composite trades are being accounted for
    auto lower = addResults.lower_bound("cashFlowResults");
    auto upper = addResults.lower_bound("cashFlowResults_a"); // Upper bound due to alphabetical order

    for (auto cashFlowResults = lower; cashFlowResults != upper; ++cashFlowResults) {

        QL_REQUIRE(cashFlowResults->second.type() == typeid(std::vector<CashFlowResults>),
                   "internal error: cashflowResults type does not match CashFlowResults: '"
                       << cashFlowResults->second.type().name() << "'");
        std::vector<CashFlowResults> cfResults = boost::any_cast<std::vector<CashFlowResults>>(cashFlowResults->second);

        for (auto const& cf : cfResults) {

            Real effectiveAmount = Null<Real>();
            Real discountFactor = Null<Real>();
            Real presentValue = Null<Real>();
            Real presentValueBase = Null<Real>();
            Real fxRateLocalBase = Null<Real>();
            Real floorStrike = Null<Real>();
            Real capStrike = Null<Real>();
            Real floorVolatility = Null<Real>();
            Real capVolatility = Null<Real>();
            Real effectiveFloorVolatility = Null<Real>();
            Real effectiveCapVolatility = Null<Real>();

            string ccy;
            if (!cf.currency.empty()) {
                ccy = cf.currency;
            } else {
                ccy = npvCurrency;
            }

            if (cf.amount != Null<Real>())
                effectiveAmount = cf.amount * multiplier;
            if (cf.discountFactor != Null<Real>())
                discountFactor = cf.discountFactor;
            else if (!ccy.empty() && cf.payDate != Null<Date>() && market) {
                auto discountCurve =
                    specificDiscountCurve.empty() ? market->discountCurve(ccy, configuration) : specificDiscountCurve;
                discountFactor = cf.payDate < asof ? 0.0 : discountCurve->discount(cf.payDate);
            }
            if (cf.presentValue != Null<Real>()) {
                presentValue = cf.presentValue * multiplier;
            } else if (effectiveAmount != Null<Real>() && discountFactor != Null<Real>()) {
                presentValue = effectiveAmount * discountFactor;
            }
            if (cf.fxRateLocalBase != Null<Real>()) {
                fxRateLocalBase = cf.fxRateLocalBase;
            } else if (!ccy.empty() && market) {
                try {
                    fxRateLocalBase = market->fxRate(ccy + baseCurrency, configuration)->value();
                } catch (...) {
                }
            }
            if (cf.presentValueBase != Null<Real>()) {
                presentValueBase = cf.presentValueBase;
            } else if (presentValue != Null<Real>() && fxRateLocalBase != Null<Real>()) {
                presentValueBase = presentValue * fxRateLocalBase;
            }
            if (cf.floorStrike != Null<Real>())
                floorStrike = cf.floorStrike;
            if (cf.capStrike != Null<Real>())
                capStrike = cf.capStrike;
            if (cf.floorVolatility != Null<Real>())
                floorVolatility = cf.floorVolatility;
            if (cf.capVolatility != Null<Real>())
                capVolatility = cf.capVolatility;
            if (cf.effectiveFloorVolatility != Null<Real>())
                floorVolatility = cf.effectiveFloorVolatility;
            if (cf.effectiveCapVolatility != Null<Real>())
                capVolatility = cf.effectiveCapVolatility;

            // to be consistent with the leg-based cf report we should do this:
            // if (!includePastCashflows && cf.payDate <= asof)
            //     continue;
            // however, this changes a lot of results, so we output all cfs for the time being

            result.emplace_back();
            result.back().cashflowNo = ++cashflowNumber[cf.legNumber];
            result.back().legNo = cf.legNumber;
            result.back().payDate = cf.payDate;
            result.back().flowType = cf.type;
            result.back().amount = effectiveAmount;
            result.back().currency = ccy;
            result.back().coupon = cf.rate;
            result.back().accrual = cf.accrualPeriod;
            result.back().accrualStartDate = cf.accrualStartDate;
            result.back().accrualEndDate = cf.accrualEndDate;
            result.back().accruedAmount = cf.accruedAmount * (cf.accruedAmount == Null<Real>() ? 1.0 : multiplier);
            result.back().fixingDate = cf.fixingDate;
            result.back().fixingValue = cf.fixingValue;
            result.back().notional = cf.notional * (cf.notional == Null<Real>() ? 1.0 : multiplier);
            result.back().discountFactor = discountFactor;
            result.back().presentValue = presentValue;
            result.back().fxRateLocalBase = fxRateLocalBase;
            result.back().presentValueBase = presentValueBase;
            result.back().baseCurrency = baseCurrency;
            result.back().floorStrike = floorStrike;
            result.back().capStrike = capStrike;
            result.back().floorVolatility = floorVolatility;
            result.back().capVolatility = capVolatility;
            result.back().effectiveFloorVolatility = effectiveFloorVolatility;
            result.back().effectiveCapVolatility = effectiveCapVolatility;
        }
    }
}

} // namespace

std::vector<TradeCashflowReportData> generateCashflowReportData(const ext::shared_ptr<ore::data::Trade>& trade,
                                                                const std::string& baseCurrency,
                                                                QuantLib::ext::shared_ptr<ore::data::Market> market,
                                                                const std::string& configuration,
                                                                const bool includePastCashflows) {

    std::vector<TradeCashflowReportData> result;

    Date asof = Settings::instance().evaluationDate();

    string specificDiscountStr = trade->envelope().additionalField("discount_curve", false);
    Handle<YieldTermStructure> specificDiscountCurve;
    if (!specificDiscountStr.empty() && market)
        specificDiscountCurve = indexOrYieldCurve(market, specificDiscountStr, configuration);

    Real multiplier = trade->instrument()->multiplier() * trade->instrument()->multiplier2();

    // add cashflows from (ql-) additional results in instrument and additional instruments

    std::map<Size, Size> cashflowNumber;

    populateReportDataFromAdditionalResults(result, cashflowNumber, trade->instrument()->additionalResults(),
                                            multiplier, baseCurrency, trade->legCurrencies(), trade->npvCurrency(),
                                            market, specificDiscountCurve, configuration, includePastCashflows);

    for (std::size_t i = 0; i < trade->instrument()->additionalInstruments().size(); ++i) {
        populateReportDataFromAdditionalResults(
            result, cashflowNumber, trade->instrument()->additionalInstruments()[i]->additionalResults(),
            trade->instrument()->additionalMultipliers()[i], baseCurrency, trade->legCurrencies(), trade->npvCurrency(),
            market, specificDiscountCurve, configuration, includePastCashflows);
    }

    // determine offset for leg numbering to avoid conflicting leg numbers from add results and leg-based results

    Size legNoOffset = 0;
    if (auto l = std::max_element(
            result.begin(), result.end(),
            [](const TradeCashflowReportData& d1, const TradeCashflowReportData& d2) { return d1.legNo < d2.legNo; });
        l != result.end()) {
        legNoOffset = l->legNo;
    }

    // add cashflows from trade legs, if no cashflows were added so far or if a leg is marked as mandatory for cashflows

    bool haveEngineCashflows = !result.empty();

    for (size_t i = 0; i < trade->legs().size(); i++) {

        Trade::LegCashflowInclusion legCashflowInclusion = Trade::LegCashflowInclusion::IfNoEngineCashflows;
        if (auto incl = trade->legCashflowInclusion().find(i); incl != trade->legCashflowInclusion().end()) {
            legCashflowInclusion = incl->second;
        }

        if (legCashflowInclusion == Trade::LegCashflowInclusion::Never ||
            (legCashflowInclusion == Trade::LegCashflowInclusion::IfNoEngineCashflows && haveEngineCashflows))
            continue;

        const QuantLib::Leg& leg = trade->legs()[i];
        bool payer = trade->legPayers()[i];

        string ccy = trade->legCurrencies()[i];

        Handle<YieldTermStructure> discountCurve = specificDiscountCurve;
        if (discountCurve.empty() && market) {
            discountCurve = market->discountCurve(ccy, configuration);
        }

        for (size_t j = 0; j < leg.size(); j++) {
            QuantLib::ext::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
            Date payDate = ptrFlow->date();
            if (!ptrFlow->hasOccurred(asof) || includePastCashflows) {
                Real amount = ptrFlow->amount();
                string flowType = "";
                if (payer)
                    amount *= -1.0;
                std::string ccy = trade->legCurrencies()[i];

                QuantLib::ext::shared_ptr<QuantLib::Coupon> ptrCoupon =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::CommodityCashFlow> ptrCommCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::TypedCashFlow> ptrTypedCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::TypedCashFlow>(ptrFlow);

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
                    notional = ptrCommCf->periodQuantity(); // this is measured in units, e.g. barrels for oil
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = "Notional (units)";
                } else if (ptrTypedCf) {
                    coupon = Null<Real>();
                    accrual = Null<Real>();
                    notional = Null<Real>();
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = ore::data::to_string(ptrTypedCf->type());
                } else {
                    coupon = Null<Real>();
                    accrual = Null<Real>();
                    notional = Null<Real>();
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = "Notional";
                }

                if (auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow)) {
                    ptrFlow = unpackIndexedCoupon(cpn);
                }

                QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> ptrFloat =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantLib::InflationCoupon> ptrInfl =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::InflationCoupon>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantLib::IndexedCashFlow> ptrIndCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::IndexedCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::FXLinkedCashFlow> ptrFxlCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::FXLinkedCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::EquityCoupon> ptrEqCp =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityCoupon>(ptrFlow);

                Date fixingDate;
                Real fixingValue = Null<Real>();
                if (ptrFloat) {
                    fixingDate = ptrFloat->fixingDate();
                    if (fixingDate > asof)
                        flowType = "InterestProjected";

                    try {
                        fixingValue = ptrFloat->index()->fixing(fixingDate);
                    } catch (...) {
                    }

                    if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::IborCoupon>(ptrFloat)) {
                        fixingValue = (c->rate() - c->spread()) / c->gearing();
                    }

                    if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredIborCoupon>(ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                    }

                    if (auto sc =
                            QuantLib::ext::dynamic_pointer_cast<QuantLib::StrippedCappedFlooredCoupon>(ptrFloat)) {
                        if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredIborCoupon>(
                                sc->underlying())) {
                            fixingValue =
                                (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                        }
                    }

                    // for (capped-floored) BMA / ON / subperiod coupons the fixing value is the
                    // compounded / averaged rate, not a single index fixing

                    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(ptrFloat)) {
                        fixingValue = (on->rate() - on->spread()) / on->gearing();
                    } else if (auto on =
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(ptrFloat)) {
                        fixingValue = (on->rate() - on->effectiveSpread()) / on->gearing();
                    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(ptrFloat)) {
                        fixingValue = (c->rate() - c->spread()) / c->gearing();
                    } else if (auto c =
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(
                                       ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                    } else if (auto c =
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(
                                       ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->effectiveSpread()) / c->underlying()->gearing();
                    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageBMACoupon>(
                                   ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                    } else if (auto sp = QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(ptrFloat)) {
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
                    fixingDate = ptrCommCf->lastPricingDate();
                    fixingValue = ptrCommCf->fixing();
                } else {
                    fixingDate = Null<Date>();
                    fixingValue = Null<Real>();
                }

                Real effectiveAmount = Null<Real>();
                Real discountFactor = Null<Real>();
                Real presentValue = Null<Real>();
                Real presentValueBase = Null<Real>();
                Real fxRateLocalBase = Null<Real>();
                Real floorStrike = Null<Real>();
                Real capStrike = Null<Real>();
                Real floorVolatility = Null<Real>();
                Real capVolatility = Null<Real>();
                Real effectiveFloorVolatility = Null<Real>();
                Real effectiveCapVolatility = Null<Real>();

                if (amount != Null<Real>())
                    effectiveAmount = amount * multiplier;

                if (market) {
                    auto dcurve = specificDiscountCurve.empty() ? discountCurve : specificDiscountCurve;
                    discountFactor = ptrFlow->hasOccurred(asof) ? 0.0 : dcurve->discount(payDate);
                    if (effectiveAmount != Null<Real>())
                        presentValue = discountFactor * effectiveAmount;
                    try {
                        fxRateLocalBase = market->fxRate(ccy + baseCurrency, configuration)->value();
                        presentValueBase = presentValue * fxRateLocalBase;
                    } catch (...) {
                    }

                    // scan for known capped / floored coupons and extract cap / floor strike and fixing
                    // date

                    // unpack stripped cap/floor coupon
                    QuantLib::ext::shared_ptr<CashFlow> c = ptrFlow;
                    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(ptrFlow)) {
                        c = tmp->underlying();
                    }
                    Date volFixingDate;
                    std::string qlIndexName; // index used to retrieve vol
                    bool usesCapVol = false, usesSwaptionVol = false;
                    Period swaptionTenor;
                    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredCoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->fixingDate();
                        qlIndexName = tmp->index()->name();
                        if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(tmp->underlying())) {
                            swaptionTenor = cms->swapIndex()->tenor();
                            qlIndexName = cms->swapIndex()->iborIndex()->name();
                            usesSwaptionVol = true;
                        } else if (auto cms = QuantLib::ext::dynamic_pointer_cast<DurationAdjustedCmsCoupon>(
                                       tmp->underlying())) {
                            swaptionTenor = cms->swapIndex()->tenor();
                            qlIndexName = cms->swapIndex()->iborIndex()->name();
                            usesSwaptionVol = true;
                        } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(tmp->underlying())) {
                            qlIndexName = ibor->index()->name();
                            usesCapVol = true;
                        }
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->underlying()->fixingDates().front();
                        qlIndexName = tmp->index()->name();
                        usesCapVol = true;
                        if (floorStrike != Null<Real>())
                            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                        if (capStrike != Null<Real>())
                            effectiveCapVolatility = tmp->effectiveCapletVolatility();
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->underlying()->fixingDates().front();
                        qlIndexName = tmp->index()->name();
                        usesCapVol = true;
                        if (floorStrike != Null<Real>())
                            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                        if (capStrike != Null<Real>())
                            effectiveCapVolatility = tmp->effectiveCapletVolatility();
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->underlying()->fixingDates().front();
                        qlIndexName = tmp->index()->name();
                        usesCapVol = true;
                        if (floorStrike != Null<Real>())
                            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                        if (capStrike != Null<Real>())
                            effectiveCapVolatility = tmp->effectiveCapletVolatility();
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
                            } else if (usesCapVol && floorVolatility == Null<Real>()) {
                                floorVolatility =
                                    market
                                        ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                      configuration)
                                        ->volatility(volFixingDate, floorStrike);
                            }
                        }
                        if (capStrike != Null<Real>()) {
                            if (usesSwaptionVol) {
                                capVolatility = market
                                                    ->swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, swaptionTenor, capStrike);
                            } else if (usesCapVol && capVolatility == Null<Real>()) {
                                capVolatility = market
                                                    ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, capStrike);
                            }
                        }
                    }
                }

                result.emplace_back();

                result.back().cashflowNo = j + 1;
                result.back().legNo = i + legNoOffset;
                result.back().payDate = payDate;
                result.back().flowType = flowType;
                result.back().amount = effectiveAmount;
                result.back().currency = ccy;
                result.back().coupon = coupon;
                result.back().accrual = accrual;
                result.back().accrualStartDate = accrualStartDate;
                result.back().accrualEndDate = accrualEndDate;
                result.back().accruedAmount = accruedAmount * (accruedAmount == Null<Real>() ? 1.0 : multiplier);
                result.back().fixingDate = fixingDate;
                result.back().fixingValue = fixingValue;
                result.back().notional = notional * (notional == Null<Real>() ? 1.0 : multiplier);
                result.back().discountFactor = discountFactor;
                result.back().presentValue = presentValue;
                result.back().fxRateLocalBase = fxRateLocalBase;
                result.back().presentValueBase = presentValueBase;
                result.back().baseCurrency = baseCurrency;
                result.back().floorStrike = floorStrike;
                result.back().capStrike = capStrike;
                result.back().floorVolatility = floorVolatility;
                result.back().capVolatility = capVolatility;
                result.back().effectiveFloorVolatility = effectiveFloorVolatility;
                result.back().effectiveCapVolatility = effectiveCapVolatility;
            }
        }
    }

    return result;
}

} // namespace analytics
} // namespace ore
