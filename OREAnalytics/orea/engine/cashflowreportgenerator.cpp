/*
Copyright (C) 2024 Quaternion Risk Management Ltd
All rights reserved.

This file is part of ORE, a free-software/open-source library
for transparent pricing and risk analysis - http://opensourcerisk.org

ORE is free software: you can redistribute it and/or modify it
under the terms of the Modified BSD License.  You should have received a
copy of the license along with this program.
The license is also available online at http://opensourcerisk.org

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
#include <qle/cashflows/typedcashflow.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
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

void appendResults( const ext::shared_ptr<ore::data::Trade>& trade,
                    const std::map<std::string, boost::any>& addResults,
                    const Real multiplier,
                    const std::string& baseCurrency,
                    QuantLib::ext::shared_ptr<ore::data::Market> market,
                    const std::string& configuration,
                    const bool includePastCashflows,
                    std::vector<TradeCashflowReportData>& result) {

    Date asof = Settings::instance().evaluationDate();

    string specificDiscount = trade->envelope().additionalField("discount_curve", false);

    Handle<YieldTermStructure> specificDiscountCurve;

    if(!specificDiscount.empty())
        specificDiscountCurve = indexOrYieldCurve(market, specificDiscount, configuration); 

    // ensures all cashFlowResults from composite trades are being accounted for
    auto lower = addResults.lower_bound("cashFlowResults");
    auto upper = addResults.lower_bound("cashFlowResults_a"); // Upper bound due to alphabetical order

    std::map<Size, Size> cashflowNumber;

    if (lower != addResults.end()) {

        for (auto cashFlowResults = lower; cashFlowResults != upper; ++cashFlowResults) {

            QL_REQUIRE(cashFlowResults->second.type() == typeid(std::vector<CashFlowResults>),
                    "internal error: cashflowResults type does not match CashFlowResults: '"
                        << cashFlowResults->second.type().name() << "'");
            std::vector<CashFlowResults> cfResults =
                boost::any_cast<std::vector<CashFlowResults>>(cashFlowResults->second);

            for (auto const& cf : cfResults) {
                string ccy = "";
                if (!cf.currency.empty()) {
                    ccy = cf.currency;
                } else if (trade->legCurrencies().size() > cf.legNumber) {
                    ccy = trade->legCurrencies()[cf.legNumber];
                } else {
                    ccy = trade->npvCurrency();
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

                if (cf.amount != Null<Real>())
                    effectiveAmount = cf.amount * multiplier;
                if (cf.discountFactor != Null<Real>())
                    discountFactor = cf.discountFactor;
                else if (!cf.currency.empty() && cf.payDate != Null<Date>() && market) {
                    auto discountCurve =  specificDiscount.empty()? market->discountCurve(cf.currency, configuration):specificDiscountCurve;
                    discountFactor = cf.payDate < asof
                                         ? 0.0
                                         : discountCurve->discount(cf.payDate);
                }
                if (cf.presentValue != Null<Real>()) {
                    presentValue = cf.presentValue * multiplier;
                } else if (effectiveAmount != Null<Real>() && discountFactor != Null<Real>()) {
                    presentValue = effectiveAmount * discountFactor;
                }
                if (cf.fxRateLocalBase != Null<Real>()) {
                    fxRateLocalBase = cf.fxRateLocalBase;
                } else if (market) {
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
}

std::vector<TradeCashflowReportData> generateCashflowReportData(const ext::shared_ptr<ore::data::Trade>& trade,
                                                                const std::string& baseCurrency,
                                                                QuantLib::ext::shared_ptr<ore::data::Market> market,
                                                                const std::string& configuration,
                                                                const bool includePastCashflows) {

    Date asof = Settings::instance().evaluationDate();

    string specificDiscount = trade->envelope().additionalField("discount_curve", false);

    Handle<YieldTermStructure> specificDiscountCurve;

    if(!specificDiscount.empty())
        specificDiscountCurve = indexOrYieldCurve(market, specificDiscount, configuration); 
    
    std::vector<TradeCashflowReportData> result;

    // Cashflow based reporting starts here
    // If trade provides cashflows as additional results, we use that information instead of the legs
    // Note that there are legs marked as mandatory that are always reported leg-wise, see below.
    // Expected flows are included in cashflow based reporting.
                                                            
    // Append cashflows to results for the instrument itself
    appendResults(trade, trade->instrument()->additionalResults(), trade->instrument()-> multiplier() * trade->instrument()->multiplier2(), 
        baseCurrency, market, configuration, includePastCashflows, result);

    // Append cashflows to the additional instruments
    auto instruments = trade->instrument()->additionalInstruments();
    for (Size i = 0; i < instruments.size(); ++i)
        appendResults(trade, instruments[i]->additionalResults(), 1.0, baseCurrency, market, configuration, includePastCashflows, result);

    // Leg based reporting starts here
    std::map<Size, Size> cashflowNumber;

    const Real multiplier = trade->instrument()->multiplier() * trade->instrument()->multiplier2();
    auto addResults = trade->instrument()->additionalResults();

    // Leg based cashflow reporting
    // If there are now cashflows, the leg based reporting is started by default
    // As an exception, mandatory legs are always reported
    bool legBasedReport = result.empty();

    const vector<Leg>& legs = trade->legs();
    for (size_t i = 0; i < legs.size(); i++) 
    {
        const auto* cashflows = &(trade->legMandatoryCashflows());
        bool mandatory = false;
        
        // If there are cashflows and the current index is in the madatory list
        if (cashflows && std::find(cashflows->begin(), cashflows->end(), i) != cashflows->end()) 
                mandatory = true;

        if(legBasedReport || mandatory)
        {
            const QuantLib::Leg& leg = legs[i];
            bool payer = trade->legPayers()[i];

            string ccy = trade->legCurrencies()[i];
            Handle<YieldTermStructure> discountCurve;
            if (market)
                discountCurve = specificDiscount.empty()? market->discountCurve(ccy, configuration):specificDiscountCurve;
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
                        } else if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(
                                    ptrFloat)) {
                            fixingValue = (on->rate() - on->effectiveSpread()) / on->gearing();
                        } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(ptrFloat)) {
                            fixingValue = (c->rate() - c->spread()) / c->gearing();
                        } else if (auto c = QuantLib::ext::dynamic_pointer_cast<
                                    QuantExt::CappedFlooredAverageONIndexedCoupon>(ptrFloat)) {
                            fixingValue =
                                (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                        } else if (auto c = QuantLib::ext::dynamic_pointer_cast<
                                    QuantExt::CappedFlooredOvernightIndexedCoupon>(ptrFloat)) {
                            fixingValue = (c->underlying()->rate() - c->underlying()->effectiveSpread()) /
                                        c->underlying()->gearing();
                        } else if (auto c =
                                    QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageBMACoupon>(
                                        ptrFloat)) {
                            fixingValue =
                                (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                        } else if (auto sp =
                                    QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(ptrFloat)) {
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
                        auto dcurve=specificDiscount.empty()? discountCurve: specificDiscountCurve;
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
                            } else if (auto cms =
                                        QuantLib::ext::dynamic_pointer_cast<DurationAdjustedCmsCoupon>(tmp->underlying())) {
                                swaptionTenor = cms->swapIndex()->tenor();
                                qlIndexName = cms->swapIndex()->iborIndex()->name();
                                usesSwaptionVol = true;
                            } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(tmp->underlying())) {
                                qlIndexName = ibor->index()->name();
                                usesCapVol = true;
                            }
                        } else if (auto tmp =
                                    QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c)) {
                            floorStrike = tmp->effectiveFloor();
                            capStrike = tmp->effectiveCap();
                            volFixingDate = tmp->underlying()->fixingDates().front();
                            qlIndexName = tmp->index()->name();
                            usesCapVol = true;
                            if (floorStrike != Null<Real>())
                                effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                            if (capStrike != Null<Real>())
                                effectiveCapVolatility = tmp->effectiveCapletVolatility();
                        } else if (auto tmp =
                                    QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(c)) {
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
                                    capVolatility =
                                        market
                                            ->swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                        configuration)
                                            ->volatility(volFixingDate, swaptionTenor, capStrike);
                                } else if (usesCapVol && capVolatility == Null<Real>()) {
                                    capVolatility =
                                        market
                                            ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                        configuration)
                                            ->volatility(volFixingDate, capStrike);
                                }
                            }
                        }
                    }

                    result.emplace_back();
                    result.back().cashflowNo = j + 1;
                    result.back().legNo = i ;
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
    }
    
    return result;
}

} // namespace analytics
} // namespace ore
