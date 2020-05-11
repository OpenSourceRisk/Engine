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

#include <qle/indexes/bondindex.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <ql/settings.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

BondIndex::BondIndex(const std::string& securityName, const Calendar& fixingCalendar,
                     const boost::shared_ptr<QuantLib::Bond>& bond, const Handle<YieldTermStructure>& discountCurve,
                     const Handle<DefaultProbabilityTermStructure>& defaultCurve, const Handle<Quote>& recoveryRate,
                     const Handle<Quote>& securitySpread, const Handle<YieldTermStructure>& incomeCurve,
                     const bool conditionalOnSurvival)
    : securityName_(securityName), fixingCalendar_(fixingCalendar), bond_(bond), discountCurve_(discountCurve),
      defaultCurve_(defaultCurve), recoveryRate_(recoveryRate), securitySpread_(securitySpread),
      incomeCurve_(incomeCurve), conditionalOnSurvival_(conditionalOnSurvival) {

    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(BondIndex::name()));
    registerWith(bond_);
    registerWith(discountCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
    registerWith(incomeCurve_);

    vanillaBondEngine_ = boost::make_shared<DiscountingRiskyBondEngine>(discountCurve, defaultCurve, recoveryRate,
                                                                        securitySpread, 6 * Months, boost::none);
}

std::string BondIndex::name() const { return "BondIndex-" + securityName_; }

Calendar BondIndex::fixingCalendar() const { return fixingCalendar_; }

bool BondIndex::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

void BondIndex::update() { notifyObservers(); }

Real BondIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {
    //! this logic is the same as in InterestRateIndex
    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid for '" << name() << "'");
    Date today = Settings::instance().evaluationDate();
    if (fixingDate > today || (fixingDate == today && forecastTodaysFixing))
        return forecastFixing(fixingDate);
    if (fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
        // must have been fixed
        // do not catch exceptions
        Rate result = pastFixing(fixingDate);
        QL_REQUIRE(result != Null<Real>(), "Missing " << name() << " fixing for " << fixingDate);
        return result;
    }
    try {
        // might have been fixed
        Rate result = pastFixing(fixingDate);
        if (result != Null<Real>())
            return result;
        else
            ; // fall through and forecast
    } catch (Error&) {
        ; // fall through and forecast
    }
    return forecastFixing(fixingDate);
}

Rate BondIndex::forecastFixing(const Date& fixingDate) const {
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(fixingDate >= today,
               "BondIndex::forecastFixing(): fixingDate (" << fixingDate << ") must be >= today (" << today << ")");
    QL_REQUIRE(bond_, "BondIndex::forecastFixing(): bond required");

    // try to get the clean price from the bond itself
    if (fixingDate == today) {
        try {
            return bond_->cleanPrice() / 100.0;
        } catch (...) {
        }
    }

    // fall back on assuming that the bond can be priced by simply discounting its cashflows
    return vanillaBondEngine_->calculateNpv(bond_->settlementDate(fixingDate), bond_->cashflows(), incomeCurve_,
                                            conditionalOnSurvival_) /
               bond_->notional(fixingDate) -
           bond_->accruedAmount(fixingDate) / 100.0;
}

Real BondIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date for '" << name() << "'");
    return timeSeries()[fixingDate];
}

} // namespace QuantExt
