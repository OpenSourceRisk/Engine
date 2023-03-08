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
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/convertiblebondreferencedata.hpp>
#include <ored/portfolio/durationadjustedcmslegdata.hpp>
#include <ored/portfolio/equityfxlegdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/legdatafactory.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/referencedatafactory.hpp>

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
}

} // namespace ore::analytics
