/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file subperiodsswap.hpp
    \brief Single currency sub periods swap instrument

    \ingroup instruments
*/

#ifndef quantext_sub_periods_swap_hpp
#define quantext_sub_periods_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>

#include <qle/cashflows/subperiodscoupon.hpp>

namespace QuantExt {
using namespace QuantLib;
//! Single currency sub periods swap
/*! \ingroup instruments
 */
class SubPeriodsSwap : public Swap {
public:
    //! \name Constructors
    //@{
    //! Constructor with conventions deduced from the index
    SubPeriodsSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor, bool isPayer,
                   const Period& fixedTenor, Rate fixedRate, const Calendar& fixedCalendar,
                   const DayCounter& fixedDayCount, BusinessDayConvention fixedConvention, const Period& floatPayTenor,
                   const QuantLib::ext::shared_ptr<IborIndex>& iborIndex, const DayCounter& floatingDayCount,
                   DateGeneration::Rule rule = DateGeneration::Backward,
                   QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding);
    //@}
    //! \name Inspectors
    //@{
    Real nominal() const;
    bool isPayer() const;

    const Schedule& fixedSchedule() const;
    Rate fixedRate() const;
    const Leg& fixedLeg() const;

    const Schedule& floatSchedule() const;
    const QuantLib::ext::shared_ptr<IborIndex>& floatIndex() const;
    QuantExt::SubPeriodsCoupon1::Type type() const;
    const Period& floatPayTenor() const;
    const Leg& floatLeg() const;

    //@}
    //! \name Results
    //@{
    Real fairRate() const;
    Real fixedLegBPS() const;
    Real fixedLegNPV() const;
    Real floatLegBPS() const;
    Real floatLegNPV() const;
    //@}

private:
    Real nominal_;
    bool isPayer_;

    Schedule fixedSchedule_;
    Rate fixedRate_;
    DayCounter fixedDayCount_;

    Schedule floatSchedule_;
    QuantLib::ext::shared_ptr<IborIndex> floatIndex_;
    DayCounter floatDayCount_;
    Period floatPayTenor_;
    QuantExt::SubPeriodsCoupon1::Type type_;
};

// Inline definitions
inline Real SubPeriodsSwap::nominal() const { return nominal_; }

inline bool SubPeriodsSwap::isPayer() const { return isPayer_; }

inline const Schedule& SubPeriodsSwap::fixedSchedule() const { return fixedSchedule_; }

inline Rate SubPeriodsSwap::fixedRate() const { return fixedRate_; }

inline const Leg& SubPeriodsSwap::fixedLeg() const { return legs_[0]; }

inline Rate SubPeriodsSwap::fixedLegBPS() const { return legBPS(0); }

inline Rate SubPeriodsSwap::fixedLegNPV() const { return legNPV(0); }

inline const Schedule& SubPeriodsSwap::floatSchedule() const { return floatSchedule_; }

inline const QuantLib::ext::shared_ptr<IborIndex>& SubPeriodsSwap::floatIndex() const { return floatIndex_; }

inline QuantExt::SubPeriodsCoupon1::Type SubPeriodsSwap::type() const { return type_; }

inline const Period& SubPeriodsSwap::floatPayTenor() const { return floatPayTenor_; }

inline const Leg& SubPeriodsSwap::floatLeg() const { return legs_[1]; }

inline Rate SubPeriodsSwap::floatLegBPS() const { return legBPS(1); }

inline Rate SubPeriodsSwap::floatLegNPV() const { return legNPV(1); }

class MakeSubPeriodsSwap {
public:
    MakeSubPeriodsSwap(const Period& swapTenor, const QuantLib::ext::shared_ptr<IborIndex>& index,
        Rate fixedRate, const Period& floatPayTenor, const Period& forwardStart = 0 * Days);

    operator SubPeriodsSwap() const;
    operator QuantLib::ext::shared_ptr<SubPeriodsSwap>() const;

    MakeSubPeriodsSwap& withEffectiveDate(const Date&);
    MakeSubPeriodsSwap& withNominal(Real n);
    MakeSubPeriodsSwap& withIsPayer(bool p);
    MakeSubPeriodsSwap& withSettlementDays(Natural settlementDays);

    MakeSubPeriodsSwap& withFixedLegTenor(const Period& t);
    MakeSubPeriodsSwap& withFixedLegCalendar(const Calendar& cal);
    MakeSubPeriodsSwap& withFixedLegConvention(BusinessDayConvention bdc);
    MakeSubPeriodsSwap& withFixedLegRule(DateGeneration::Rule r);
    MakeSubPeriodsSwap& withFixedLegDayCount(const DayCounter& dc);

    MakeSubPeriodsSwap& withSubCouponsType(const QuantExt::SubPeriodsCoupon1::Type& st);

    MakeSubPeriodsSwap& withDiscountingTermStructure(const Handle<YieldTermStructure>& discountCurve);
    MakeSubPeriodsSwap& withPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine);

private:
    Period swapTenor_;
    QuantLib::ext::shared_ptr<IborIndex> index_;
    Rate fixedRate_;
    Period floatPayTenor_;
    Period forwardStart_;

    Date effectiveDate_;
    Real nominal_;
    bool isPayer_;
    Natural settlementDays_;

    Period fixedTenor_;
    Calendar fixedCalendar_;
    BusinessDayConvention fixedConvention_;
    DateGeneration::Rule fixedRule_;
    DayCounter fixedDayCount_, floatDayCounter_;

    QuantExt::SubPeriodsCoupon1::Type subCouponsType_;

    QuantLib::ext::shared_ptr<PricingEngine> engine_;
};
} // namespace QuantExt

#endif
