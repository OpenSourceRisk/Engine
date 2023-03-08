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

#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/durationadjustedcmslegdata.hpp>
#include <ored/portfolio/equityfxlegdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/legdatafactory.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

#define REG_LEGDATA(NAME, CLASS)                                                                                       \
    ore::data::LegDataFactory::instance().addBuilder(NAME, &ore::data::createLegData<CLASS>);

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
    REG_LEGDATA("Cashflow", ore::data::CashflowData)
    REG_LEGDATA("CPI", ore::data::CPILegData)
    REG_LEGDATA("YY", ore::data::YoYLegData)
    REG_LEGDATA("CMS", ore::data::CMSLegData)
    REG_LEGDATA("CMB", ore::data::CMBLegData)
    REG_LEGDATA("DigitalCMS", ore::data::DigitalCMSLegData)
    REG_LEGDATA("CMSSpread", ore::data::CMSSpreadLegData)
    REG_LEGDATA("DigitalCMSSpread", ore::data::DigitalCMSSpreadLegData)
    REG_LEGDATA("Equity", ore::data::EquityLegData)
    REG_LEGDATA("Equity", ore::data::EquityLegData)
    REG_LEGDATA("CommodityFixed", ore::data::CommodityFixedLegData)
    REG_LEGDATA("CommodityFloating", ore::data::CommodityFloatingLegData)
    REG_LEGDATA("DurationAdjustedCMS", ore::data::DurationAdjustedCmsLegData)
    REG_LEGDATA("EquityMargin", ore::data::EquityMarginLegData)
}

} // namespace ore::analytics
