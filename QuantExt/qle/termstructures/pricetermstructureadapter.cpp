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

PriceTermStructureAdapter::PriceTermStructureAdapter(const boost::shared_ptr<Quote>& spotQuote,
    const boost::shared_ptr<PriceTermStructure>& priceCurve, const boost::shared_ptr<YieldTermStructure>& discount)
    : spotQuote_(spotQuote), priceCurve_(priceCurve), discount_(discount) {

    QL_REQUIRE(priceCurve_->referenceDate() == discount_->referenceDate(),
        "PriceTermStructureAdapter: The reference date of the discount curve and price curve should be the same");

    registerWith(spotQuote_);
    registerWith(priceCurve_);
    registerWith(discount_);
}

Date PriceTermStructureAdapter::maxDate() const {
    // Take the max of the two underlying curves' max date
    // Extrapolation will be determined by each underlying curve individually
    return max(priceCurve_->maxDate(), discount_->maxDate());
}

const Date& PriceTermStructureAdapter::referenceDate() const {
    QL_REQUIRE(priceCurve_->referenceDate() == discount_->referenceDate(), 
        "PriceTermStructureAdapter: The reference date of the discount curve and price curve should be the same");
    return priceCurve_->referenceDate();
}

DayCounter PriceTermStructureAdapter::dayCounter() const {
    return priceCurve_->dayCounter();
}

const boost::shared_ptr<Quote>& PriceTermStructureAdapter::spotQuote() const {
    return spotQuote_;
}

const boost::shared_ptr<PriceTermStructure>& PriceTermStructureAdapter::priceCurve() const {
    return priceCurve_;
}

const boost::shared_ptr<YieldTermStructure>& PriceTermStructureAdapter::discount() const {
    return discount_;
}

DiscountFactor PriceTermStructureAdapter::discountImpl(Time t) const {
    // Returns discount factor exp(-s(t) * t) where s(t) is defined such that
    // FP(0, t) = S(0) exp([z(t) - s(t)] t)
    Real forwardPrice = priceCurve_->price(t);
    DiscountFactor discount = discount_->discount(t);
    return discount * forwardPrice / spotQuote_->value();
}

}
