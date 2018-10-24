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

/*! \file qle/indexes/dividendindex.cpp
\brief dividend index class for adding historic dividends to the fixing manager.
\ingroup indexes
*/

#include <qle/indexes/dividendindex.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

DividendIndex::DividendIndex(const std::string& equityName, const Calendar& fixingCalendar)
    : equityName_(equityName), fixingCalendar_(fixingCalendar) {
    name_ = equityName + "_div";
    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(name()));
}

Real DividendIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid");

    Date today = Settings::instance().evaluationDate();

    Real result = Null<Real>();

    QL_REQUIRE(fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings(),
               "DividendIndex class only supports historic fixings.");
    result = pastFixing(fixingDate);
    QL_REQUIRE(result != Null<Real>(), "Missing " << name() << " fixing for " << fixingDate);

    return result;
}

boost::shared_ptr<DividendIndex> DividendIndex::clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
                                                  const Handle<YieldTermStructure>& dividend) const {
    return boost::make_shared<DividendIndex>(equityName(), fixingCalendar());
}

} // namespace QuantExt
