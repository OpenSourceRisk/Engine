/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file creditlinkedswap.hpp
    \brief credit linked swap instrument
*/

#pragma once

#include <ql/instruments/creditdefaultswap.hpp>

#include <ql/cashflow.hpp>
#include <ql/currency.hpp>
#include <ql/instrument.hpp>

namespace QuantExt {
using namespace QuantLib;
  
class CreditLinkedSwap : public QuantLib::Instrument {
public:
    enum class LegType { IndependentPayments, ContingentPayments, DefaultPayments, RecoveryPayments };

    CreditLinkedSwap(const std::vector<Leg>& legs, const std::vector<bool>& legPayers,
                     const std::vector<LegType>& legTypes, const bool settlesAccrual, const Real fixedRecoveryRate,
                     const QuantExt::CreditDefaultSwap::ProtectionPaymentTime& defaultPaymentTime,
                     const Currency& currency);

    struct arguments : public QuantLib::PricingEngine::arguments {
        void validate() const override {}
        std::vector<Leg> legs;
        std::vector<bool> legPayers;
        std::vector<LegType> legTypes;
        Date maturityDate;
        Currency currency;
        bool settlesAccrual;
        Real fixedRecoveryRate;
        QuantExt::CreditDefaultSwap::ProtectionPaymentTime defaultPaymentTime;
    };

    using results = QuantLib::Instrument::results;
    using engine = QuantLib::GenericEngine<arguments, results>;

    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;

    Date maturity() const;

private:
    std::vector<Leg> legs_;
    std::vector<bool> legPayers_;
    std::vector<LegType> legTypes_;
    bool settlesAccrual_;
    Real fixedRecoveryRate_;
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime defaultPaymentTime_;
    Currency currency_;
};

} // namespace QuantExt
