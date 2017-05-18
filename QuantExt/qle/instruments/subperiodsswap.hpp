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

using namespace QuantLib;

namespace QuantExt {
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
                   const boost::shared_ptr<IborIndex>& iborIndex, const DayCounter& floatingDayCount,
                   DateGeneration::Rule rule = DateGeneration::Backward,
                   SubPeriodsCoupon::Type type = SubPeriodsCoupon::Compounding);
    //@}
    //! \name Inspectors
    //@{
    Real nominal() const;
    bool isPayer() const;

    const Schedule& fixedSchedule() const;
    Rate fixedRate() const;
    const Leg& fixedLeg() const;

    const Schedule& floatSchedule() const;
    const boost::shared_ptr<IborIndex>& floatIndex() const;
    SubPeriodsCoupon::Type type() const;
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
    boost::shared_ptr<IborIndex> floatIndex_;
    DayCounter floatDayCount_;
    Period floatPayTenor_;
    SubPeriodsCoupon::Type type_;
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

inline const boost::shared_ptr<IborIndex>& SubPeriodsSwap::floatIndex() const { return floatIndex_; }

inline SubPeriodsCoupon::Type SubPeriodsSwap::type() const { return type_; }

inline const Period& SubPeriodsSwap::floatPayTenor() const { return floatPayTenor_; }

inline const Leg& SubPeriodsSwap::floatLeg() const { return legs_[1]; }

inline Rate SubPeriodsSwap::floatLegBPS() const { return legBPS(1); }

inline Rate SubPeriodsSwap::floatLegNPV() const { return legNPV(1); }
} // namespace QuantExt

#endif
