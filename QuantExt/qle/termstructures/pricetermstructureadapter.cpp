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

#include <qle/termstructures/pricetermstructureadapter.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

PriceTermStructureAdapter::PriceTermStructureAdapter(const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve,
                                                     const QuantLib::ext::shared_ptr<YieldTermStructure>& discount,
                                                     Natural spotDays, const Calendar& spotCalendar)
    : priceCurve_(priceCurve), discount_(discount), spotDays_(spotDays), spotCalendar_(spotCalendar) {

    QL_REQUIRE(
        priceCurve_->referenceDate() == discount_->referenceDate(),
        "PriceTermStructureAdapter: The reference date of the discount curve and price curve should be the same");

    registerWith(priceCurve_);
    registerWith(discount_);
}

PriceTermStructureAdapter::PriceTermStructureAdapter(const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve,
                                                     const QuantLib::ext::shared_ptr<YieldTermStructure>& discount,
                                                     const Handle<Quote>& spotQuote)
    : priceCurve_(priceCurve), discount_(discount), spotDays_(0), spotQuote_(spotQuote) {

    QL_REQUIRE(
        priceCurve_->referenceDate() == discount_->referenceDate(),
        "PriceTermStructureAdapter: The reference date of the discount curve and price curve should be the same");

    registerWith(priceCurve_);
    registerWith(discount_);
    registerWith(spotQuote_);
}

Date PriceTermStructureAdapter::maxDate() const {
    // Take the min of the two underlying curves' max date
    // Extrapolation will be determined by each underlying curve individually
    return min(priceCurve_->maxDate(), discount_->maxDate());
}

const Date& PriceTermStructureAdapter::referenceDate() const {
    QL_REQUIRE(
        priceCurve_->referenceDate() == discount_->referenceDate(),
        "PriceTermStructureAdapter: The reference date of the discount curve and price curve should be the same");
    return priceCurve_->referenceDate();
}

DayCounter PriceTermStructureAdapter::dayCounter() const { return priceCurve_->dayCounter(); }

const QuantLib::ext::shared_ptr<PriceTermStructure>& PriceTermStructureAdapter::priceCurve() const { return priceCurve_; }

const QuantLib::ext::shared_ptr<YieldTermStructure>& PriceTermStructureAdapter::discount() const { return discount_; }

Natural PriceTermStructureAdapter::spotDays() const { return spotDays_; }

const Calendar& PriceTermStructureAdapter::spotCalendar() const { return spotCalendar_; }

DiscountFactor PriceTermStructureAdapter::discountImpl(Time t) const {
    if (t == 0.0)
        return 1.0;
    // Returns discount factor exp(-s(t) * t) where s(t) is defined such that
    // FP(0, t) = S(0) exp([z(t) - s(t)] t)
    Real spotPrice;
    if (spotQuote_.empty()) {
        Time spotTime = timeFromReference(spotCalendar_.advance(referenceDate(), spotDays_ * Days));
        spotPrice = priceCurve_->price(spotTime, true);
    } else {
        spotPrice = spotQuote_->value();
    }
    Real forwardPrice = priceCurve_->price(t, true);
    DiscountFactor discount = discount_->discount(t, true);
    return discount * forwardPrice / spotPrice;
}

} // namespace QuantExt
