/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

// Phase 3 – additional QuantExt instrument bindings
// (Task 2: bond instruments)

#ifndef qle_instruments_ext_i
#define qle_instruments_ext_i

%include instruments.i
%include bonds.i
%include qle_common_typemaps.i

%{
#include <qle/instruments/bondoption.hpp>
#include <qle/instruments/bondrepo.hpp>
#include <qle/instruments/forwardbond.hpp>
%}

// Rename QuantExt instrument classes to avoid collision with ore::data trade
// types of the same name in ored_portfolio_trades.i.
%rename(QleBondOption)  QuantExt::BondOption;
%rename(QleBondRepo)    QuantExt::BondRepo;
%rename(QleForwardBond) QuantExt::ForwardBond;

// ---------------------------------------------------------------------------
// BondOption
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::BondOption)
namespace QuantExt {
class BondOption : public QuantLib::Instrument {
  public:
    BondOption(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying,
               const QuantLib::CallabilitySchedule& putCallSchedule,
               const bool knocksOutOnDefault = false);
    const QuantLib::CallabilitySchedule& callability() const;
};
}

// ---------------------------------------------------------------------------
// BondRepo
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::BondRepo)
namespace QuantExt {
class BondRepo : public QuantLib::Instrument {
  public:
    BondRepo(const QuantLib::Leg& cashLeg,
             const bool cashLegPays,
             const QuantLib::ext::shared_ptr<QuantLib::Bond>& security,
             const QuantLib::Real securityMultiplier);
    const QuantLib::Leg& cashLeg() const;
    bool cashLegPays() const;
    QuantLib::ext::shared_ptr<QuantLib::Bond> security() const;
    QuantLib::Real securityMultiplier() const;
};
}

// ---------------------------------------------------------------------------
// ForwardBond  – two constructors disambiguated via %extend factory
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::ForwardBond)
namespace QuantExt {
class ForwardBond : public QuantLib::Instrument {
  public:
    // Vanilla forward bond constructor
    ForwardBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying,
                QuantLib::Real strikeAmount,
                const QuantLib::Date& fwdMaturityDate,
                const QuantLib::Date& fwdSettlementDate,
                bool isPhysicallySettled,
                bool knockOut,
                bool settlementDirty,
                QuantLib::Real compensationPayment,
                const QuantLib::Date compensationPaymentDate,
                bool isLong,
                QuantLib::Real bondNotional = 1.0);

    const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying();
};
}

%extend QuantExt::ForwardBond {
    // T-Lock (lock-rate) variant exposed as a named factory function
    static QuantLib::ext::shared_ptr<QuantExt::ForwardBond> createTLock(
        const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying,
        QuantLib::Real lockRate,
        const QuantLib::DayCounter& lockRateDayCounter,
        bool longInForward,
        const QuantLib::Date& fwdMaturityDate,
        const QuantLib::Date& fwdSettlementDate,
        bool isPhysicallySettled,
        bool knockOut,
        bool settlementDirty,
        QuantLib::Real compensationPayment,
        const QuantLib::Date compensationPaymentDate,
        bool isLong,
        QuantLib::Real bondNotional = 1.0,
        QuantLib::Real dv01 = QuantLib::Null<QuantLib::Real>())
    {
        return QuantLib::ext::make_shared<QuantExt::ForwardBond>(
            underlying, lockRate, lockRateDayCounter, longInForward,
            fwdMaturityDate, fwdSettlementDate, isPhysicallySettled, knockOut,
            settlementDirty, compensationPayment, compensationPaymentDate,
            isLong, bondNotional, dv01);
    }
}

#endif
