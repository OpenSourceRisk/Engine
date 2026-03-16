/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/instruments/bondrepo.hpp>

namespace QuantExt {

void BondRepo::arguments::validate() const {
    QL_REQUIRE(!cashLeg.empty(), "BondRepo::validate(): cashLeg is empty");
    QL_REQUIRE(security, "BondRepo::validate(): security is null");
}

BondRepo::BondRepo(const Leg& cashLeg, const bool cashLegPays, const QuantLib::ext::shared_ptr<Bond>& security,
                   const Real securityMultiplier)
    : cashLeg_(cashLeg), cashLegPays_(cashLegPays), security_(security), securityMultiplier_(securityMultiplier) {
    for (auto const& c : cashLeg_)
        registerWith(c);
    registerWith(security_);
}

void BondRepo::deepUpdate() {
    security_->deepUpdate();
    update();
}

bool BondRepo::isExpired() const {
    for (auto const& c : cashLeg_)
        if (!c->hasOccurred())
            return false;
    return true;
}

void BondRepo::setupArguments(QuantLib::PricingEngine::arguments* args) const {
    BondRepo::arguments* arguments = dynamic_cast<BondRepo::arguments*>(args);
    QL_REQUIRE(arguments != 0, "BondRepo::setupArguments(): wrong argument type");
    arguments->cashLeg = cashLeg_;
    arguments->cashLegPays = cashLegPays_;
    arguments->security = security_;
    arguments->securityMultiplier = securityMultiplier_;
}

void BondRepo::fetchResults(const QuantLib::PricingEngine::results* r) const { Instrument::fetchResults(r); }
void BondRepo::setupExpired() const { Instrument::setupExpired(); }

} // namespace QuantExt
