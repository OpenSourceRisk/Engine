/*
 Copyright (C) 2015 Peter Caspers
 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/
 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program.
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ql/currencies/exchangeratemanager.hpp>
#include <qle/indexes/fxindex.hpp>
namespace QuantExt {

FxIndex::FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
                 const Calendar& fixingCalendar, const Handle<YieldTermStructure>& sourceYts,
                 const Handle<YieldTermStructure>& targetYts, bool inverseIndex)
    : familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target),
      sourceYts_(sourceYts), targetYts_(targetYts), useQuote_(false), fixingCalendar_(fixingCalendar),
      inverseIndex_(inverseIndex) {

    std::ostringstream tmp;
    tmp << familyName_ << " " << sourceCurrency_.code() << "/" << targetCurrency_.code();
    name_ = tmp.str();
    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(name()));
    registerWith(sourceYts_);
    registerWith(targetYts_);

    // we should register with the exchange rate manager
    // to be notified of changes in the spot exchange rate
    // however currently exchange rates are not quotes anyway
    // so this is to be revisited later
}
FxIndex::FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
                 const Calendar& fixingCalendar, const Handle<Quote> fxQuote,
                 const Handle<YieldTermStructure>& sourceYts, const Handle<YieldTermStructure>& targetYts,
                 bool inverseIndex)
    : familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target),
      sourceYts_(sourceYts), targetYts_(targetYts), fxQuote_(fxQuote), useQuote_(true), fixingCalendar_(fixingCalendar),
      inverseIndex_(inverseIndex) {

    std::ostringstream tmp;
    tmp << familyName_ << " " << sourceCurrency_.code() << "/" << targetCurrency_.code();
    name_ = tmp.str();
    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(name()));
    registerWith(fxQuote_);
    registerWith(sourceYts_);
    registerWith(targetYts_);
}

Real FxIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid");

    Date today = Settings::instance().evaluationDate();

    Real result = Null<Decimal>();

    if (fixingDate > today || (fixingDate == today && forecastTodaysFixing))
        result = forecastFixing(fixingDate);

    if (result == Null<Real>()) {
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
                result = forecastFixing(fixingDate);
        }
    }

    return inverseIndex_ ? 1.0 / result : result;
}

Real FxIndex::forecastFixing(const Date& fixingDate) const {
    QL_REQUIRE(!sourceYts_.empty() && !targetYts_.empty(), "null term structure set to this instance of " << name());

    // we base the forecast always on the exchange rate (and not on today's
    // fixing)
    Real rate;
    if (!useQuote_) {
        rate = ExchangeRateManager::instance().lookup(sourceCurrency_, targetCurrency_).rate();
    } else {
        QL_REQUIRE(!fxQuote_.empty(), "FxIndex::forecastFixing(): fx quote required");
        rate = fxQuote_->value();
    }

    // the exchange rate is interpreted as the spot rate w.r.t. the index's
    // settlement date
    Date refValueDate = valueDate(Settings::instance().evaluationDate());

    // the fixing is obeying the settlement delay as well
    Date fixingValueDate = valueDate(fixingDate);

    // we can assume fixingValueDate >= valueDate
    QL_REQUIRE(fixingValueDate >= refValueDate, "value date for requested fixing as of "
                                                    << fixingDate << " (" << fixingValueDate
                                                    << ") must be greater or equal to today's fixing value date ("
                                                    << refValueDate << ")");

    // compute the forecast applying the usual no arbitrage principle
    Real forward = rate * sourceYts_->discount(fixingValueDate) * targetYts_->discount(refValueDate) /
                   (sourceYts_->discount(refValueDate) * targetYts_->discount(fixingValueDate));

    return forward;
}

} // namespace QuantExt
