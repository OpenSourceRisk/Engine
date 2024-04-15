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

/*! \file fixedbmaswap.hpp
    \brief fixed vs averaged bma swap

    \ingroup instruments
*/

#ifndef quantlib_makefixedbmaswap_hpp
#define quantlib_makefixedbmaswap_hpp

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/instruments/swap.hpp>

namespace QuantExt {
using namespace QuantLib;

//! swap paying a fixed rate against BMA coupons
class FixedBMASwap : public Swap {
public:
    class results;
    class engine;
    enum Type { Receiver = -1, Payer = 1 };
    FixedBMASwap(Type type, Real nominal,
                 // Fixed leg
                 const Schedule& fixedSchedule, Rate fixedRate, const DayCounter& fixedDayCount,
                 // BMA leg
                 const Schedule& bmaSchedule, const QuantLib::ext::shared_ptr<BMAIndex>& bmaIndex,
                 const DayCounter& bmaDayCount);

    //! \name Inspectors
    //@{
    Real fixedRate() const;
    Real nominal() const;
    //! "payer" or "receiver" refer to the BMA leg
    Type type() const;
    const Leg& bmaLeg() const;
    const Leg& fixedLeg() const;
    //@}

    //! \name Results
    //@{
    Real fixedLegBPS() const;
    Real fixedLegNPV() const;
    Rate fairRate() const;

    Real bmaLegBPS() const;
    Real bmaLegNPV() const;
    //@}
    // results
    void fetchResults(const PricingEngine::results*) const override;

private:
    Type type_;
    Real nominal_;
    Rate fixedRate_;
    // results
    mutable Rate fairRate_;
};

class FixedBMASwap::results : public Swap::results {
public:
    Rate fairRate;
    void reset() override;
};

class FixedBMASwap::engine : GenericEngine<FixedBMASwap::arguments, FixedBMASwap::results> {};
// factory class for making fixed vs bma swaps

class MakeFixedBMASwap {
public:
    MakeFixedBMASwap(const Period& swapTenor, const QuantLib::ext::shared_ptr<BMAIndex>& bmaIndex,
                     Rate fixedRate = Null<Rate>(), const Period& forwardStart = 0 * Days);

    operator FixedBMASwap() const;
    operator QuantLib::ext::shared_ptr<FixedBMASwap>() const;

    // We regard the BMA leg parameters as fixed by convention apart from tenor
    // and provide a factory for the fixed leg per MakeVanillaSwap

    MakeFixedBMASwap& receiveFixed(bool flag = true);
    MakeFixedBMASwap& withType(FixedBMASwap::Type type);
    MakeFixedBMASwap& withNominal(Real n);
    MakeFixedBMASwap& withBMALegTenor(const Period& tenor);

    MakeFixedBMASwap& withSettlementDays(Natural settlementDays);
    MakeFixedBMASwap& withEffectiveDate(const Date&);
    MakeFixedBMASwap& withTerminationDate(const Date&);

    MakeFixedBMASwap& withFixedLegTenor(const Period& t);
    MakeFixedBMASwap& withFixedLegCalendar(const Calendar& cal);
    MakeFixedBMASwap& withFixedLegConvention(BusinessDayConvention bdc);
    MakeFixedBMASwap& withFixedLegTerminationDateConvention(BusinessDayConvention bdc);
    MakeFixedBMASwap& withFixedLegRule(DateGeneration::Rule r);
    MakeFixedBMASwap& withFixedLegEndOfMonth(bool flag = true);
    MakeFixedBMASwap& withFixedLegFirstDate(const Date& d);
    MakeFixedBMASwap& withFixedLegNextToLastDate(const Date& d);
    MakeFixedBMASwap& withFixedLegDayCount(const DayCounter& dc);

    MakeFixedBMASwap& withDiscountingTermStructure(const Handle<YieldTermStructure>& discountCurve);
    MakeFixedBMASwap& withPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine);

private:
    Period swapTenor_;
    QuantLib::ext::shared_ptr<BMAIndex> bmaIndex_;
    Rate fixedRate_;
    Period fixedTenor_;
    Period forwardStart_;

    Natural settlementDays_;
    Date effectiveDate_, terminationDate_;
    Calendar fixedCalendar_, bmaCalendar_;

    FixedBMASwap::Type type_;
    Real nominal_;
    Period bmaLegTenor_;
    BusinessDayConvention fixedConvention_, fixedTerminationDateConvention_;
    BusinessDayConvention bmaConvention_, bmaTerminationDateConvention_;
    DateGeneration::Rule fixedRule_, bmaRule_;
    bool fixedEndOfMonth_, bmaEndOfMonth_;
    Date fixedFirstDate_, fixedNextToLastDate_;
    Date bmaFirstDate_, bmaNextToLastDate_;
    DayCounter fixedDayCount_, bmaDayCount_;

    QuantLib::ext::shared_ptr<PricingEngine> engine_;
};
} // namespace QuantExt

#endif
