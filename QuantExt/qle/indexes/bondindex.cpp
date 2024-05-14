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

#include <ql/cashflows/cpicoupon.hpp>
#include <ql/settings.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/utilities/inflation.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

BondIndex::BondIndex(const std::string& securityName, const bool dirty, const bool relative,
                     const Calendar& fixingCalendar, const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond,
                     const Handle<YieldTermStructure>& discountCurve,
                     const Handle<DefaultProbabilityTermStructure>& defaultCurve, const Handle<Quote>& recoveryRate,
                     const Handle<Quote>& securitySpread, const Handle<YieldTermStructure>& incomeCurve,
                     const bool conditionalOnSurvival, const Date& issueDate, const PriceQuoteMethod priceQuoteMethod,
                     const double priceQuoteBaseValue, const bool isInflationLinked, const double bidAskAdjustment,
                     const bool bondIssueDateFallback)
    : securityName_(securityName), dirty_(dirty), relative_(relative), fixingCalendar_(fixingCalendar), bond_(bond),
      discountCurve_(discountCurve), defaultCurve_(defaultCurve), recoveryRate_(recoveryRate),
      securitySpread_(securitySpread), incomeCurve_(incomeCurve), conditionalOnSurvival_(conditionalOnSurvival),
      issueDate_(issueDate), priceQuoteMethod_(priceQuoteMethod), priceQuoteBaseValue_(priceQuoteBaseValue),
      isInflationLinked_(isInflationLinked), bidAskAdjustment_(bidAskAdjustment),
      bondIssueDateFallback_(bondIssueDateFallback) {

    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(BondIndex::name()));
    registerWith(bond_);
    registerWith(discountCurve_);
    registerWith(defaultCurve_);
    registerWith(recoveryRate_);
    registerWith(securitySpread_);
    registerWith(incomeCurve_);

    vanillaBondEngine_ = QuantLib::ext::make_shared<DiscountingRiskyBondEngine>(discountCurve, defaultCurve, recoveryRate,
                                                                        securitySpread, 6 * Months, boost::none);
}

std::string BondIndex::name() const { return "BOND-" + securityName_; }

Calendar BondIndex::fixingCalendar() const { return fixingCalendar_; }

bool BondIndex::isValidFixingDate(const Date& d) const { return fixingCalendar().isBusinessDay(d); }

void BondIndex::update() { notifyObservers(); }

Real BondIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {
    //! this logic is the same as in InterestRateIndex
    QL_REQUIRE(isValidFixingDate(fixingDate), "Fixing date " << fixingDate << " is not valid for '" << name() << "'");
    Date today = Settings::instance().evaluationDate();
    if (fixingDate > today || (fixingDate == today && forecastTodaysFixing))
        return forecastFixing(fixingDate);
    Real adj = priceQuoteMethod_ == PriceQuoteMethod::CurrencyPerUnit ? 1.0 / priceQuoteBaseValue_ : 1.0;
    if (fixingDate < today || Settings::instance().enforcesTodaysHistoricFixings()) {
        // must have been fixed
        // do not catch exceptions
        Rate result = pastFixing(fixingDate);
        QL_REQUIRE(result != Null<Real>(), "Missing " << name() << " fixing for " << fixingDate);
        return result * adj;
    }
    try {
        // might have been fixed
        Rate result = pastFixing(fixingDate);
        if (result != Null<Real>())
            return result * adj;
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

    // on today, try to get the dirty absolute price from the bond itself

    Real price = Null<Real>();
    if (fixingDate == today) {
        try {
            price = bond_->settlementValue();
        } catch (...) {
        }
    }

    // for future dates or if the above did not work, assume that the bond can be priced by
    // simply discounting its cashflows

    if (price == Null<Real>()) {
        auto res = vanillaBondEngine_->calculateNpv(bond_->settlementDate(fixingDate),
                                                    bond_->settlementDate(fixingDate), bond_->cashflows(), boost::none,
                                                    incomeCurve_, conditionalOnSurvival_, false);
        price = res.npv;
    }

    price += bidAskAdjustment_ * bond_->notional(fixingDate);

    if (!dirty_) {
        price -= bond_->accruedAmount(fixingDate) / 100.0 * bond_->notional(fixingDate);
    }

    if (relative_) {
        if (close_enough(bond_->notional(fixingDate), 0.0))
            price = 0.0;
        else
            price /= bond_->notional(fixingDate);
    }

    return price;
}

Real BondIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date for '" << name() << "'");

    Date fd = (bondIssueDateFallback_ && fixingDate < issueDate_) ? issueDate_ : fixingDate;

    Real price = timeSeries()[fd] + bidAskAdjustment_;
    if (price == Null<Real>())
        return price;
    if (dirty_) {
        QL_REQUIRE(bond_, "BondIndex::pastFixing(): bond required for dirty prices");
        price += bond_->accruedAmount(fd) / 100.0;
    }

    if (isInflationLinked_) {
        price *= QuantExt::inflationLinkedBondQuoteFactor(bond_);
    }

    if (!relative_) {
        QL_REQUIRE(bond_, "BondIndex::pastFixing(): bond required for absolute prices");
        price *= bond_->notional(fd);
    }
    return price;
}

BondFuturesIndex::BondFuturesIndex(const QuantLib::Date& expiryDate, const std::string& securityName, const bool dirty,
                                   const bool relative, const Calendar& fixingCalendar,
                                   const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond,
                                   const Handle<YieldTermStructure>& discountCurve,
                                   const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                   const Handle<Quote>& recoveryRate, const Handle<Quote>& securitySpread,
                                   const Handle<YieldTermStructure>& incomeCurve, const bool conditionalOnSurvival,
                                   const Date& issueDate, const PriceQuoteMethod priceQuoteMethod, const double priceQuoteBaseValue)
    : BondIndex(securityName, dirty, relative, fixingCalendar, bond, discountCurve, defaultCurve, recoveryRate,
                securitySpread, incomeCurve, conditionalOnSurvival, issueDate, priceQuoteMethod, priceQuoteBaseValue),
      expiryDate_(expiryDate) {}

std::string BondFuturesIndex::name() const {
    if (name_ == "") {
        std::ostringstream o;
        o << "BOND-" << securityName() << "-" << QuantLib::io::iso_date(expiryDate_);
        name_ = o.str();
        // Remove the "-dd" portion from the expiry date
        name_.erase(name_.length() - 3);
    }

    return name_;
}

Rate BondFuturesIndex::forecastFixing(const Date& fixingDate) const {
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(fixingDate >= today, "BondFuturesIndex::forecastFixing(): fixingDate ("
                                        << fixingDate << ") must be >= today (" << today << ")");
    QL_REQUIRE(bond_, "BondFuturesIndex::forecastFixing(): bond required");

    auto bondNpvResults = vanillaBondEngine_->calculateNpv(bond()->settlementDate(expiryDate_),
                                                           bond()->settlementDate(expiryDate_), bond()->cashflows(),
                                                           boost::none, incomeCurve(), conditionalOnSurvival(), false);
    Real price = bondNpvResults.npv;

    if (!dirty()) {
        price -= bond()->accruedAmount(expiryDate_) / 100.0 * bond()->notional(expiryDate_);
    }

    if (relative()) {
        if (close_enough(bond()->notional(expiryDate_), 0.0))
            price = 0.0;
        else
            price /= bond()->notional(expiryDate_);
    }

    return price;
}

Date ConstantMaturityBondIndex::maturityDate(const Date& valueDate) const {
    // same as IborIndex
    return fixingCalendar().advance(valueDate, tenor_, convention_, endOfMonth_);
}

Rate ConstantMaturityBondIndex::forecastFixing(const Date& fixingDate) const {
    QL_REQUIRE(bond_, "cannot forecast ConstantMaturityBondIndex fixing, because underlying bond not set");
    QL_REQUIRE(fixingDate == bondStartDate_, "bond yield fixing only available at bond start date, "
	       << io::iso_date(fixingDate) << " != " << io::iso_date(bondStartDate_));
    return bond_->yield(dayCounter_, compounding_, frequency_, accuracy_, maxEvaluations_, guess_, priceType_);
}

} // namespace QuantExt
