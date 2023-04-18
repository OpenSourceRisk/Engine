/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/instruments/forwardrateagreement.hpp>

namespace QuantExt {

ForwardRateAgreement::ForwardRateAgreement(const Date& valueDate,
                                           const Date& maturityDate,
                                           Position::Type type,
                                           Rate strikeForwardRate,
                                           Real notionalAmount,
                                           const ext::shared_ptr<IborIndex>& index,
                                           Handle<YieldTermStructure> discountCurve,
                                           bool useIndexedCoupon)
    : QuantLib::ForwardRateAgreement(valueDate, maturityDate, type, strikeForwardRate, notionalAmount,
                                     index, discountCurve, useIndexedCoupon) {}

ForwardRateAgreement::ForwardRateAgreement(const Date& valueDate,
                                           Position::Type type,
                                           Rate strikeForwardRate,
                                           Real notionalAmount,
                                           const ext::shared_ptr<IborIndex>& index,
                                           Handle<YieldTermStructure> discountCurve)
    : QuantLib::ForwardRateAgreement(valueDate, type, strikeForwardRate, notionalAmount,
                                     index, discountCurve) {}

void ForwardRateAgreement::setupArguments(PricingEngine::arguments* args) const {
    auto* arguments = dynamic_cast<ForwardRateAgreement::arguments*>(args);
    QL_REQUIRE(arguments != nullptr, "wrong argument type");

    arguments->type = fraType_;
    arguments->notionalAmount = notionalAmount_;
    arguments->index = index_;
    arguments->valueDate = valueDate_;
    arguments->maturityDate = maturityDate_;
    arguments->discountCurve = discountCurve_;
    arguments->strikeForwardRate = strikeForwardRate_;
}

void ForwardRateAgreement::performCalculations() const {
    if (engine_) // engine is optional for FRA instrument
        Instrument::performCalculations();
    QuantLib::ForwardRateAgreement::performCalculations();
}

} // namespace QuantExt
