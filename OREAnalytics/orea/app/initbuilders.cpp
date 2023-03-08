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

#include <orea/app/initbuilders.hpp>

#include <ored/model/calibrationinstrumentfactory.hpp>
#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/calibrationinstruments/yoycapfloor.hpp>
#include <ored/model/calibrationinstruments/yoyswap.hpp>
#include <ored/portfolio/asianoption.hpp>
#include <ored/portfolio/barrieroption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/bondrepo.hpp>
#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/builders/asianoption.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/bondoption.hpp>
#include <ored/portfolio/builders/bondrepo.hpp>
#include <ored/portfolio/builders/bondtotalreturnswap.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/capflooredaverageonindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredcpileg.hpp>
#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/capfloorednonstandardyoyleg.hpp>
#include <ored/portfolio/builders/capflooredovernightindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredyoyleg.hpp>
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
#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/builders/creditdefaultswapoption.hpp>
#include <ored/portfolio/builders/creditlinkedswap.hpp>
#include <ored/portfolio/builders/currencyswap.hpp>
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
#include <ored/portfolio/builders/equitytouchoption.hpp>
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
#include <ored/portfolio/builders/quantoequityoption.hpp>
#include <ored/portfolio/builders/quantovanillaoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>
#include <ored/portfolio/builders/varianceswap.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/capfloor.hpp>
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
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/commodityswaption.hpp>
#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/compositetrade.hpp>
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
#include <ored/portfolio/equityswap.hpp>
#include <ored/portfolio/equitytouchoption.hpp>
#include <ored/portfolio/failedtrade.hpp>
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
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/referencedatafactory.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/vanillaoption.hpp>
#include <ored/portfolio/varianceswap.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

#define ORE_REGISTER_LEG_DATA(NAME, CLASS)                                                                                       \
    ore::data::LegDataFactory::instance().addBuilder(NAME, &ore::data::createLegData<CLASS>);

#define ORE_REGISTER_CALIBRATION_INSTRUMENT(NAME, CLASS)                                                                             \
    ore::data::CalibrationInstrumentFactory::instance().addBuilder(NAME,                                               \
                                                                   &ore::data::createCalibrationInstrument<CLASS>);

#define ORE_REGISTER_REFERENCE_DATUM(NAME, CLASS)                                                                                      \
    ore::data::ReferenceDatumFactory::instance().addBuilder(                                                           \
        NAME, &ore::data::createReferenceDatumBuilder<ore::data::ReferenceDatumBuilder<CLASS>>);

#define ORE_REGISTER_BOND_BUILDER(NAME, CLASS) ore::data::BondFactory::instance().addBuilder(NAME, boost::make_shared<CLASS>());

#define ORE_REGISTER_TRADE_BUILDER(NAME, CLASS)                                                                                  \
    ore::data::TradeFactory::instance().addBuilder(NAME, boost::make_shared<ore::data::TradeBuilder<CLASS>>());

#define ORE_REGISTER_LEGBUILDER(NAME, CLASS)                                                                                    \
    ore::data::EngineBuilderFactory::instance().addLegBuilder([]() { return boost::make_shared<CLASS>(); });

#define ORE_REGISTER_AMC_ENGINE_BUILDER(CLASS)                                                                                  \
    ore::data::EngineBuilderFactory::instance().addAmcEngineBuilder(                                                   \
        [](const boost::shared_ptr<QuantExt::CrossAssetModel>& cam, const std::vector<ore::data::Date>& grid) {        \
            return boost::make_shared<CLASS>(cam, grid);                                                               \
        });

#define ORE_REGISTER_ENGINE_BUILDER(CLASS)                                                                                      \
    ore::data::EngineBuilderFactory::instance().addEngineBuilder([]() { return boost::make_shared<CLASS>(); });

namespace ore::analytics {

void initBuilders() {

    static boost::shared_mutex mutex;
    static bool hasRun = false;

    boost::unique_lock<boost::shared_mutex> lock(mutex);

    if (hasRun)
        return;

    hasRun = true;

    ORE_REGISTER_LEG_DATA("Cashflow", ore::data::CashflowData)
    ORE_REGISTER_LEG_DATA("Fixed", ore::data::FixedLegData)
    ORE_REGISTER_LEG_DATA("ZeroCouponFixed", ore::data::ZeroCouponFixedLegData)
    ORE_REGISTER_LEG_DATA("Floating", ore::data::FloatingLegData)
    ORE_REGISTER_LEG_DATA("CPI", ore::data::CPILegData)
    ORE_REGISTER_LEG_DATA("YY", ore::data::YoYLegData)
    ORE_REGISTER_LEG_DATA("CMS", ore::data::CMSLegData)
    ORE_REGISTER_LEG_DATA("CMB", ore::data::CMBLegData)
    ORE_REGISTER_LEG_DATA("DigitalCMS", ore::data::DigitalCMSLegData)
    ORE_REGISTER_LEG_DATA("CMSSpread", ore::data::CMSSpreadLegData)
    ORE_REGISTER_LEG_DATA("DigitalCMSSpread", ore::data::DigitalCMSSpreadLegData)
    ORE_REGISTER_LEG_DATA("Equity", ore::data::EquityLegData)
    ORE_REGISTER_LEG_DATA("CommodityFixed", ore::data::CommodityFixedLegData)
    ORE_REGISTER_LEG_DATA("CommodityFloating", ore::data::CommodityFloatingLegData)
    ORE_REGISTER_LEG_DATA("DurationAdjustedCMS", ore::data::DurationAdjustedCmsLegData)
    ORE_REGISTER_LEG_DATA("EquityMargin", ore::data::EquityMarginLegData)

    ORE_REGISTER_CALIBRATION_INSTRUMENT("CpiCapFloor", ore::data::CpiCapFloor)
    ORE_REGISTER_CALIBRATION_INSTRUMENT("YoYCapFloor", ore::data::YoYCapFloor)
    ORE_REGISTER_CALIBRATION_INSTRUMENT("YoYSwap", ore::data::YoYSwap)

    ORE_REGISTER_REFERENCE_DATUM("Bond", ore::data::BondReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("CreditIndex", ore::data::CreditIndexReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("EquityIndex", ore::data::EquityIndexReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("CurrencyHedgedEquityIndex", ore::data::CurrencyHedgedEquityIndexReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("Credit", ore::data::CreditReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("Equity", ore::data::EquityReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("BondBasket", ore::data::BondBasketReferenceDatum)
    ORE_REGISTER_REFERENCE_DATUM("ConvertibleBond", ore::data::BondBasketReferenceDatum)

    ORE_REGISTER_BOND_BUILDER("Bond", ore::data::VanillaBondBuilder)

    ORE_REGISTER_TRADE_BUILDER("CrossCurrencySwap", ore::data::CrossCurrencySwap)
    ORE_REGISTER_TRADE_BUILDER("CommoditySpreadOption", ore::data::CommoditySpreadOption)
    ORE_REGISTER_TRADE_BUILDER("EquityFutureOption", ore::data::EquityFutureOption)
    ORE_REGISTER_TRADE_BUILDER("BondTRS", ore::data::BondTRS)
    ORE_REGISTER_TRADE_BUILDER("CommodityOption", ore::data::CommodityOption)
    ORE_REGISTER_TRADE_BUILDER("CapFloor", ore::data::CapFloor)
    ORE_REGISTER_TRADE_BUILDER("FxDigitalOption", ore::data::FxDigitalOption)
    ORE_REGISTER_TRADE_BUILDER("CommoditySwaption", ore::data::CommoditySwaption)
    ORE_REGISTER_TRADE_BUILDER("FxDigitalBarrierOption", ore::data::FxDigitalBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("ForwardRateAgreement", ore::data::ForwardRateAgreement)
    ORE_REGISTER_TRADE_BUILDER("CommodityDigitalAveragePriceOption", ore::data::CommodityDigitalAveragePriceOption)
    ORE_REGISTER_TRADE_BUILDER("CommoditySwap", ore::data::CommoditySwap)
    ORE_REGISTER_TRADE_BUILDER("EquitySwap", ore::data::EquitySwap)
    ORE_REGISTER_TRADE_BUILDER("FxForward", ore::data::FxForward)
    ORE_REGISTER_TRADE_BUILDER("BondRepo", ore::data::BondRepo)
    ORE_REGISTER_TRADE_BUILDER("FxAverageForward", ore::data::FxAverageForward)
    ORE_REGISTER_TRADE_BUILDER("FxEuropeanBarrierOption", ore::data::FxEuropeanBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("FxTouchOption", ore::data::FxTouchOption)
    ORE_REGISTER_TRADE_BUILDER("EquityAsianOption", ore::data::EquityAsianOption)
    ORE_REGISTER_TRADE_BUILDER("FxAsianOption", ore::data::FxAsianOption)
    ORE_REGISTER_TRADE_BUILDER("CommodityAsianOption", ore::data::CommodityAsianOption)
    ORE_REGISTER_TRADE_BUILDER("Swaption", ore::data::Swaption)
    ORE_REGISTER_TRADE_BUILDER("EquityVarianceSwap", ore::data::EqVarSwap)
    ORE_REGISTER_TRADE_BUILDER("FxVarianceSwap", ore::data::FxVarSwap)
    ORE_REGISTER_TRADE_BUILDER("CommodityVarianceSwap", ore::data::ComVarSwap)
    ORE_REGISTER_TRADE_BUILDER("FxDoubleTouchOption", ore::data::FxDoubleTouchOption)
    ORE_REGISTER_TRADE_BUILDER("FxDoubleBarrierOption", ore::data::FxDoubleBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("EquityBarrierOption", ore::data::EquityBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("FxSwap", ore::data::FxSwap)
    ORE_REGISTER_TRADE_BUILDER("EquityTouchOption", ore::data::EquityTouchOption)
    ORE_REGISTER_TRADE_BUILDER("EquityDigitalOption", ore::data::EquityDigitalOption)
    ORE_REGISTER_TRADE_BUILDER("CompositeTrade", ore::data::CompositeTrade)
    ORE_REGISTER_TRADE_BUILDER("MultiLegOption", ore::data::MultiLegOption)
    ORE_REGISTER_TRADE_BUILDER("Swap", ore::data::Swap)
    ORE_REGISTER_TRADE_BUILDER("IndexCreditDefaultSwap", ore::data::IndexCreditDefaultSwap)
    ORE_REGISTER_TRADE_BUILDER("CommodityForward", ore::data::CommodityForward)
    ORE_REGISTER_TRADE_BUILDER("EquityCliquetOption", ore::data::EquityCliquetOption)
    ORE_REGISTER_TRADE_BUILDER("CommodityDigitalOption", ore::data::CommodityDigitalOption)
    ORE_REGISTER_TRADE_BUILDER("EquityForward", ore::data::EquityForward)
    ORE_REGISTER_TRADE_BUILDER("IndexCreditDefaultSwapOption", ore::data::IndexCreditDefaultSwapOption)
    ORE_REGISTER_TRADE_BUILDER("CommodityAveragePriceOption", ore::data::CommodityAveragePriceOption)
    ORE_REGISTER_TRADE_BUILDER("CreditDefaultSwapOption", ore::data::CreditDefaultSwapOption)
    ORE_REGISTER_TRADE_BUILDER("FailedTrade", ore::data::FailedTrade)
    ORE_REGISTER_TRADE_BUILDER("ForwardBond", ore::data::ForwardBond)
    ORE_REGISTER_TRADE_BUILDER("EquityDoubleTouchOption", ore::data::EquityDoubleTouchOption)
    ORE_REGISTER_TRADE_BUILDER("CommodityOptionStrip", ore::data::CommodityOptionStrip)
    ORE_REGISTER_TRADE_BUILDER("SyntheticCDO", ore::data::SyntheticCDO)
    ORE_REGISTER_TRADE_BUILDER("Bond", ore::data::Bond)
    ORE_REGISTER_TRADE_BUILDER("CreditLinkedSwap", ore::data::CreditLinkedSwap)
    ORE_REGISTER_TRADE_BUILDER("EquityEuropeanBarrierOption", ore::data::EquityEuropeanBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("InflationSwap", ore::data::InflationSwap)
    ORE_REGISTER_TRADE_BUILDER("EquityDoubleBarrierOption", ore::data::EquityDoubleBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("BondOption", ore::data::BondOption)
    ORE_REGISTER_TRADE_BUILDER("CreditDefaultSwap", ore::data::CreditDefaultSwap)
    ORE_REGISTER_TRADE_BUILDER("FxKIKOBarrierOption", ore::data::FxKIKOBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("FxBarrierOption", ore::data::FxBarrierOption)
    ORE_REGISTER_TRADE_BUILDER("EquityOption", ore::data::EquityOption)
    ORE_REGISTER_TRADE_BUILDER("FxOption", ore::data::FxOption)

    ORE_REGISTER_LEGBUILDER("CommodityFixedLegBuilder", ore::data::CommodityFixedLegBuilder)
    ORE_REGISTER_LEGBUILDER("CommodityFloatingLegBuilder", ore::data::CommodityFloatingLegBuilder)
    ORE_REGISTER_LEGBUILDER("DurationAdjustedCmsLegBuilder", ore::data::DurationAdjustedCmsLegBuilder)
    ORE_REGISTER_LEGBUILDER("FixedLegBuilder", ore::data::FixedLegBuilder)
    ORE_REGISTER_LEGBUILDER("ZeroCouponFixedLegBuilder", ore::data::ZeroCouponFixedLegBuilder)
    ORE_REGISTER_LEGBUILDER("FloatingLegBuilder", ore::data::FloatingLegBuilder)
    ORE_REGISTER_LEGBUILDER("CashflowLegBuilder", ore::data::CashflowLegBuilder)
    ORE_REGISTER_LEGBUILDER("CPILegBuilder", ore::data::CPILegBuilder)
    ORE_REGISTER_LEGBUILDER("YYLegBuilder", ore::data::YYLegBuilder)
    ORE_REGISTER_LEGBUILDER("CMSLegBuilder", ore::data::CMSLegBuilder)
    ORE_REGISTER_LEGBUILDER("CMBLegBuilder", ore::data::CMBLegBuilder)
    ORE_REGISTER_LEGBUILDER("DigitalCMSLegBuilder", ore::data::DigitalCMSLegBuilder)
    ORE_REGISTER_LEGBUILDER("CMSSpreadLegBuilder", ore::data::CMSSpreadLegBuilder)
    ORE_REGISTER_LEGBUILDER("DigitalCMSSpreadLegBuilder", ore::data::DigitalCMSSpreadLegBuilder)
    ORE_REGISTER_LEGBUILDER("EquityLegBuilder", ore::data::EquityLegBuilder)
    ORE_REGISTER_LEGBUILDER("EquityMarginLegBuilder", ore::data::EquityMarginLegBuilder)

    ORE_REGISTER_AMC_ENGINE_BUILDER(ore::data::CamAmcCurrencySwapEngineBuilder)
    ORE_REGISTER_AMC_ENGINE_BUILDER(ore::data::LgmAmcBermudanSwaptionEngineBuilder)
    ORE_REGISTER_AMC_ENGINE_BUILDER(ore::data::CamAmcSwapEngineBuilder)
    ORE_REGISTER_AMC_ENGINE_BUILDER(ore::data::CamAmcFxOptionEngineBuilder)

    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommoditySpreadOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CpiCapFloorEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityFutureEuropeanOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::DiscountingBondTRSEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionMCDAAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionMCDAASEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionMCDGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionADGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionADGASEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionACGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanAsianOptionTWEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanForwardOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityEuropeanCSOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityAmericanOptionFDEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityAmericanOptionBAWEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFloorEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxDigitalOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxDigitalCSOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommoditySwaptionAnalyticalEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommoditySwaptionMonteCarloEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxDigitalBarrierOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommoditySwapEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanCompositeEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxForwardEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::DiscountingBondRepoEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::AccrualBondRepoEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredOvernightIndexedCouponLegEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredCpiLegCouponEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredCpiLegCashFlowEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxTouchOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EuropeanSwaptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::LGMGridBermudanSwaptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::LgmMcBermudanSwaptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::VarSwapEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxDoubleTouchOptionAnalyticEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxDoubleBarrierOptionAnalyticEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityBarrierOptionAnalyticEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityBarrierOptionFDEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityTouchOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredYoYLegEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityDigitalOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionMCDAAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionMCDAASEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionMCDGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionADGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionADGASEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionACGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanAsianOptionTWEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::SwapEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::SwapEngineBuilderOptimised)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CrossCurrencySwapEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::MidPointIndexCdsEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityForwardEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionMCDAAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionMCDAASEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionMCDGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionADGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionADGASEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionACGAPEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanAsianOptionTWEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CmsSpreadCouponPricerBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::AnalyticHaganCmsCouponPricerBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::NumericalHaganCmsCouponPricerBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::LinearTSRCmsCouponPricerBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityForwardEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::BlackIndexCdsOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityApoAnalyticalEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CommodityApoMonteCarloEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::BlackCdsOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::YoYCapFloorEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredAverageONIndexedCouponLegEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::DiscountingForwardBondEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityDoubleTouchOptionAnalyticEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredIborLegEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::LinearTsrDurationAdjustedCmsCouponPricerBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::GaussCopulaBucketingCdoEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::BondDiscountingEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CreditLinkedSwapEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityDoubleBarrierOptionAnalyticEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::BondOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::MidPointCdsEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxBarrierOptionAnalyticEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxBarrierOptionFDEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityEuropeanCSOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityAmericanOptionFDEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::EquityAmericanOptionBAWEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::CapFlooredNonStandardYoYLegEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::QuantoEquityEuropeanOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxEuropeanCSOptionEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxAmericanOptionFDEngineBuilder)
    ORE_REGISTER_ENGINE_BUILDER(ore::data::FxAmericanOptionBAWEngineBuilder)
}

} // namespace ore::analytics
