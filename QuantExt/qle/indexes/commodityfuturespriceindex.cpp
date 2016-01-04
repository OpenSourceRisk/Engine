/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/indexes/commodityfuturespriceindex.hpp>
#include <ql/settings.hpp>

#include <iostream>
#include <sstream>
using namespace std;

namespace QuantLib {

    CommodityFuturesPriceIndex::CommodityFuturesPriceIndex(std::string baseName, Period expiry, 
                                                           Calendar fixingCalendar)
        : expiry_(expiry), fixingCalendar_(fixingCalendar) {        
        ostringstream oss;
        oss << uppercase << baseName << expiry;
        name_ = oss.str(); 
        registerWith(Settings::instance().evaluationDate());
        registerWith(IndexManager::instance().notifier(name()));
    }

    Rate CommodityFuturesPriceIndex::fixing(const Date& fixingDate,
                                            bool forecastTodaysFixing) const {

        QL_REQUIRE(isValidFixingDate(fixingDate),
                   "Fixing date " << fixingDate << " is not valid");

        Date today = Settings::instance().evaluationDate();

        if (fixingDate>today ||
            (fixingDate==today && forecastTodaysFixing))
            QL_FAIL("only past fixings are supported for Commodity indices");

        if (fixingDate<today ||
            Settings::instance().enforcesTodaysHistoricFixings()) {
            // must have been fixed
            // do not catch exceptions
            Rate result = pastFixing(fixingDate);
            QL_REQUIRE(result != Null<Real>(),
                       "Missing " << name() << " fixing for " << fixingDate);
            return result;
        }

        try {
            // might have been fixed
            Rate result = pastFixing(fixingDate);
            if (result!=Null<Real>())
                return result;
            else
                QL_FAIL("today's fixing not available for " << name_);
        } catch (Error&) {
            QL_FAIL("today's FX fixing not available for " << name_);
        }
    }

}
