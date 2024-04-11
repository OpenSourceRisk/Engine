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

/*! \file qle/instruments/bondrepo.hpp
    \brief bond repo instrument
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/pricingengine.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Bond repo instrument
class BondRepo : public QuantLib::Instrument {
public:
    class arguments : public QuantLib::PricingEngine::arguments {
    public:
        Leg cashLeg;
        bool cashLegPays;
        QuantLib::ext::shared_ptr<Bond> security;
        Real securityMultiplier;
        void validate() const override;
    };
    using results = QuantLib::Instrument::results;
    using engine = QuantLib::GenericEngine<arguments, results>;
    BondRepo(const Leg& cashLeg, const bool cashLegPays, const QuantLib::ext::shared_ptr<Bond>& security,
             const Real securityMultiplier);

    // Observable interface
    void deepUpdate() override;

    // Instrument interface
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;

    //! Inspectors
    const Leg& cashLeg() const { return cashLeg_; }
    bool cashLegPays() const { return cashLegPays_; }
    QuantLib::ext::shared_ptr<Bond> security() const { return security_; }
    Real securityMultiplier() const { return securityMultiplier_; }

private:
    const Leg cashLeg_;
    const bool cashLegPays_;
    const QuantLib::ext::shared_ptr<Bond> security_;
    const Real securityMultiplier_;

    // Instrument interface
    void setupExpired() const override;
};

//! %Arguments for Bond repo

} // namespace QuantExt
