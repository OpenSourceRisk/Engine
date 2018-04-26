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

/*! \file qle/indexes/equityindex.cpp
\brief equity index class for holding equity fixing histories and forwarding.
\ingroup indexes
*/

#include <qle/indexes/equityindex.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

EquityIndex::EquityIndex(const std::string& familyName, const Calendar& fixingCalendar, const Handle<Quote> spotQuote,
                         const Handle<YieldTermStructure>& rate, const Handle<YieldTermStructure>& dividend)
    : familyName_(familyName), rate_(rate), dividend_(dividend), spotQuote_(spotQuote),
      fixingCalendar_(fixingCalendar) {

    name_ = familyName;
    if (!spotQuote_.empty())
        registerWith(spotQuote_);
    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(name()));
}

Real EquityIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid");

    Date today = Settings::instance().evaluationDate();

    if (fixingDate > today || (fixingDate == today && forecastTodaysFixing))
        return forecastFixing(fixingDate);

    Real result = Null<Decimal>();

    if (fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
        // must have been fixed
        // do not catch exceptions
        result = pastFixing(fixingDate);
        QL_REQUIRE(result != Null<Real>(), "Missing " << name() << " fixing for " << fixingDate);
    } else {
        try {
            // might have been fixed
            result = pastFixing(fixingDate);
        } catch (Error&) {
            ; // fall through and forecast
        }
        if (result == Null<Real>())
            return forecastFixing(fixingDate);
    }

    return result;
}

Real EquityIndex::forecastFixing(const Date& fixingDate) const {
    QL_REQUIRE(!rate_.empty(), "null term structure set to this instance of " << name());
    return forecastFixing(rate_->timeFromReference(fixingDate));
}

Real EquityIndex::forecastFixing(const Time& fixingTime) const {
    QL_REQUIRE(!spotQuote_.empty(), "null spot quote set to this instance of " << name());
    QL_REQUIRE(!rate_.empty() && !dividend_.empty(), "null term structure set to this instance of " << name());

    // we base the forecast always on the spot quote (and not on today's fixing)
    Real price = spotQuote_->value();

    // compute the forecast applying the usual no arbitrage principle
    Real forward = price * dividend_->discount(fixingTime) / rate_->discount(fixingTime);
    return forward;
}

boost::shared_ptr<EquityIndex> EquityIndex::clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
                                                  const Handle<YieldTermStructure>& dividend) const {
    return boost::make_shared<EquityIndex>(familyName(), fixingCalendar(), spotQuote, rate, dividend);
}

} // namespace QuantLib
