/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/indexes/iborindexfixingoverride.hpp>

using namespace QuantExt;

namespace ore {
namespace data {

Leg FixedLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                              RequiredFixings& requiredFixings, const string& configuration,
                              const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    Leg leg = makeFixedLeg(data, openEndDateReplacement);
    applyIndexing(leg, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(leg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    if (data.legType() == "Fixed" && !data.isNotResetXCCY()) {

        QL_REQUIRE(!data.fxIndex().empty(), "FixedLegBuilder: need fx index for fx resetting leg");
        auto fxIndex = buildFxIndex(data.fxIndex(), data.currency(), data.foreignCurrency(), engineFactory->market(),
                                    configuration, true);

        // If no initial notional is given, all coupons including the first period will be FX linked (i.e resettable)
        Size j = 0;
        if (data.notionals().size() == 0) {
            DLOG("Building FX Resettable with unspecified domestic notional");
        } else {
            // given an initial amount (not a resettable period)
            LOG("Building FX Resettable with first domestic notional specified explicitly");
            j = 1;
        }

        for (; j < leg.size(); ++j) {
            QuantLib::ext::shared_ptr<FixedRateCoupon> coupon = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(leg[j]);
            Date fixingDate = fxIndex->fixingCalendar().advance(coupon->accrualStartDate(),
                                                                -static_cast<Integer>(fxIndex->fixingDays()), Days);
            QuantLib::ext::shared_ptr<FixedRateFXLinkedNotionalCoupon> fxLinkedCoupon =
                QuantLib::ext::make_shared<FixedRateFXLinkedNotionalCoupon>(fixingDate, data.foreignAmount(), fxIndex, coupon);
            leg[j] = fxLinkedCoupon;

            // Add the FX fixing to the required fixings
            requiredFixings.addFixingDate(fixingDate, data.fxIndex(), fxLinkedCoupon->date());
        }
    }
    return leg;
}

Leg ZeroCouponFixedLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                        RequiredFixings& requiredFixings, const string& configuration,
                                        const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    Leg leg = makeZCFixedLeg(data);
    applyIndexing(leg, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(leg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return leg;
}

Leg FloatingLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                 RequiredFixings& requiredFixings, const string& configuration,
                                 const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto floatData = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating");
    string indexName = floatData->index();
    auto index = *engineFactory->market()->iborIndex(indexName, configuration);
    
    auto ois = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index);
    Leg result;
    if (ois != nullptr) {
        QuantLib::ext::shared_ptr<OvernightIndex> idx = ois;
        if (!floatData->historicalFixings().empty())
            idx = QuantLib::ext::make_shared<OvernightIndexWithFixingOverride>(ois, floatData->historicalFixings());
        result = makeOISLeg(data, idx, engineFactory, true, openEndDateReplacement);
    } else {
        auto bma = QuantLib::ext::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index);
        if (bma != nullptr)
            result = makeBMALeg(data, bma, engineFactory, openEndDateReplacement);
        else {
            QuantLib::ext::shared_ptr<IborIndex> idx = index;
            if (!floatData->historicalFixings().empty())
                idx = QuantLib::ext::make_shared<IborIndexWithFixingOverride>(index, floatData->historicalFixings());
            result = makeIborLeg(data, idx, engineFactory, true, openEndDateReplacement);
        }
    }
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));

    // handle fx resetting Ibor leg

    if (data.legType() == "Floating" && !data.isNotResetXCCY()) {
        QL_REQUIRE(!data.fxIndex().empty(), "FloatingRateLegBuilder: need fx index for fx resetting leg");
        auto fxIndex = buildFxIndex(data.fxIndex(), data.currency(), data.foreignCurrency(), engineFactory->market(),
                                    configuration, true);       

        // If the domestic notional value is not specified, i.e. there are no notionals specified in the leg
        // data, then all coupons including the first will be FX linked. If the first coupon's FX fixing date
        // is in the past, a FX fixing will be used to determine the first domestic notional. If the first
        // coupon's FX fixing date is in the future, the first coupon's domestic notional will be determined
        // by the FX forward rate on that future fixing date.

        Size j = 0;
        if (data.notionals().size() == 0) {
            DLOG("Building FX Resettable with unspecified domestic notional");
        } else {
            // First coupon a plain floating rate coupon i.e. it is not FX linked because the initial notional is
            // known. But, we need to add it to additionalLegs_ so that we don't miss the first coupon's ibor fixing
            LOG("Building FX Resettable with first domestic notional specified explicitly");
            j = 1;
        }

        // Make the necessary FX linked floating rate coupons
        for (; j < result.size(); ++j) {
            QuantLib::ext::shared_ptr<FloatingRateCoupon> coupon =
                QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(result[j]);            
            Date fixingDate = fxIndex->fixingCalendar().advance(coupon->accrualStartDate(),
                                                                -static_cast<Integer>(fxIndex->fixingDays()), Days);
            QuantLib::ext::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon =
                QuantLib::ext::make_shared<FloatingRateFXLinkedNotionalCoupon>(fixingDate, data.foreignAmount(), 
                    fxIndex, coupon);
            // set the same pricer
            fxLinkedCoupon->setPricer(coupon->pricer());
            result[j] = fxLinkedCoupon;

            // Add the FX fixing to the required fixings
            requiredFixings.addFixingDate(fixingDate, data.fxIndex(), fxLinkedCoupon->date());
        }
    }

    return result;
}

Leg CashflowLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                 RequiredFixings& requiredFixings, const string& configuration,
                                 const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    return makeSimpleLeg(data);
}

Leg CPILegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                            RequiredFixings& requiredFixings, const string& configuration,
                            const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto cpiData = QuantLib::ext::dynamic_pointer_cast<CPILegData>(data.concreteLegData());
    QL_REQUIRE(cpiData, "Wrong LegType, expected CPI");
    string inflationIndexName = cpiData->index();
    auto index = *engineFactory->market()->zeroInflationIndex(inflationIndexName, configuration);
    Leg result = makeCPILeg(data, index, engineFactory, openEndDateReplacement);
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

Leg YYLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                           RequiredFixings& requiredFixings, const string& configuration,
                           const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto yyData = QuantLib::ext::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yyData, "Wrong LegType, expected YY");
    string inflationIndexName = yyData->index();
    bool irregularYoY = yyData->irregularYoY();
    Leg result;
    if (!irregularYoY) {
        auto index = *engineFactory->market()->yoyInflationIndex(inflationIndexName, configuration);
        result = makeYoYLeg(data, index, engineFactory, openEndDateReplacement);
        applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
        addToRequiredFixings(result,
                             QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    } else {
        auto index = *engineFactory->market()->zeroInflationIndex(inflationIndexName, configuration);
        result = makeYoYLeg(data, index, engineFactory, openEndDateReplacement);
        applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
        addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    }
    return result;
}

Leg CMSLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                            RequiredFixings& requiredFixings, const string& configuration,
                            const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto cmsData = QuantLib::ext::dynamic_pointer_cast<CMSLegData>(data.concreteLegData());
    QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");
    string swapIndexName = cmsData->swapIndex();
    auto index = *engineFactory->market()->swapIndex(swapIndexName, configuration);
    Leg result = makeCMSLeg(data, index, engineFactory, true, openEndDateReplacement);
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

Leg CMBLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                            RequiredFixings& requiredFixings, const string& configuration,
                            const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto cmbData = QuantLib::ext::dynamic_pointer_cast<CMBLegData>(data.concreteLegData());
    QL_REQUIRE(cmbData, "Wrong LegType, expected CMB");
    string indexName = cmbData->genericBond();
    auto index = parseConstantMaturityBondIndex(indexName);
    Leg result = makeCMBLeg(data, engineFactory, true, openEndDateReplacement);
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

Leg DigitalCMSLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                   RequiredFixings& requiredFixings, const string& configuration,
                                   const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto digitalCmsData = QuantLib::ext::dynamic_pointer_cast<DigitalCMSLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsData, "Wrong LegType, expected DigitalCMS");

    auto cmsData = QuantLib::ext::dynamic_pointer_cast<CMSLegData>(digitalCmsData->underlying());
    QL_REQUIRE(cmsData, "Incomplete DigitalCmsLeg, expected CMSLegData");

    string swapIndexName = digitalCmsData->underlying()->swapIndex();
    auto index = *engineFactory->market()->swapIndex(swapIndexName, configuration);
    Leg result = makeDigitalCMSLeg(data, index, engineFactory, true, openEndDateReplacement);
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

Leg CMSSpreadLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                  RequiredFixings& requiredFixings, const string& configuration,
                                  const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto cmsSpreadData = QuantLib::ext::dynamic_pointer_cast<CMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(cmsSpreadData, "Wrong LegType, expected CMSSpread");
    auto index1 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex1(), configuration);
    auto index2 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex2(), configuration);
    Leg result = makeCMSSpreadLeg(data,
                                  QuantLib::ext::make_shared<QuantLib::SwapSpreadIndex>(
                                      "CMSSpread_" + index1->familyName() + "_" + index2->familyName(), index1, index2),
                                  engineFactory, true, openEndDateReplacement);
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

Leg DigitalCMSSpreadLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                         RequiredFixings& requiredFixings, const string& configuration,
                                         const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto digitalCmsSpreadData = QuantLib::ext::dynamic_pointer_cast<DigitalCMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsSpreadData, "Wrong LegType, expected DigitalCMSSpread");

    auto cmsSpreadData = QuantLib::ext::dynamic_pointer_cast<CMSSpreadLegData>(digitalCmsSpreadData->underlying());
    QL_REQUIRE(cmsSpreadData, "Incomplete DigitalCmsSpread Leg, expected CMSSpread data");

    auto index1 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex1(), configuration);
    auto index2 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex2(), configuration);

    Leg result =
        makeDigitalCMSSpreadLeg(data,
                                QuantLib::ext::make_shared<QuantLib::SwapSpreadIndex>(
                                    "CMSSpread_" + index1->familyName() + "_" + index2->familyName(), index1, index2),
                                engineFactory, openEndDateReplacement);
    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

Leg EquityLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                               RequiredFixings& requiredFixings, const string& configuration,
                               const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto eqData = QuantLib::ext::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqData, "Wrong LegType, expected Equity");
    string eqName = eqData->eqName();
    auto eqCurve = *engineFactory->market()->equityCurve(eqName, configuration);

    // A little hacky but ensures for a dividend swap the swap value doesn't move equity price
    if (eqData->returnType() == EquityReturnType::Dividend) {
        Real spotVal = eqCurve->equitySpot()->value();
        Handle<Quote> divSpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spotVal));
        eqCurve = eqCurve->clone(divSpot, eqCurve->equityForecastCurve(), eqCurve->equityDividendCurve());
    }

    Currency dataCurrency = parseCurrencyWithMinors(data.currency());
    Currency eqCurrency;
    // Set the equity currency if provided
    if (!eqData->eqCurrency().empty())
        eqCurrency = parseCurrencyWithMinors(eqData->eqCurrency());

    if (eqCurve->currency().empty()) {
        WLOG("No equity currency set in EquityIndex for equity " << eqCurve->name());
    } else {
        // check if it equity currency not set, use from market else check if it matches what is in market
        if (!eqCurrency.empty())
            QL_REQUIRE(eqCurve->currency() == eqCurrency,
                       "Equity Currency provided does not match currency of Equity Curve");
        else
            eqCurrency = eqCurve->currency();
    }

    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex = nullptr;
    // if equity currency differs from the leg currency we need an FxIndex
    if (!eqCurrency.empty() && dataCurrency != eqCurrency) {
        QL_REQUIRE(eqData->fxIndex() != "",
                   "No FxIndex - if equity currency differs from leg currency an FxIndex must be provided");

        // An extra check, this ensures that the equity currency provided to use in the FX Index, matches that in
        // equity curves in the market, this is required as future cashflows will be in the equity curve currency
        if (!eqCurve->currency().empty() && !eqCurrency.empty()) {
            QL_REQUIRE(eqCurve->currency() == eqCurrency,
                       "Equity Currency provided does not match currency of Equity Curve");
        }

        fxIndex = buildFxIndex(eqData->fxIndex(), data.currency(), eqCurrency.code(), engineFactory->market(),
                               configuration, useXbsCurves);
    }

    Leg result = makeEquityLeg(data, eqCurve, fxIndex, openEndDateReplacement);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}

} // namespace data
} // namespace ore
