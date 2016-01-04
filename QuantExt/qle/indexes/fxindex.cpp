/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*
 Copyright (C) 2012 Roland Lichters

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/indexes/fxindex.hpp>
#include <ql/settings.hpp>

#include <sstream>

namespace QuantLib {

    FxIndex::FxIndex(const Currency& unitCurrency,
                     const Currency& priceCurrency,
                     const Calendar& fixingCalendar)
        : unitCurrency_(unitCurrency), 
          priceCurrency_(priceCurrency), 
          fixingCalendar_(fixingCalendar) {
        
        name_ = unitCurrency_.code() + "/" + priceCurrency_.code();
        
        registerWith(Settings::instance().evaluationDate());
        registerWith(IndexManager::instance().notifier(name()));
    }

    Rate FxIndex::fixing(const Date& fixingDate,
                         bool forecastTodaysFixing) const {

        QL_REQUIRE(isValidFixingDate(fixingDate),
                   "Fixing date " << fixingDate << " is not valid");

        Date today = Settings::instance().evaluationDate();

        if (fixingDate>today ||
            (fixingDate==today && forecastTodaysFixing))
            QL_FAIL("only past fixings are supported for FX indices");

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
                QL_FAIL("today's FX fixing not available");
        } catch (Error&) {
            QL_FAIL("today's FX fixing not available");
        }
    }

}
