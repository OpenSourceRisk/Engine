/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/trade.hpp
    \brief base trade data model and serialization
    \ingroup portfolio
*/

#include <ored/portfolio/cashflowutils.hpp>
#include <ored/utilities/indexnametranslator.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/durationadjustedcmscoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/interpolatediborcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/typedcashflow.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

void populateReportDataFromAdditionalResults(std::vector<TradeCashflowReportData>& result,
                                             std::map<Size, Size>& cashflowNumber,
                                             const std::map<std::string, QuantLib::ext::any>& addResults,
                                             const Real multiplier, const std::string& baseCurrency,
                                             const std::string& npvCurrency,
                                             QuantLib::ext::shared_ptr<ore::data::Market> market,
                                             const Handle<YieldTermStructure>& specificDiscountCurve,
                                             const std::string& configuration, const bool includePastCashflows) {

    Date asof = Settings::instance().evaluationDate();

    // ensures all cashFlowResults from composite trades are being accounted for
    auto lower = addResults.lower_bound("cashFlowResults");
    auto upper = addResults.lower_bound("cashFlowResults_a"); // Upper bound due to alphabetical order

    for (auto cashFlowResults = lower; cashFlowResults != upper; ++cashFlowResults) {

        QL_REQUIRE(cashFlowResults->second.type() == typeid(std::vector<CashFlowResults>),
                   "internal error: cashflowResults type does not match CashFlowResults: '"
                       << cashFlowResults->second.type().name() << "'");
        std::vector<CashFlowResults> cfResults =
            QuantLib::ext::any_cast<std::vector<CashFlowResults>>(cashFlowResults->second);

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
            else if (!ccy.empty() && cf.payDate != Null<Date>()) {
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
            } else if (!ccy.empty()) {
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

TradeCashflowReportData getCashflowReportData(
    ext::shared_ptr<CashFlow> ptrFlow, const bool payer, const double multiplier, const std::string& baseCcy,
    const std::string ccy, const Date asof, const ext::shared_ptr<YieldTermStructure>& discountCurveCcy,
    const double fxCcyBase,
    const std::function<ext::shared_ptr<SwaptionVolatilityStructure>(const std::string& qualifier)>& swaptionVol,
    const std::function<ext::shared_ptr<OptionletVolatilityStructure>(const std::string& qualifier)>& capFloorVol) {

    Real amount = (payer ? -1.0 : 1.0) * ptrFlow->amount();

    Date payDate = ptrFlow->date();

    auto ptrCoupon = ext::dynamic_pointer_cast<Coupon>(ptrFlow);
    auto ptrCommCf = ext::dynamic_pointer_cast<CommodityCashFlow>(ptrFlow);
    auto ptrTypedCf = ext::dynamic_pointer_cast<TypedCashFlow>(ptrFlow);
    auto ptrFxlTypedCf = ext::dynamic_pointer_cast<FXLinkedTypedCashFlow>(ptrFlow);

    string flowType;
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
    } else if (ptrFxlTypedCf) {
        coupon = Null<Real>();
        accrual = Null<Real>();
        notional = Null<Real>();
        accrualStartDate = accrualEndDate = Null<Date>();
        accruedAmount = Null<Real>();
        flowType = ore::data::to_string(ptrFxlTypedCf->type());
    } else {
        coupon = Null<Real>();
        accrual = Null<Real>();
        notional = Null<Real>();
        accrualStartDate = accrualEndDate = Null<Date>();
        accruedAmount = Null<Real>();
        flowType = "Notional";
    }

    ptrFlow = unpackIndexedCouponOrCashFlow(ptrFlow);

    auto ptrFloat = ext::dynamic_pointer_cast<FloatingRateCoupon>(ptrFlow);
    auto ptrInfl = ext::dynamic_pointer_cast<InflationCoupon>(ptrFlow);
    auto ptrIndCf = ext::dynamic_pointer_cast<IndexedCashFlow>(ptrFlow);
    auto ptrCommIndCf = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(ptrFlow);
    auto ptrCommIndAvgCf = ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(ptrFlow);
    auto ptrFxlCf = ext::dynamic_pointer_cast<FXLinkedCashFlow>(ptrFlow);
    auto ptrEqCp = ext::dynamic_pointer_cast<EquityCoupon>(ptrFlow);

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

        if (auto c = ext::dynamic_pointer_cast<IborCoupon>(ptrFloat)) {
            fixingValue = (c->rate() - c->spread()) / c->gearing();
        }

        if (auto c = ext::dynamic_pointer_cast<InterpolatedIborCoupon>(ptrFloat)) {
            fixingValue = (c->rate() - c->spread()) / c->gearing();
        }

        if (auto c = ext::dynamic_pointer_cast<CappedFlooredIborCoupon>(ptrFloat)) {
            fixingValue = (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
        }

        if (auto sc = ext::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(ptrFloat)) {
            if (auto c = ext::dynamic_pointer_cast<CappedFlooredIborCoupon>(sc->underlying())) {
                fixingValue = (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
            }
        }

        // for (capped-floored) BMA / ON / subperiod coupons the fixing value is the
        // compounded / averaged rate, not a single index fixing

        if (auto on = ext::dynamic_pointer_cast<AverageONIndexedCoupon>(ptrFloat)) {
            fixingValue = (on->rate() - on->spread()) / on->gearing();
        } else if (auto on = ext::dynamic_pointer_cast<QuantLib::OvernightIndexedCoupon>(ptrFloat)) {
            fixingValue = (on->rate() - on->spread()) / on->gearing();
        } else if (auto on = ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(ptrFloat)) {
            fixingValue = (on->rate() - on->effectiveSpread()) / on->gearing();
        } else if (auto c = ext::dynamic_pointer_cast<AverageBMACoupon>(ptrFloat)) {
            fixingValue = (c->rate() - c->spread()) / c->gearing();
        } else if (auto c = ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(ptrFloat)) {
            fixingValue = (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
        } else if (auto c = ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(ptrFloat)) {
            fixingValue = (c->underlying()->rate() - c->underlying()->effectiveSpread()) / c->underlying()->gearing();
        } else if (auto c = ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(ptrFloat)) {
            fixingValue = (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
        } else if (auto sp = ext::dynamic_pointer_cast<SubPeriodsCoupon1>(ptrFloat)) {
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
    } else if (ptrCommIndCf) {
        fixingDate = ptrCommIndCf->lastPricingDate();
        fixingValue = ptrCommIndCf->fixing();
        flowType = "Notional (units)";
    } else if (ptrCommIndAvgCf) {
        fixingDate = ptrCommIndAvgCf->lastPricingDate();
        fixingValue = ptrCommIndAvgCf->fixing();
        flowType = "Notional (units)";
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
    Real floorStrike = Null<Real>();
    Real capStrike = Null<Real>();
    Real floorVolatility = Null<Real>();
    Real capVolatility = Null<Real>();
    Real effectiveFloorVolatility = Null<Real>();
    Real effectiveCapVolatility = Null<Real>();

    if (amount != Null<Real>())
        effectiveAmount = amount * multiplier;

    discountFactor = ptrFlow->hasOccurred(asof) ? 0.0 : discountCurveCcy->discount(payDate);
    if (effectiveAmount != Null<Real>())
        presentValue = discountFactor * effectiveAmount;
    try {
        presentValueBase = presentValue * fxCcyBase;
    } catch (...) {
    }

    // scan for known capped / floored coupons and extract cap / floor strike and fixing
    // date

    // unpack stripped cap/floor coupon
    ext::shared_ptr<CashFlow> c = ptrFlow;
    if (auto tmp = ext::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(ptrFlow)) {
        c = tmp->underlying();
    }
    Date volFixingDate;
    std::string qlIndexName; // index used to retrieve vol
    bool usesCapVol = false, usesSwaptionVol = false;
    Period swaptionTenor;
    if (auto tmp = ext::dynamic_pointer_cast<CappedFlooredCoupon>(c)) {
        floorStrike = tmp->effectiveFloor();
        capStrike = tmp->effectiveCap();
        volFixingDate = tmp->fixingDate();
        qlIndexName = tmp->index()->name();
        if (auto cms = ext::dynamic_pointer_cast<CmsCoupon>(tmp->underlying())) {
            swaptionTenor = cms->swapIndex()->tenor();
            qlIndexName = cms->swapIndex()->iborIndex()->name();
            usesSwaptionVol = true;
        } else if (auto cms = ext::dynamic_pointer_cast<DurationAdjustedCmsCoupon>(tmp->underlying())) {
            swaptionTenor = cms->swapIndex()->tenor();
            qlIndexName = cms->swapIndex()->iborIndex()->name();
            usesSwaptionVol = true;
        } else if (auto ibor = ext::dynamic_pointer_cast<IborCoupon>(tmp->underlying())) {
            qlIndexName = ibor->index()->name();
            usesCapVol = true;
        } else if (auto ibor = ext::dynamic_pointer_cast<InterpolatedIborCoupon>(tmp->underlying())) {
            qlIndexName = ibor->iborIndex()->name();
            usesCapVol = true;
        }
    } else if (auto tmp = ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c)) {
        floorStrike = tmp->effectiveFloor();
        capStrike = tmp->effectiveCap();
        volFixingDate = tmp->underlying()->fixingDates().front();
        qlIndexName = tmp->index()->name();
        usesCapVol = true;
        if (floorStrike != Null<Real>())
            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
        if (capStrike != Null<Real>())
            effectiveCapVolatility = tmp->effectiveCapletVolatility();
    } else if (auto tmp = ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(c)) {
        floorStrike = tmp->effectiveFloor();
        capStrike = tmp->effectiveCap();
        volFixingDate = tmp->underlying()->fixingDates().front();
        qlIndexName = tmp->index()->name();
        usesCapVol = true;
        if (floorStrike != Null<Real>())
            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
        if (capStrike != Null<Real>())
            effectiveCapVolatility = tmp->effectiveCapletVolatility();
    } else if (auto tmp = ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(c)) {
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

    if (volFixingDate != Date() && fixingDate > asof) {
        volFixingDate = std::max(volFixingDate, asof + 1);
        if (floorStrike != Null<Real>()) {
            if (usesSwaptionVol) {
                floorVolatility = swaptionVol ? swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName))
                                                    ->volatility(volFixingDate, swaptionTenor, floorStrike)
                                              : Null<Real>();
            } else if (usesCapVol && floorVolatility == Null<Real>()) {
                floorVolatility = capFloorVol ? capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName))
                                                    ->volatility(volFixingDate, floorStrike)
                                              : Null<Real>();
            }
        }
        if (capStrike != Null<Real>()) {
            if (usesSwaptionVol) {
                capVolatility = swaptionVol ? swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName))
                                                  ->volatility(volFixingDate, swaptionTenor, capStrike)
                                            : Null<Real>();
            } else if (usesCapVol && capVolatility == Null<Real>()) {
                capVolatility = capFloorVol ? capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName))
                                                  ->volatility(volFixingDate, capStrike)
                                            : Null<Real>();
            }
        }
    }

    TradeCashflowReportData result;

    result.payDate = payDate;
    result.flowType = flowType;
    result.amount = effectiveAmount;
    result.currency = ccy;
    result.coupon = coupon;
    result.accrual = accrual;
    result.accrualStartDate = accrualStartDate;
    result.accrualEndDate = accrualEndDate;
    result.accruedAmount = accruedAmount * (accruedAmount == Null<Real>() ? 1.0 : multiplier);
    result.fixingDate = fixingDate;
    result.fixingValue = fixingValue;
    result.notional = notional * (notional == Null<Real>() ? 1.0 : multiplier);
    result.discountFactor = discountFactor;
    result.presentValue = presentValue;
    result.fxRateLocalBase = fxCcyBase;
    result.presentValueBase = presentValueBase;
    result.baseCurrency = baseCcy;
    result.floorStrike = floorStrike;
    result.capStrike = capStrike;
    result.floorVolatility = floorVolatility;
    result.capVolatility = capVolatility;
    result.effectiveFloorVolatility = effectiveFloorVolatility;
    result.effectiveCapVolatility = effectiveCapVolatility;

    return result;
}

} // namespace data
} // namespace ore
