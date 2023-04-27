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

/*! \file qle/indexes/equityindex.cpp
    \brief equity index class for holding equity fixing histories and forwarding.
    \ingroup indexes
*/

#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <boost/make_shared.hpp>
#include <string>

using std::string;
using QuantExt::PriceTermStructure;

namespace QuantExt {

CommodityIndex::CommodityIndex(const std::string& underlyingName, const Date& expiryDate,
                               const Calendar& fixingCalendar, const Handle<QuantExt::PriceTermStructure>& curve)
    : underlyingName_(underlyingName), expiryDate_(expiryDate), fixingCalendar_(fixingCalendar),
      curve_(curve), keepDays_(false) {
    init();
}

CommodityIndex::CommodityIndex(const string& underlyingName, const Date& expiryDate,
                               const Calendar& fixingCalendar, bool keepDays, const Handle<PriceTermStructure>& curve)
    : underlyingName_(underlyingName), expiryDate_(expiryDate), fixingCalendar_(fixingCalendar),
      curve_(curve), keepDays_(keepDays) {
    init();
}

void CommodityIndex::init() {

    if (expiryDate_ == Date()) {
        // spot price index
        name_ = "COMM-" + underlyingName_;
        isFuturesIndex_ = false;
    } else {
        // futures price index
        std::ostringstream o;
        o << "COMM-" << underlyingName_ << "-" << QuantLib::io::iso_date(expiryDate_);
        name_ = o.str();
        if (!keepDays_) {
            // Remove the "-dd" portion from the expiry date
            name_.erase(name_.length() - 3);
        }
        isFuturesIndex_ = true;
    }

    registerWith(curve_);
    registerWith(Settings::instance().evaluationDate());
    registerWith(IndexManager::instance().notifier(name()));

}

Real CommodityIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {

    QL_REQUIRE(isValidFixingDate(fixingDate), "Commodity index " << name() << ": fixing date " <<
                                                                 io::iso_date(fixingDate) << " is not valid");
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(expiryDate_ == Date() || fixingDate <= expiryDate_,
               "Commodity index " << name() << ": fixing requested on fixing date (" << io::iso_date(fixingDate)
                                  << ") that is past the expiry date (" << io::iso_date(expiryDate_)
                                  << "). Eval date is " << today);

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

Real CommodityIndex::pastFixing(const Date& fixingDate) const {
    QL_REQUIRE(isValidFixingDate(fixingDate), fixingDate << " is not a valid fixing date");
    return timeSeries()[fixingDate];
}

Real CommodityIndex::forecastFixing(const Time& fixingTime) const {
    if (isFuturesIndex_)
        return curve_->price(expiryDate_);
    else
        return curve_->price(fixingTime);
}
Real CommodityIndex::forecastFixing(const Date& fixingDate) const {
    if (isFuturesIndex_)
        return curve_->price(expiryDate_);
    else
        return curve_->price(fixingDate);
}

boost::shared_ptr<CommodityIndex> CommoditySpotIndex::clone(const Date& expiryDate,
                                                            const boost::optional<Handle<PriceTermStructure>>& ts) const {
    const auto& pts = ts ? *ts : priceCurve();
    return boost::make_shared<CommoditySpotIndex>(underlyingName(), fixingCalendar(), pts);
}

boost::shared_ptr<CommodityIndex> CommodityFuturesIndex::clone(const Date& expiry,
                                                               const boost::optional<Handle<PriceTermStructure>>& ts) const {
    const auto& pts = ts ? *ts : priceCurve();
    const auto& ed = expiry == Date() ? expiryDate() : expiry;
    return boost::make_shared<CommodityFuturesIndex>(underlyingName(), ed, fixingCalendar(), keepDays(), pts);
}

CommodityBasisFutureIndex::CommodityBasisFutureIndex(const std::string& underlyingName, const Date& expiryDate,
                                                     const Calendar& fixingCalendar,
                                                     const Handle<QuantExt::CommodityFuturesIndex>& baseIndex,
                                                     const boost::shared_ptr<FutureExpiryCalculator>& expiryCalcBasis,
                                                     const boost::shared_ptr<FutureExpiryCalculator>& expiryCalcBase,
                                                     const Handle<QuantExt::PriceTermStructure>& priceCurve,
                                                     const bool addSpread)
    : CommodityFuturesIndex(underlyingName, expiryDate, fixingCalendar, priceCurve), baseIndex_(baseIndex),
      expiryCalcBasis_(expiryCalcBasis), expiryCalcBase_(expiryCalcBase), addSpread_(addSpread) {
    QL_REQUIRE(expiryDate_ != Date(), "non-empty expiry date expected CommodityFuturesIndex");
    QL_REQUIRE(!baseIndex_.empty() && baseIndex_.currentLink() != nullptr,
               "non-null spreadIndex required for CommodityBasisFutureIndex");
    QL_REQUIRE(expiryCalcBasis != nullptr,
               "non-null future expiry calculator for the basis conventions CommodityBasisFutureIndex");
    QL_REQUIRE(expiryCalcBasis != nullptr,
               "non-null future expiry calculator for the base conventions CommodityBasisFutureIndex");
    registerWith(baseIndex);
    QuantLib::Date contractDate = getContractDate(expiryDate);
    cashflow_ = makeCashflow(contractDate, contractDate + 1 * Months - 1 * Days);
}

Real CommodityBasisFutureIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const {
    double lambda = addSpread_ ? 1.0 : -1.0;
    return cashflow_->amount() + lambda * CommodityFuturesIndex::fixing(fixingDate, forecastTodaysFixing);
}

boost::shared_ptr<CommodityIndex>
CommodityBasisFutureIndex::clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
                                 const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const {
    if (expiryDate != QuantLib::Date()) {
        QuantLib::Date contractDate = getContractDate(expiryDate);
        cashflow_ = makeCashflow(contractDate, contractDate + 1 * Months - 1 * Days);
    }
    CommodityFuturesIndex::clone(expiryDate, ts);
}

QuantLib::Date CommodityBasisFutureIndex::getContractDate(const Date& expiry) const {

    using QuantLib::Date;
    using QuantLib::io::iso_date;

    // Try the expiry date's associated contract month and year. Start with the expiry month and year itself.
    // Assume that expiry is within 12 months of the contract month and year.
    Date result(1, expiry.month(), expiry.year());
    Date calcExpiry = expiryCalcBasis_->expiryDate(result, 0);
    Size maxIncrement = 12;
    while (calcExpiry != expiry && maxIncrement > 0) {
        if (calcExpiry < expiry)
            result += 1 * Months;
        else
            result -= 1 * Months;
        calcExpiry = expiryCalcBasis_->expiryDate(result, 0);
        maxIncrement--;
    }

    QL_REQUIRE(calcExpiry == expiry, "Expected the calculated expiry, " << iso_date(calcExpiry)
                                                                        << ", to equal the expiry, " << iso_date(expiry)
                                                                        << ".");

    return result;
}

boost::shared_ptr<QuantLib::CashFlow> CommodityBasisFutureIndex::makeCashflow(const QuantLib::Date& start,
    const QuantLib::Date& end) const {
    return boost::make_shared<CommodityIndexedCashFlow>(
        1.0, start, end, baseIndex_, 0, QuantLib::NullCalendar(), QuantLib::Unadjusted, 0, QuantLib::NullCalendar(), 0.0,
        1.0, CommodityIndexedCashFlow::PaymentTiming::InArrears, true, true, true, 0, expiryCalcBase_);
}

} // namespace QuantExt
