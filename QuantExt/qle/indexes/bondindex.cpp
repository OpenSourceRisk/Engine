/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qlep/indexes/bondindex.cpp
  \brief light-weight bond index class for holding historical clean bond price fixings
  \ingroup indexes
*/

#include <ql/settings.hpp>

#include <boost/make_shared.hpp>
#include <qle/indexes/bondindex.hpp>

namespace QuantExt {

BondIndex::BondIndex(const std::string& name, const Calendar& fixingCalendar)
    : familyName_("BondIndex"), name_(familyName_ + "-" + name), fixingCalendar_(fixingCalendar) {

    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(BondIndex::name()));
}

Real BondIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid");

    Date today = Settings::instance().evaluationDate();

    QL_REQUIRE(fixingDate < today || (fixingDate == today && forecastTodaysFixing == false),
               "Bond index covers historical fixings only");

    QL_REQUIRE(IndexManager::instance().hasHistory(name_), "missing fixing history for bond index " << name_);
    const TimeSeries<Real>& history = IndexManager::instance().getHistory(name_);

    QL_REQUIRE(!history.empty(), "fixing history for bond index " << name_ << " is empty");
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    QL_REQUIRE(history[fixingDate] != Null<Real>(),
               "missing bond price fixing for name " << name_ << " date " << QuantLib::io::iso_date(fixingDate));

    return history[fixingDate];
}

} // namespace QuantExt
