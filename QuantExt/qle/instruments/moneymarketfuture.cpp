/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <qle/instruments/moneymarketfuture.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/event.hpp>
#include <utility>

namespace QuantExt {
using namespace QuantLib;

    MoneyMarketFuture::MoneyMarketFuture(QuantLib::ext::shared_ptr<QuantLib::IborIndex> index,
                                               const QuantLib::Date& valueDate,
                                               QuantLib::Handle<QuantLib::Quote> convexityAdjustment)
    : index_(std::move(index)), valueDate_(valueDate), convexityAdjustment_(std::move(convexityAdjustment)) {
        QL_REQUIRE(index_, "null index");
        maturityDate_ = index_->maturityDate(valueDate_);
        registerWith(index_);
        registerWith(convexityAdjustment_);
        registerWith(QuantLib::Settings::instance().evaluationDate());
    }

    QuantLib::Real MoneyMarketFuture::rate() const {
        auto ts = index_->forwardingTermStructure();
        QL_REQUIRE(!ts.empty(), "MoneyMarketFuture: no forwarding term structure set in the index");
        auto tau = index_->dayCounter().yearFraction(valueDate_, maturityDate_, valueDate_, maturityDate_);
        return (ts->discount(valueDate_) / ts->discount(maturityDate_) - 1.0) / tau;
    }

    bool MoneyMarketFuture::isExpired() const {
        return QuantLib::detail::simple_event(maturityDate_).hasOccurred();
    }

    QuantLib::Real MoneyMarketFuture::convexityAdjustment() const {
        return convexityAdjustment_.empty() ? 0.0 : convexityAdjustment_->value();
    }

    void MoneyMarketFuture::performCalculations() const {
        double R = convexityAdjustment() + rate();
        NPV_ = 100.0 * (1.0 - R);
    }

}
