/*
 Copyright (C) 2020 Quaternion Risk Managment Ltd
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

#include <qle/instruments/ascot.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>

#include <ql/utilities/null_deleter.hpp>

#include <iostream>

namespace QuantExt {

Ascot::Ascot(Option::Type callPut, const ext::shared_ptr<Exercise>& exercise, const Real& bondQuantity,
             const ext::shared_ptr<ConvertibleBond2>& bond, const Leg& fundingLeg)
    : callPut_(callPut), exercise_(exercise), bondQuantity_(bondQuantity), bond_(bond), fundingLeg_(fundingLeg) {

    registerWith(bond_);
    bond_->alwaysForwardNotifications();

    registerWith(Settings::instance().evaluationDate());
    for (auto& c : fundingLeg_) {
        registerWith(c);
        if (auto lazy = QuantLib::ext::dynamic_pointer_cast<LazyObject>(c))
            lazy->alwaysForwardNotifications();
    }
}

void Ascot::setupArguments(PricingEngine::arguments* args) const {

    Ascot::arguments* arguments = dynamic_cast<Ascot::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");

    arguments->callPut = callPut_;
    arguments->exercise = exercise_;
    arguments->bondQuantity = bondQuantity_;
    arguments->bond = bond_;
    arguments->fundingLeg = fundingLeg_;
}

void Ascot::arguments::validate() const {
    QL_REQUIRE(exercise, "exercise not set");
    QL_REQUIRE(bond, "convertible bond is not set");
    QL_REQUIRE(!fundingLeg.empty(), "no funding leg provided");
    for (Size i = 0; i < fundingLeg.size(); ++i)
        QL_REQUIRE(fundingLeg[i], "null cash flow provided");
}

} // namespace QuantExt
