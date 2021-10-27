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

#include <boost/algorithm/string.hpp>
#include <ql/currencies/exchangeratemanager.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace std;

namespace QuantExt {

FxIndex::FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
                 const Calendar& fixingCalendar, const Handle<YieldTermStructure>& sourceYts,
                 const Handle<YieldTermStructure>& targetYts, bool inverseIndex, bool fixingTriangulation)
    : familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target),
      sourceYts_(sourceYts), targetYts_(targetYts), useQuote_(false), fixingCalendar_(fixingCalendar),
      inverseIndex_(inverseIndex), fixingTriangulation_(fixingTriangulation) {

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
                 bool inverseIndex, bool fixingTriangulation)
    : familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target),
      sourceYts_(sourceYts), targetYts_(targetYts), fxQuote_(fxQuote), useQuote_(true), fixingCalendar_(fixingCalendar),
      inverseIndex_(inverseIndex), fixingTriangulation_(fixingTriangulation) {

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

    QL_REQUIRE(isValidFixingDate(fixingDate),
               "Fixing date " << fixingDate << " is not valid for FxIndex '" << name() << "'");

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

    return result;
}

Real FxIndex::forecastFixing(const Time& fixingTime) const {
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
    
    // TODO: Add a time based adjusement for settlement days
    Real forward = rate * sourceYts_->discount(fixingTime)  /
        targetYts_->discount(fixingTime);
    return forward;
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
    Date refValueDate = valueDate(fixingCalendar().adjust(Settings::instance().evaluationDate()));

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

    return inverseIndex_ ? 1.0 / forward : forward;
}

boost::shared_ptr<FxIndex> FxIndex::clone(const Handle<Quote> fxQuote, const Handle<YieldTermStructure>& sourceYts,
                                          const Handle<YieldTermStructure>& targetYts) {
    return boost::make_shared<FxIndex>(familyName_, fixingDays_, sourceCurrency_, targetCurrency_, fixingCalendar_,
                                       fxQuote, sourceYts, targetYts, inverseIndex_);
}

std::string FxIndex::name() const { 
    return name_; 
}

Calendar FxIndex::fixingCalendar() const { 
    return fixingCalendar_; 
}

bool FxIndex::isValidFixingDate(const Date& d) const { 
    return fixingCalendar().isBusinessDay(d); 
}

void FxIndex::update() { 
    notifyObservers(); 
}

Date FxIndex::fixingDate(const Date& valueDate) const {
    Date fixingDate = fixingCalendar().advance(valueDate, -static_cast<Integer>(fixingDays_), Days);
    return fixingDate;
}

Date FxIndex::valueDate(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return fixingCalendar().advance(fixingDate, fixingDays_, Days);
}

Real FxIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");

    Real fixing = timeSeries()[fixingDate];

    if (fixing == Null<Real>() && fixingTriangulation_) {        
        // check reverse
        string revName = familyName_ + " " + targetCurrency_.code() + "/" + sourceCurrency_.code();
        if (IndexManager::instance().hasHistoricalFixing(revName, fixingDate))
            return 1.0 / IndexManager::instance().getHistory(revName)[fixingDate];

        // Now we search for a pair of quotes that we can combine to construct the quote required.
        // We only search for a pair of quotes a single step apart.
        //
        // Suppose we want a USDJPY quote and we have EUR based data, there are 4 combinations to
        // consider:
        // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
        // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
        // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
        // USDEUR, JPYEUR  => we want USDEUR / JPYEUR [Triangulation (but in the reverse order)]
        //
        // Loop over the map, look for domestic then use the map to find the other side of the pair.

        // Loop over all the possible Indexes
        vector<string> availIndexes = IndexManager::instance().histories();
        for (auto i : availIndexes) {
            if (boost::starts_with(i, familyName_)) {
                // check for a fixing
                Real fixing = IndexManager::instance().getHistory(i)[fixingDate];
                if (fixing != Null<Real>()) {
                    
                    Size l = i.size();
                    string keyDomestic = i.substr(l - 7, 3);
                    string keyForeign = i.substr(l - 3);
                    string domestic = sourceCurrency_.code();
                    string foreign = targetCurrency_.code();

                    if (domestic == keyDomestic) {
                        // we have domestic, now look for foreign/keyForeign
                        // USDEUR, JPYEUR  => we want USDEUR / JPYEUR [Triangulation (but in the reverse order)]
                        string forName = familyName_ + " " + foreign + "/" + keyForeign;
                        if (IndexManager::instance().hasHistoricalFixing(forName, fixingDate)) {
                            Real forFixing = IndexManager::instance().getHistory(forName)[fixingDate];
                            return fixing / forFixing;
                        }

                        // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
                        forName = familyName_ + " " + keyForeign + "/" + foreign;
                        if (IndexManager::instance().hasHistoricalFixing(forName, fixingDate)) {
                            Real forFixing = IndexManager::instance().getHistory(forName)[fixingDate];
                            return fixing * forFixing;
                        }
                    }

                    if (domestic == keyForeign) {
                        // we have fixing, now look for foreign/keyDomestic
                        // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
                        string forName = familyName_ + " " + foreign + "/" + keyDomestic;
                        if (IndexManager::instance().hasHistoricalFixing(forName, fixingDate)) {
                            Real forFixing = IndexManager::instance().getHistory(forName)[fixingDate];
                            return 1 / (fixing * forFixing);
                        }

                        // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
                        forName = familyName_ + " " + keyDomestic + "/" + foreign;
                        if (IndexManager::instance().hasHistoricalFixing(forName, fixingDate)) {
                            Real forFixing = IndexManager::instance().getHistory(forName)[fixingDate];
                            return fixing / forFixing;
                        }
                    }
                }
            }
        }
    }
    return inverseIndex_ ? 1.0 / fixing : fixing;
}

} // namespace QuantExt
