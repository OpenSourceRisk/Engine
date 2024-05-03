/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/utilities/databuilders.hpp>

#include <ored/model/calibrationinstrumentfactory.hpp>
#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/calibrationinstruments/yoycapfloor.hpp>
#include <ored/model/calibrationinstruments/yoyswap.hpp>

#include <ored/portfolio/ascot.hpp>
#include <ored/portfolio/asianoption.hpp>
#include <ored/portfolio/balanceguaranteedswap.hpp>
#include <ored/portfolio/barrieroption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/bondposition.hpp>
#include <ored/portfolio/bondrepo.hpp>
#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/builders/ascot.hpp>
#include <ored/portfolio/builders/asianoption.hpp>
#include <ored/portfolio/builders/balanceguaranteedswap.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/bondoption.hpp>
#include <ored/portfolio/builders/bondrepo.hpp>
#include <ored/portfolio/builders/bondtotalreturnswap.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/capflooredaveragebmacouponleg.hpp>
#include <ored/portfolio/builders/capflooredaverageonindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredcpileg.hpp>
#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/capfloorednonstandardyoyleg.hpp>
#include <ored/portfolio/builders/capflooredovernightindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredyoyleg.hpp>
#include <ored/portfolio/builders/cbo.hpp>
#include <ored/portfolio/builders/cdo.hpp>
#include <ored/portfolio/builders/cliquetoption.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/portfolio/builders/commodityapo.hpp>
#include <ored/portfolio/builders/commodityapomodelbuilder.hpp>
#include <ored/portfolio/builders/commodityasianoption.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/builders/commodityspreadoption.hpp>
#include <ored/portfolio/builders/commodityswap.hpp>
#include <ored/portfolio/builders/commodityswaption.hpp>
#include <ored/portfolio/builders/convertiblebond.hpp>
#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/builders/creditdefaultswapoption.hpp>
#include <ored/portfolio/builders/creditlinkedswap.hpp>
#include <ored/portfolio/builders/currencyswap.hpp>
#include <ored/portfolio/builders/deltagammaengines.hpp>
#include <ored/portfolio/builders/durationadjustedcms.hpp>
#include <ored/portfolio/builders/equityasianoption.hpp>
#include <ored/portfolio/builders/equitybarrieroption.hpp>
#include <ored/portfolio/builders/equitycompositeoption.hpp>
#include <ored/portfolio/builders/equitydigitaloption.hpp>
#include <ored/portfolio/builders/equitydoublebarrieroption.hpp>
#include <ored/portfolio/builders/equitydoubletouchoption.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/builders/equityfuturesoption.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/builders/equityoutperformanceoption.hpp>
#include <ored/portfolio/builders/equitytouchoption.hpp>
#include <ored/portfolio/builders/flexiswap.hpp>
#include <ored/portfolio/builders/formulabasedcoupon.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/builders/fxasianoption.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/builders/fxdoublebarrieroption.hpp>
#include <ored/portfolio/builders/fxdoubletouchoption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/fxtouchoption.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/builders/multilegoption.hpp>
#include <ored/portfolio/builders/pairwisevarianceswap.hpp>
#include <ored/portfolio/builders/quantoequityoption.hpp>
#include <ored/portfolio/builders/quantovanillaoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>
#include <ored/portfolio/builders/varianceswap.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/callableswap.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/cbo.hpp>
#include <ored/portfolio/cdo.hpp>
#include <ored/portfolio/cliquetoption.hpp>
#include <ored/portfolio/commodityapo.hpp>
#include <ored/portfolio/commoditydigitalapo.hpp>
#include <ored/portfolio/commoditydigitaloption.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/commodityoptionstrip.hpp>
#include <ored/portfolio/commodityposition.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/commodityswaption.hpp>
#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/compositetrade.hpp>
#include <ored/portfolio/convertiblebond.hpp>
#include <ored/portfolio/convertiblebondreferencedata.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswapoption.hpp>
#include <ored/portfolio/creditlinkedswap.hpp>
#include <ored/portfolio/crosscurrencyswap.hpp>
#include <ored/portfolio/durationadjustedcmslegbuilder.hpp>
#include <ored/portfolio/durationadjustedcmslegdata.hpp>
#include <ored/portfolio/equitybarrieroption.hpp>
#include <ored/portfolio/equityderivative.hpp>
#include <ored/portfolio/equitydigitaloption.hpp>
#include <ored/portfolio/equitydoublebarrieroption.hpp>
#include <ored/portfolio/equitydoubletouchoption.hpp>
#include <ored/portfolio/equityeuropeanbarrieroption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <ored/portfolio/equityfxlegbuilder.hpp>
#include <ored/portfolio/equityfxlegdata.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/equityoutperformanceoption.hpp>
#include <ored/portfolio/equityposition.hpp>
#include <ored/portfolio/equityswap.hpp>
#include <ored/portfolio/equitytouchoption.hpp>
#include <ored/portfolio/failedtrade.hpp>
#include <ored/portfolio/flexiswap.hpp>
#include <ored/portfolio/formulabasedlegbuilder.hpp>
#include <ored/portfolio/formulabasedlegdata.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ored/portfolio/fxaverageforward.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <ored/portfolio/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/portfolio/fxdoublebarrieroption.hpp>
#include <ored/portfolio/fxdoubletouchoption.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapdata.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/indexing.hpp>
#include <ored/portfolio/inflationswap.hpp>
#include <ored/portfolio/instrumentwrapper.hpp>
#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/legdatafactory.hpp>
#include <ored/portfolio/multilegoption.hpp>
#include <ored/portfolio/pairwisevarianceswap.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/referencedatafactory.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/trs.hpp>
#include <ored/portfolio/trsunderlyingbuilder.hpp>
#include <ored/portfolio/trswrapper.hpp>
#include <ored/portfolio/vanillaoption.hpp>
#include <ored/portfolio/varianceswap.hpp>
// scripted products
#include <ored/portfolio/accumulator.hpp>
#include <ored/portfolio/autocallable_01.hpp>
#include <ored/portfolio/basketoption.hpp>
#include <ored/portfolio/basketvarianceswap.hpp>
#include <ored/portfolio/bestentryoption.hpp>
#include <ored/portfolio/builders/asianoption.hpp>
#include <ored/portfolio/builders/cliquetoption.hpp>
#include <ored/portfolio/builders/riskparticipationagreement.hpp>
#include <ored/portfolio/builders/scriptedtrade.hpp>
#include <ored/portfolio/doubledigitaloption.hpp>
#include <ored/portfolio/europeanoptionbarrier.hpp>
#include <ored/portfolio/genericbarrieroption.hpp>
#include <ored/portfolio/knockoutswap.hpp>
#include <ored/portfolio/performanceoption_01.hpp>
#include <ored/portfolio/rainbowoption.hpp>
#include <ored/portfolio/riskparticipationagreement.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/tarf.hpp>
#include <ored/portfolio/windowbarrieroption.hpp>
#include <ored/portfolio/worstofbasketswap.hpp>

#include <qle/math/basiccpuenvironment.hpp>
#include <qle/math/openclenvironment.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ore::data {

void dataBuilders() {

    static boost::shared_mutex mutex;
    static bool hasRun = false;

    boost::unique_lock<boost::shared_mutex> lock(mutex);

    if (hasRun)
        return;

    hasRun = true;

    ORE_REGISTER_LEG_DATA("Cashflow", CashflowData, false)
    ORE_REGISTER_LEG_DATA("Fixed", FixedLegData, false)
    ORE_REGISTER_LEG_DATA("ZeroCouponFixed", ZeroCouponFixedLegData, false)
    ORE_REGISTER_LEG_DATA("Floating", FloatingLegData, false)
    ORE_REGISTER_LEG_DATA("CPI", CPILegData, false)
    ORE_REGISTER_LEG_DATA("YY", YoYLegData, false)
    ORE_REGISTER_LEG_DATA("CMS", CMSLegData, false)
    ORE_REGISTER_LEG_DATA("CMB", CMBLegData, false)
    ORE_REGISTER_LEG_DATA("DigitalCMS", DigitalCMSLegData, false)
    ORE_REGISTER_LEG_DATA("CMSSpread", CMSSpreadLegData, false)
    ORE_REGISTER_LEG_DATA("DigitalCMSSpread", DigitalCMSSpreadLegData, false)
    ORE_REGISTER_LEG_DATA("Equity", EquityLegData, false)
    ORE_REGISTER_LEG_DATA("CommodityFixed", CommodityFixedLegData, false)
    ORE_REGISTER_LEG_DATA("CommodityFloating", CommodityFloatingLegData, false)
    ORE_REGISTER_LEG_DATA("DurationAdjustedCMS", DurationAdjustedCmsLegData, false)
    ORE_REGISTER_LEG_DATA("EquityMargin", EquityMarginLegData, false)
    ORE_REGISTER_LEG_DATA("FormulaBased", FormulaBasedLegData, false)

    ORE_REGISTER_CALIBRATION_INSTRUMENT("CpiCapFloor", CpiCapFloor, false)
    ORE_REGISTER_CALIBRATION_INSTRUMENT("YoYCapFloor", YoYCapFloor, false)
    ORE_REGISTER_CALIBRATION_INSTRUMENT("YoYSwap", YoYSwap, false)

    ORE_REGISTER_REFERENCE_DATUM("Bond", BondReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("CreditIndex", CreditIndexReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("EquityIndex", EquityIndexReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("CurrencyHedgedEquityIndex", CurrencyHedgedEquityIndexReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("PortfolioBasket", PortfolioBasketReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("CommodityIndex", CommodityIndexReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("Credit", CreditReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("Equity", EquityReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("BondBasket", BondBasketReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("ConvertibleBond", ConvertibleBondReferenceDatum, false)
    ORE_REGISTER_REFERENCE_DATUM("CBO", CboReferenceDatum, false)

    ORE_REGISTER_BOND_BUILDER("Bond", VanillaBondBuilder, false)
    ORE_REGISTER_BOND_BUILDER("ConvertibleBond", ConvertibleBondBuilder, false)

    ORE_REGISTER_TRADE_BUILDER("CrossCurrencySwap", CrossCurrencySwap, false)
    ORE_REGISTER_TRADE_BUILDER("CommoditySpreadOption", CommoditySpreadOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityFutureOption", EquityFutureOption, false)
    ORE_REGISTER_TRADE_BUILDER("BondTRS", BondTRS, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityOption", CommodityOption, false)
    ORE_REGISTER_TRADE_BUILDER("CapFloor", CapFloor, false)
    ORE_REGISTER_TRADE_BUILDER("FxDigitalOption", FxDigitalOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommoditySwaption", CommoditySwaption, false)
    ORE_REGISTER_TRADE_BUILDER("FxDigitalBarrierOption", FxDigitalBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("ForwardRateAgreement", ForwardRateAgreement, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityDigitalAveragePriceOption", CommodityDigitalAveragePriceOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommoditySwap", CommoditySwap, false)
    ORE_REGISTER_TRADE_BUILDER("EquitySwap", EquitySwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxForward", FxForward, false)
    ORE_REGISTER_TRADE_BUILDER("BondRepo", BondRepo, false)
    ORE_REGISTER_TRADE_BUILDER("FxAverageForward", FxAverageForward, false)
    ORE_REGISTER_TRADE_BUILDER("FxEuropeanBarrierOption", FxEuropeanBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxTouchOption", FxTouchOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityAsianOption", EquityAsianOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxAsianOption", FxAsianOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityAsianOption", CommodityAsianOption, false)
    ORE_REGISTER_TRADE_BUILDER("Swaption", Swaption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityVarianceSwap", EqVarSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxVarianceSwap", FxVarSwap, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityVarianceSwap", ComVarSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxDoubleTouchOption", FxDoubleTouchOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxDoubleBarrierOption", FxDoubleBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityBarrierOption", EquityBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxSwap", FxSwap, false)
    ORE_REGISTER_TRADE_BUILDER("EquityTouchOption", EquityTouchOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityDigitalOption", EquityDigitalOption, false)
    ORE_REGISTER_TRADE_BUILDER("CompositeTrade", CompositeTrade, false)
    ORE_REGISTER_TRADE_BUILDER("MultiLegOption", MultiLegOption, false)
    ORE_REGISTER_TRADE_BUILDER("Swap", Swap, false)
    ORE_REGISTER_TRADE_BUILDER("IndexCreditDefaultSwap", IndexCreditDefaultSwap, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityForward", CommodityForward, false)
    ORE_REGISTER_TRADE_BUILDER("EquityCliquetOption", EquityCliquetOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityDigitalOption", CommodityDigitalOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityForward", EquityForward, false)
    ORE_REGISTER_TRADE_BUILDER("IndexCreditDefaultSwapOption", IndexCreditDefaultSwapOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityAveragePriceOption", CommodityAveragePriceOption, false)
    ORE_REGISTER_TRADE_BUILDER("CreditDefaultSwapOption", CreditDefaultSwapOption, false)
    ORE_REGISTER_TRADE_BUILDER("Failed", FailedTrade, false)
    ORE_REGISTER_TRADE_BUILDER("ForwardBond", ForwardBond, false)
    ORE_REGISTER_TRADE_BUILDER("EquityDoubleTouchOption", EquityDoubleTouchOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityOptionStrip", CommodityOptionStrip, false)
    ORE_REGISTER_TRADE_BUILDER("SyntheticCDO", SyntheticCDO, false)
    ORE_REGISTER_TRADE_BUILDER("Bond", Bond, false)
    ORE_REGISTER_TRADE_BUILDER("CreditLinkedSwap", CreditLinkedSwap, false)
    ORE_REGISTER_TRADE_BUILDER("EquityEuropeanBarrierOption", EquityEuropeanBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("InflationSwap", InflationSwap, false)
    ORE_REGISTER_TRADE_BUILDER("EquityDoubleBarrierOption", EquityDoubleBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("BondOption", BondOption, false)
    ORE_REGISTER_TRADE_BUILDER("CreditDefaultSwap", CreditDefaultSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxKIKOBarrierOption", FxKIKOBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxBarrierOption", FxBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityOption", EquityOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxOption", FxOption, false)
    ORE_REGISTER_TRADE_BUILDER("CBO", CBO, false)

    ORE_REGISTER_TRADE_BUILDER("TotalReturnSwap", TRS, false)
    ORE_REGISTER_TRADE_BUILDER("ContractForDifference", CFD, false)
    ORE_REGISTER_TRADE_BUILDER("BondPosition", BondPosition, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityPosition", CommodityPosition, false)
    ORE_REGISTER_TRADE_BUILDER("EquityPosition", EquityPosition, false)
    ORE_REGISTER_TRADE_BUILDER("EquityOptionPosition", EquityOptionPosition, false)
    ORE_REGISTER_TRADE_BUILDER("Ascot", Ascot, false)
    ORE_REGISTER_TRADE_BUILDER("ConvertibleBond", ConvertibleBond, false)

    ORE_REGISTER_TRADE_BUILDER("ScriptedTrade", ScriptedTrade, false)
    ORE_REGISTER_TRADE_BUILDER("Autocallable_01", Autocallable_01, false)
    ORE_REGISTER_TRADE_BUILDER("EquityWindowBarrierOption", EquityWindowBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxWindowBarrierOption", FxWindowBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityWindowBarrierOption", CommodityWindowBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityRainbowOption", EquityRainbowOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxRainbowOption", FxRainbowOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityRainbowOption", CommodityRainbowOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityGenericBarrierOption", EquityGenericBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxGenericBarrierOption", FxGenericBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityGenericBarrierOption", CommodityGenericBarrierOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityBestEntryOption", EquityBestEntryOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxBestEntryOption", FxBestEntryOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityBestEntryOption", CommodityBestEntryOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityAccumulator", EquityAccumulator, false)
    ORE_REGISTER_TRADE_BUILDER("FxAccumulator", FxAccumulator, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityAccumulator", CommodityAccumulator, false)
    ORE_REGISTER_TRADE_BUILDER("EquityBasketVarianceSwap", EquityBasketVarianceSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxBasketVarianceSwap", FxBasketVarianceSwap, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityBasketVarianceSwap", CommodityBasketVarianceSwap, false)
    ORE_REGISTER_TRADE_BUILDER("EquityTaRF", EquityTaRF, false)
    ORE_REGISTER_TRADE_BUILDER("FxTaRF", FxTaRF, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityTaRF", CommodityTaRF, false)
    ORE_REGISTER_TRADE_BUILDER("EquityWorstOfBasketSwap", EquityWorstOfBasketSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxWorstOfBasketSwap", FxWorstOfBasketSwap, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityWorstOfBasketSwap", CommodityWorstOfBasketSwap, false)
    ORE_REGISTER_TRADE_BUILDER("EquityBasketOption", EquityBasketOption, false)
    ORE_REGISTER_TRADE_BUILDER("FxBasketOption", FxBasketOption, false)
    ORE_REGISTER_TRADE_BUILDER("CommodityBasketOption", CommodityBasketOption, false)
    ORE_REGISTER_TRADE_BUILDER("EuropeanOptionBarrier", EuropeanOptionBarrier, false)
    ORE_REGISTER_TRADE_BUILDER("KnockOutSwap", KnockOutSwap, false)
    ORE_REGISTER_TRADE_BUILDER("DoubleDigitalOption", DoubleDigitalOption, false)
    ORE_REGISTER_TRADE_BUILDER("PerformanceOption_01", PerformanceOption_01, false)
    ORE_REGISTER_TRADE_BUILDER("RiskParticipationAgreement", RiskParticipationAgreement, false)
    ORE_REGISTER_TRADE_BUILDER("EquityOutperformanceOption", EquityOutperformanceOption, false)
    ORE_REGISTER_TRADE_BUILDER("EquityPairwiseVarianceSwap", EqPairwiseVarSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FxPairwiseVarianceSwap", FxPairwiseVarSwap, false)

    ORE_REGISTER_TRADE_BUILDER("BalanceGuaranteedSwap", BalanceGuaranteedSwap, false)
    ORE_REGISTER_TRADE_BUILDER("CallableSwap", CallableSwap, false)
    ORE_REGISTER_TRADE_BUILDER("FlexiSwap", FlexiSwap, false)

    ORE_REGISTER_LEGBUILDER("CommodityFixedLegBuilder", CommodityFixedLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("CommodityFloatingLegBuilder", CommodityFloatingLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("DurationAdjustedCmsLegBuilder", DurationAdjustedCmsLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("FixedLegBuilder", FixedLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("ZeroCouponFixedLegBuilder", ZeroCouponFixedLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("FloatingLegBuilder", FloatingLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("CashflowLegBuilder", CashflowLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("CPILegBuilder", CPILegBuilder, false)
    ORE_REGISTER_LEGBUILDER("YYLegBuilder", YYLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("CMSLegBuilder", CMSLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("CMBLegBuilder", CMBLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("DigitalCMSLegBuilder", DigitalCMSLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("CMSSpreadLegBuilder", CMSSpreadLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("DigitalCMSSpreadLegBuilder", DigitalCMSSpreadLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("EquityLegBuilder", EquityLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("EquityMarginLegBuilder", EquityMarginLegBuilder, false)
    ORE_REGISTER_LEGBUILDER("FormulaBasedLegBuilder", FormulaBasedLegBuilder, false)

    ORE_REGISTER_AMC_ENGINE_BUILDER(CamAmcCurrencySwapEngineBuilder, false)
    ORE_REGISTER_AMC_ENGINE_BUILDER(LGMAmcSwaptionEngineBuilder, false)
    ORE_REGISTER_AMC_ENGINE_BUILDER(CamAmcSwapEngineBuilder, false)
    ORE_REGISTER_AMC_ENGINE_BUILDER(CamAmcFxOptionEngineBuilder, false)
    ORE_REGISTER_AMC_ENGINE_BUILDER(CamAmcFxForwardEngineBuilder, false)
    ORE_REGISTER_AMC_ENGINE_BUILDER(CamAmcMultiLegOptionEngineBuilder, false)
    ORE_REGISTER_AMC_ENGINE_BUILDER(ScriptedTradeEngineBuilder, false)

    ORE_REGISTER_AMCCG_ENGINE_BUILDER(ScriptedTradeEngineBuilder, false)

    ORE_REGISTER_ENGINE_BUILDER(CommoditySpreadOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CpiCapFloorEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityFutureEuropeanOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(DiscountingBondTRSEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionMCDAAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionMCDAASEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionMCDGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionADGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionADGASEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionACGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanAsianOptionTWEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanForwardOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityEuropeanCSOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityAmericanOptionFDEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityAmericanOptionBAWEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFloorEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxDigitalOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxDigitalCSOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommoditySwaptionAnalyticalEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommoditySwaptionMonteCarloEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxDigitalBarrierOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommoditySwapEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanCompositeEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxForwardEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(DiscountingBondRepoEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(AccrualBondRepoEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredOvernightIndexedCouponLegEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredCpiLegCouponEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredCpiLegCashFlowEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxTouchOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EuropeanSwaptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(LGMGridSwaptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(LGMFDSwaptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(LGMMCSwaptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(VarSwapEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxDoubleTouchOptionAnalyticEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxDoubleBarrierOptionAnalyticEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityBarrierOptionAnalyticEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityBarrierOptionFDEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityTouchOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredYoYLegEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityDigitalOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionMCDAAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionMCDAASEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionMCDGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionADGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionADGASEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionACGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanAsianOptionTWEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(SwapEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(SwapEngineBuilderOptimised, false)
    ORE_REGISTER_ENGINE_BUILDER(CrossCurrencySwapEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(MidPointIndexCdsEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(MidPointCdsMultiStateEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityForwardEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionMCDAAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionMCDAASEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionMCDGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionADGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionADGASEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionACGAPEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanAsianOptionTWEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CmsSpreadCouponPricerBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(AnalyticHaganCmsCouponPricerBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(NumericalHaganCmsCouponPricerBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(LinearTSRCmsCouponPricerBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityForwardEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BlackIndexCdsOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(NumericalIntegrationIndexCdsOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityApoAnalyticalEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CommodityApoMonteCarloEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BlackCdsOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(YoYCapFloorEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredAverageBMACouponLegEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredAverageONIndexedCouponLegEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(DiscountingForwardBondEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityDoubleTouchOptionAnalyticEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredIborLegEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(LinearTsrDurationAdjustedCmsCouponPricerBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(GaussCopulaBucketingCdoEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BondDiscountingEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BondMultiStateDiscountingEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CreditLinkedSwapEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityDoubleBarrierOptionAnalyticEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BondOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(MidPointCdsEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxBarrierOptionAnalyticEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxBarrierOptionFDEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanCSOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityAmericanOptionFDEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityAmericanOptionBAWEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CapFlooredNonStandardYoYLegEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(QuantoEquityEuropeanOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanCSOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxAmericanOptionFDEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FxAmericanOptionBAWEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(AscotIntrinsicEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(ConvertibleBondFDDefaultableEquityJumpDiffusionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CboMCEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(CamMcMultiLegOptionEngineBuilder, false)

    ORE_REGISTER_ENGINE_BUILDER(ScriptedTradeEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(AsianOptionScriptedEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(RiskParticipationAgreementBlackEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(RiskParticipationAgreementXCcyBlackEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(RiskParticipationAgreementSwapLGMGridEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(RiskParticipationAgreementTLockLGMGridEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityCliquetOptionMcScriptEngineBuilder, false)

    ORE_REGISTER_ENGINE_BUILDER(FormulaBasedCouponPricerBuilder, false)

    ORE_REGISTER_ENGINE_BUILDER(SwapEngineBuilderDeltaGamma, false)
    ORE_REGISTER_ENGINE_BUILDER(CurrencySwapEngineBuilderDeltaGamma, false)
    ORE_REGISTER_ENGINE_BUILDER(FxEuropeanOptionEngineBuilderDeltaGamma, false)
    ORE_REGISTER_ENGINE_BUILDER(EquityEuropeanOptionEngineBuilderDeltaGamma, false)
    ORE_REGISTER_ENGINE_BUILDER(FxForwardEngineBuilderDeltaGamma, false)

    ORE_REGISTER_ENGINE_BUILDER(EquityOutperformanceOptionEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(PairwiseVarSwapEngineBuilder, false)

    ORE_REGISTER_ENGINE_BUILDER(FlexiSwapDiscountingEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(FlexiSwapLGMGridEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BalanceGuaranteedSwapDiscountingEngineBuilder, false)
    ORE_REGISTER_ENGINE_BUILDER(BalanceGuaranteedSwapFlexiSwapLGMGridEngineBuilder, false)

    ORE_REGISTER_TRS_UNDERLYING_BUILDER("Bond", BondTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("ForwardBond", ForwardBondTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("EquityPosition", EquityPositionTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("CommodityPosition", CommodityPositionTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("EquityOptionPosition", EquityOptionPositionTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("BondPosition", BondPositionTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("Derivative", DerivativeTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("ConvertibleBond", ConvertibleBondTrsUnderlyingBuilder, false)
    ORE_REGISTER_TRS_UNDERLYING_BUILDER("CBO", CBOTrsUnderlyingBuilder, false)

    ORE_REGISTER_COMPUTE_FRAMEWORK_CREATOR("OpenCL", QuantExt::OpenClFramework, false);
    ORE_REGISTER_COMPUTE_FRAMEWORK_CREATOR("BasicCpu", QuantExt::BasicCpuFramework, false);
}

} // namespace ore::data
