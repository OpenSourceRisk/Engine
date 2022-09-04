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
#include <ql/quotes/derivedquote.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/math/functional.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

FxRateQuote::FxRateQuote(Handle<Quote> spotQuote, const Handle<YieldTermStructure>& sourceYts,
                         const Handle<YieldTermStructure>& targetYts, Natural fixingDays,
                         const Calendar& fixingCalendar, Date refDate)
    : spotQuote_(spotQuote), sourceYts_(sourceYts), targetYts_(targetYts), fixingDays_(fixingDays),
      fixingCalendar_(fixingCalendar), refDate_(refDate) {
    registerWith(spotQuote_);
    registerWith(sourceYts_);
    registerWith(targetYts_);
}

Real FxRateQuote::value() const {
    QL_ENSURE(isValid(), "invalid FxRateQuote");

    Date today = refDate_ == Null<Date>() ? Settings::instance().evaluationDate() : refDate_;
    Date refValueDate = fixingCalendar_.advance(fixingCalendar_.adjust(today), fixingDays_, Days);

    return spotQuote_->value() * targetYts_->discount(refValueDate) / sourceYts_->discount(refValueDate);
}

bool FxRateQuote::isValid() const { 
    return !spotQuote_.empty() && spotQuote_->isValid() && !sourceYts_.empty() && !targetYts_.empty();
}

void FxRateQuote::update() { 
    notifyObservers();
}

FxSpotQuote::FxSpotQuote(Handle<Quote> todaysQuote, const Handle<YieldTermStructure>& sourceYts,
                         const Handle<YieldTermStructure>& targetYts, Natural fixingDays,
                         const Calendar& fixingCalendar, Date refDate)
    : todaysQuote_(todaysQuote), sourceYts_(sourceYts), targetYts_(targetYts), fixingDays_(fixingDays),
      fixingCalendar_(fixingCalendar), refDate_(refDate) {
    registerWith(todaysQuote_);
    registerWith(sourceYts_);
    registerWith(targetYts_);
}

Real FxSpotQuote::value() const {
    QL_ENSURE(isValid(), "invalid FxSpotQuote");

    Date today = refDate_ == Null<Date>() ? Settings::instance().evaluationDate() : refDate_;
    Date refValueDate = fixingCalendar_.advance(fixingCalendar_.adjust(today), fixingDays_, Days);

    return todaysQuote_->value() / targetYts_->discount(refValueDate) * sourceYts_->discount(refValueDate);
}

bool FxSpotQuote::isValid() const {
    return !todaysQuote_.empty() && todaysQuote_->isValid() && !sourceYts_.empty() && !targetYts_.empty();
}

void FxSpotQuote::update() {
    notifyObservers();
}

FxIndex::FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
                 const Calendar& fixingCalendar, const Handle<YieldTermStructure>& sourceYts,
                 const Handle<YieldTermStructure>& targetYts, bool inverseIndex, bool fixingTriangulation)
    : familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target),
      sourceYts_(sourceYts), targetYts_(targetYts), useQuote_(false), fixingCalendar_(fixingCalendar),
      inverseIndex_(inverseIndex), fixingTriangulation_(fixingTriangulation) {

    initialise();
    registerWith(Settings::instance().evaluationDate());
}

FxIndex::FxIndex(const std::string& familyName, Natural fixingDays, const Currency& source, const Currency& target,
                 const Calendar& fixingCalendar, const Handle<Quote> fxSpot,
                 const Handle<YieldTermStructure>& sourceYts, const Handle<YieldTermStructure>& targetYts,
                 bool inverseIndex, bool fixingTriangulation)
    : familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target),
      sourceYts_(sourceYts), targetYts_(targetYts), fxSpot_(fxSpot), useQuote_(true), fixingCalendar_(fixingCalendar),
      inverseIndex_(inverseIndex), fixingTriangulation_(fixingTriangulation) {

    initialise();
    registerWith(Settings::instance().evaluationDate());
}

FxIndex::FxIndex(const Date& referenceDate, const std::string& familyName, Natural fixingDays, const Currency& source,
        const Currency& target, const Calendar& fixingCalendar, const Handle<YieldTermStructure>& sourceYts,
        const Handle<YieldTermStructure>& targetYts, bool inverseIndex,
        bool fixingTriangulation)
    : referenceDate_(referenceDate), familyName_(familyName), fixingDays_(fixingDays), sourceCurrency_(source),
      targetCurrency_(target), sourceYts_(sourceYts), targetYts_(targetYts), useQuote_(true),
      fixingCalendar_(fixingCalendar), inverseIndex_(inverseIndex), fixingTriangulation_(fixingTriangulation) {
    
    initialise();
}

FxIndex::FxIndex(const Date& referenceDate, const std::string& familyName, Natural fixingDays, const Currency& source,
        const Currency& target, const Calendar& fixingCalendar, const Handle<Quote> fxSpot,
        const Handle<YieldTermStructure>& sourceYts, const Handle<YieldTermStructure>& targetYts, 
        bool inverseIndex, bool fixingTriangulation)
    : referenceDate_(referenceDate), familyName_(familyName),
    fixingDays_(fixingDays), sourceCurrency_(source), targetCurrency_(target), sourceYts_(sourceYts),
    targetYts_(targetYts), fxSpot_(fxSpot), useQuote_(true), fixingCalendar_(fixingCalendar),
    inverseIndex_(inverseIndex), fixingTriangulation_(fixingTriangulation) {

    initialise();
}

void FxIndex::initialise() {
    std::ostringstream tmp;
    tmp << familyName_ << " " << sourceCurrency_.code() << "/" << targetCurrency_.code();
    name_ = tmp.str();
        
    registerWith(IndexManager::instance().notifier(name()));
    registerWith(fxSpot_);
    registerWith(sourceYts_);
    registerWith(targetYts_);
}

const Handle<Quote> FxIndex::fxQuote(bool withSettlementLag) const {   
    Handle<Quote> quote;
    if (withSettlementLag || fixingDays_ == 0)
        quote = fxSpot_;
    
    if (quote.empty()) {
        if (fxRate_.empty()) {
            Handle<Quote> tmpQuote;
            if (!useQuote_)
                tmpQuote = Handle<Quote>(boost::make_shared<SimpleQuote>(
                    ExchangeRateManager::instance().lookup(sourceCurrency_, targetCurrency_).rate()));
            else
                tmpQuote = fxSpot_;

            // adjust for spot
            fxRate_ = Handle<Quote>(boost::make_shared<FxRateQuote>(tmpQuote, sourceYts_, targetYts_, 
                fixingDays_, fixingCalendar_, referenceDate_));
        }
        quote = fxRate_;
    }
    if (inverseIndex_) {
        // Create a derived quote to invert
        auto m = [](Real x) { return 1.0 / x; };
        quote = Handle<Quote>(boost::make_shared<DerivedQuote<decltype(m)>>(quote, m));
    }
    return quote;
}

Real FxIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    Date adjustedFixingDate = fixingCalendar().adjust(fixingDate, Preceding);
    QL_REQUIRE(isValidFixingDate(adjustedFixingDate),
               "Fixing date " << adjustedFixingDate << " is not valid for FxIndex '" << name() << "'");

    Date today = Settings::instance().evaluationDate();

    Real result = Null<Real>();

    if (adjustedFixingDate > today || (adjustedFixingDate == today && forecastTodaysFixing))
        result = forecastFixing(adjustedFixingDate);

    if (result == Null<Real>()) {
        if (adjustedFixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
            // must have been fixed
            // do not catch exceptions
            result = pastFixing(adjustedFixingDate);
            QL_REQUIRE(result != Null<Real>(), "Missing " << name() << " fixing for " << adjustedFixingDate);
        } else {
            try {
                // might have been fixed
                result = pastFixing(adjustedFixingDate);
            } catch (Error&) {
                ; // fall through and forecast
            }
            if (result == Null<Real>())
                result = forecastFixing(adjustedFixingDate);
        }
    }

    return inverseIndex_ ? 1.0 / result : result;
}

Real FxIndex::forecastFixing(const Time& fixingTime) const {
    QL_REQUIRE(!sourceYts_.empty() && !targetYts_.empty(), "null term structure set to this instance of " << name());

    // we base the forecast always on the exchange rate (and not on today's fixing)
    Real rate;
    if (!useQuote_) {
        rate = ExchangeRateManager::instance().lookup(sourceCurrency_, targetCurrency_).rate();
    } else {
        QL_REQUIRE(!fxSpot_.empty(), "FxIndex::forecastFixing(): fx quote required");
        rate = fxSpot_->value();
    }

    // TODO: do we need to add this a class variable?
    DayCounter dc = Actual365Fixed();

    // to make the spot adjustment we get the time to spot, and also add this to fixing time
    Date refValueDate = fixingCalendar().adjust(Settings::instance().evaluationDate());
    Date spotValueDate = valueDate(refValueDate);

    // time from ref to spot date
    Real spotTime = dc.yearFraction(refValueDate, spotValueDate);
    Real forwardTime = spotTime + fixingTime;

    // compute the forecast applying the usual no arbitrage principle
    Real forward = rate * sourceYts_->discount(forwardTime) * targetYts_->discount(spotTime) /
                   (targetYts_->discount(forwardTime) * sourceYts_->discount(spotTime));
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
        QL_REQUIRE(!fxSpot_.empty(), "FxIndex::forecastFixing(): fx quote required");
        rate = fxSpot_->value();
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

    return forward;
}

boost::shared_ptr<FxIndex> FxIndex::clone(const Handle<Quote> fxQuote, const Handle<YieldTermStructure>& sourceYts,
                                          const Handle<YieldTermStructure>& targetYts, const string& familyName, bool inverseIndex) {
    Handle<Quote> quote = fxQuote.empty() ? fxSpot_ : fxQuote;
    Handle<YieldTermStructure> source = sourceYts.empty() ? sourceYts_ : sourceYts;
    Handle<YieldTermStructure> target = targetYts.empty() ? targetYts_ : targetYts;
    string famName = familyName.empty() ? familyName_ : familyName;
    if (referenceDate_ == Null<Date>())
        return boost::make_shared<FxIndex>(famName, fixingDays_, sourceCurrency_, targetCurrency_,
            fixingCalendar_, quote, source, target, inverseIndex);
    else
        return boost::make_shared<FxIndex>(referenceDate_, famName, fixingDays_, sourceCurrency_, targetCurrency_,
            fixingCalendar_, quote, source, target, inverseIndex);

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
    fxRate_ = Handle<Quote>();
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
    if (fixing != Null<Real>())
        return fixing;

    if (fixingTriangulation_) {        
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
                            return forFixing / fixing;
                        }
                    }
                }
            }
        }
    }
    return fixing;
}

} // namespace QuantExt
