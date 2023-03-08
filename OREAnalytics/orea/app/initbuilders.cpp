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

#define REG_LEGDATA(NAME, CLASS)                                                                                       \
    ore::data::LegDataFactory::instance().addBuilder(NAME, &ore::data::createLegData<CLASS>);

#define REG_CALIBRATION_INSTR(NAME, CLASS)                                                                             \
    ore::data::CalibrationInstrumentFactory::instance().addBuilder(NAME,                                               \
                                                                   &ore::data::createCalibrationInstrument<CLASS>);

#define REG_REFDATUM(NAME, CLASS)                                                                                      \
    ore::data::ReferenceDatumFactory::instance().addBuilder(                                                           \
        NAME, &ore::data::createReferenceDatumBuilder<ore::data::ReferenceDatumBuilder<CLASS>>);

#define REG_BONDBUILDER(NAME, CLASS) ore::data::BondFactory::instance().addBuilder(NAME, boost::make_shared<CLASS>());

#define REG_TRADEBUILDER(NAME, CLASS)                                                                                  \
    ore::data::TradeFactory::instance().addBuilder(NAME, boost::make_shared<ore::data::TradeBuilder<CLASS>>());

namespace ore::analytics {

void initBuilders() {

    static boost::shared_mutex mutex;
    static bool hasRun = false;

    boost::unique_lock<boost::shared_mutex> lock(mutex);

    if (hasRun)
        return;

    hasRun = true;

    REG_LEGDATA("Cashflow", ore::data::CashflowData)
    REG_LEGDATA("Fixed", ore::data::FixedLegData)
    REG_LEGDATA("ZeroCouponFixed", ore::data::ZeroCouponFixedLegData)
    REG_LEGDATA("Floating", ore::data::FloatingLegData)
    REG_LEGDATA("CPI", ore::data::CPILegData)
    REG_LEGDATA("YY", ore::data::YoYLegData)
    REG_LEGDATA("CMS", ore::data::CMSLegData)
    REG_LEGDATA("CMB", ore::data::CMBLegData)
    REG_LEGDATA("DigitalCMS", ore::data::DigitalCMSLegData)
    REG_LEGDATA("CMSSpread", ore::data::CMSSpreadLegData)
    REG_LEGDATA("DigitalCMSSpread", ore::data::DigitalCMSSpreadLegData)
    REG_LEGDATA("Equity", ore::data::EquityLegData)
    REG_LEGDATA("CommodityFixed", ore::data::CommodityFixedLegData)
    REG_LEGDATA("CommodityFloating", ore::data::CommodityFloatingLegData)
    REG_LEGDATA("DurationAdjustedCMS", ore::data::DurationAdjustedCmsLegData)
    REG_LEGDATA("EquityMargin", ore::data::EquityMarginLegData)

    REG_CALIBRATION_INSTR("CpiCapFloor", ore::data::CpiCapFloor)
    REG_CALIBRATION_INSTR("YoYCapFloor", ore::data::YoYCapFloor)
    REG_CALIBRATION_INSTR("YoYSwap", ore::data::YoYSwap)

    REG_REFDATUM("Bond", ore::data::BondReferenceDatum)
    REG_REFDATUM("CreditIndex", ore::data::CreditIndexReferenceDatum)
    REG_REFDATUM("EquityIndex", ore::data::EquityIndexReferenceDatum)
    REG_REFDATUM("CurrencyHedgedEquityIndex", ore::data::CurrencyHedgedEquityIndexReferenceDatum)
    REG_REFDATUM("Credit", ore::data::CreditReferenceDatum)
    REG_REFDATUM("Equity", ore::data::EquityReferenceDatum)
    REG_REFDATUM("BondBasket", ore::data::BondBasketReferenceDatum)
    REG_REFDATUM("ConvertibleBond", ore::data::BondBasketReferenceDatum)

    REG_BONDBUILDER("Bond", ore::data::VanillaBondBuilder)

    REG_TRADEBUILDER("CrossCurrencySwap", ore::data::CrossCurrencySwap)
    REG_TRADEBUILDER("CommoditySpreadOption", ore::data::CommoditySpreadOption)
    REG_TRADEBUILDER("EquityFutureOption", ore::data::EquityFutureOption)
    REG_TRADEBUILDER("BondTRS", ore::data::BondTRS)
    REG_TRADEBUILDER("CommodityOption", ore::data::CommodityOption)
    REG_TRADEBUILDER("CapFloor", ore::data::CapFloor)
    REG_TRADEBUILDER("FxDigitalOption", ore::data::FxDigitalOption)
    REG_TRADEBUILDER("CommoditySwaption", ore::data::CommoditySwaption)
    REG_TRADEBUILDER("FxDigitalBarrierOption", ore::data::FxDigitalBarrierOption)
    REG_TRADEBUILDER("ForwardRateAgreement", ore::data::ForwardRateAgreement)
    REG_TRADEBUILDER("CommodityDigitalAveragePriceOption", ore::data::CommodityDigitalAveragePriceOption)
    REG_TRADEBUILDER("CommoditySwap", ore::data::CommoditySwap)
    REG_TRADEBUILDER("EquitySwap", ore::data::EquitySwap)
    REG_TRADEBUILDER("FxForward", ore::data::FxForward)
    REG_TRADEBUILDER("BondRepo", ore::data::BondRepo)
    REG_TRADEBUILDER("FxAverageForward", ore::data::FxAverageForward)
    REG_TRADEBUILDER("FxEuropeanBarrierOption", ore::data::FxEuropeanBarrierOption)
    REG_TRADEBUILDER("FxTouchOption", ore::data::FxTouchOption)
    REG_TRADEBUILDER("EquityAsianOption", ore::data::EquityAsianOption)
    REG_TRADEBUILDER("FxAsianOption", ore::data::FxAsianOption)
    REG_TRADEBUILDER("CommodityAsianOption", ore::data::CommodityAsianOption)
    REG_TRADEBUILDER("Swaption", ore::data::Swaption)
    REG_TRADEBUILDER("EquityVarianceSwap", ore::data::EqVarSwap)
    REG_TRADEBUILDER("FxVarianceSwap", ore::data::FxVarSwap)
    REG_TRADEBUILDER("CommodityVarianceSwap", ore::data::ComVarSwap)
    REG_TRADEBUILDER("FxDoubleTouchOption", ore::data::FxDoubleTouchOption)
    REG_TRADEBUILDER("FxDoubleBarrierOption", ore::data::FxDoubleBarrierOption)
    REG_TRADEBUILDER("EquityBarrierOption", ore::data::EquityBarrierOption)
    REG_TRADEBUILDER("FxSwap", ore::data::FxSwap)
    REG_TRADEBUILDER("EquityTouchOption", ore::data::EquityTouchOption)
    REG_TRADEBUILDER("EquityDigitalOption", ore::data::EquityDigitalOption)
    REG_TRADEBUILDER("CompositeTrade", ore::data::CompositeTrade)
    REG_TRADEBUILDER("MultiLegOption", ore::data::MultiLegOption)
    REG_TRADEBUILDER("Swap", ore::data::Swap)
    REG_TRADEBUILDER("IndexCreditDefaultSwap", ore::data::IndexCreditDefaultSwap)
    REG_TRADEBUILDER("CommodityForward", ore::data::CommodityForward)
    REG_TRADEBUILDER("EquityCliquetOption", ore::data::EquityCliquetOption)
    REG_TRADEBUILDER("CommodityDigitalOption", ore::data::CommodityDigitalOption)
    REG_TRADEBUILDER("EquityForward", ore::data::EquityForward)
    REG_TRADEBUILDER("IndexCreditDefaultSwapOption", ore::data::IndexCreditDefaultSwapOption)
    REG_TRADEBUILDER("CommodityAveragePriceOption", ore::data::CommodityAveragePriceOption)
    REG_TRADEBUILDER("CreditDefaultSwapOption", ore::data::CreditDefaultSwapOption)
    REG_TRADEBUILDER("FailedTrade", ore::data::FailedTrade)
    REG_TRADEBUILDER("ForwardBond", ore::data::ForwardBond)
    REG_TRADEBUILDER("EquityDoubleTouchOption", ore::data::EquityDoubleTouchOption)
    REG_TRADEBUILDER("CommodityOptionStrip", ore::data::CommodityOptionStrip)
    REG_TRADEBUILDER("SyntheticCDO", ore::data::SyntheticCDO)
    REG_TRADEBUILDER("Bond", ore::data::Bond)
    REG_TRADEBUILDER("CreditLinkedSwap", ore::data::CreditLinkedSwap)
    REG_TRADEBUILDER("EquityEuropeanBarrierOption", ore::data::EquityEuropeanBarrierOption)
    REG_TRADEBUILDER("InflationSwap", ore::data::InflationSwap)
    REG_TRADEBUILDER("EquityDoubleBarrierOption", ore::data::EquityDoubleBarrierOption)
    REG_TRADEBUILDER("BondOption", ore::data::BondOption)
    REG_TRADEBUILDER("CreditDefaultSwap", ore::data::CreditDefaultSwap)
    REG_TRADEBUILDER("FxKIKOBarrierOption", ore::data::FxKIKOBarrierOption)
    REG_TRADEBUILDER("FxBarrierOption", ore::data::FxBarrierOption)
    REG_TRADEBUILDER("EquityOption", ore::data::EquityOption)
    REG_TRADEBUILDER("FxOption", ore::data::FxOption)
}

} // namespace ore::analytics
