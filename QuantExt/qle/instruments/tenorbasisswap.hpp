/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file tenorbasisswap.hpp
    \brief Single currency tenor basis swap instrument

    \ingroup instruments
*/

#ifndef quantext_tenor_basis_swap_hpp
#define quantext_tenor_basis_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>

#include <qle/cashflows/subperiodscoupon.hpp>

using namespace QuantLib;

namespace QuantExt {
//! Single currency tenor basis swap
/*! \ingroup instruments
 */
class TenorBasisSwap : public Swap {
public:
    class results;
    class engine;
    //! \name Constructors
    //@{
    //! Constructor with conventions deduced from the indices
    TenorBasisSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor, bool payLongIndex,
                   const boost::shared_ptr<IborIndex>& longIndex, Spread longSpread,
                   const boost::shared_ptr<IborIndex>& shortIndex, Spread shortSpread, const Period& shortPayTenor,
                   DateGeneration::Rule rule = DateGeneration::Backward, bool includeSpread = false,
                   SubPeriodsCoupon::Type type = SubPeriodsCoupon::Compounding);
    //! Constructor using Schedules with a full interface
    TenorBasisSwap(Real nominal, bool payLongIndex, const Schedule& longSchedule,
                   const boost::shared_ptr<IborIndex>& longIndex, Spread longSpread, const Schedule& shortSchedule,
                   const boost::shared_ptr<IborIndex>& shortIndex, Spread shortSpread, bool includeSpread = false,
                   SubPeriodsCoupon::Type type = SubPeriodsCoupon::Compounding);
    //@}
    //! \name Inspectors
    //@{
    Real nominal() const;
    bool payLongIndex() const;

    const Schedule& longSchedule() const;
    const boost::shared_ptr<IborIndex>& longIndex() const;
    Spread longSpread() const;
    const Leg& longLeg() const;

    const Schedule& shortSchedule() const;
    const boost::shared_ptr<IborIndex>& shortIndex() const;
    Spread shortSpread() const;
    const Period& shortPayTenor() const;
    bool includeSpread() const;
    SubPeriodsCoupon::Type type() const;
    const Leg& shortLeg() const;
    //@}
    //! \name Results
    //@{
    Real longLegBPS() const;
    Real longLegNPV() const;
    Rate fairLongLegSpread() const;

    Real shortLegBPS() const;
    Real shortLegNPV() const;
    Spread fairShortLegSpread() const;
    //@}
    void fetchResults(const PricingEngine::results*) const;

private:
    void initializeLegs();
    void setupExpired() const;

    Real nominal_;
    bool payLongIndex_;

    Schedule longSchedule_;
    boost::shared_ptr<IborIndex> longIndex_;
    Spread longSpread_;

    Schedule shortSchedule_;
    boost::shared_ptr<IborIndex> shortIndex_;
    Spread shortSpread_;
    Period shortPayTenor_;
    bool includeSpread_;
    SubPeriodsCoupon::Type type_;
    Size shortNo_, longNo_;

    mutable Spread fairLongSpread_;
    mutable Spread fairShortSpread_;
};

//! %Results from tenor basis swap calculation
//! \ingroup instruments
class TenorBasisSwap::results : public Swap::results {
public:
    Spread fairLongSpread;
    Spread fairShortSpread;
    void reset();
};

//! \ingroup instruments
class TenorBasisSwap::engine : public GenericEngine<Swap::arguments, TenorBasisSwap::results> {};

// Inline definitions
inline Real TenorBasisSwap::nominal() const { return nominal_; }

inline bool TenorBasisSwap::payLongIndex() const { return payLongIndex_; }

inline const Schedule& TenorBasisSwap::longSchedule() const { return longSchedule_; }

inline const boost::shared_ptr<IborIndex>& TenorBasisSwap::longIndex() const { return longIndex_; }

inline Spread TenorBasisSwap::longSpread() const { return longSpread_; }

inline const Leg& TenorBasisSwap::longLeg() const { return payLongIndex_ ? legs_[0] : legs_[1]; }

inline const Schedule& TenorBasisSwap::shortSchedule() const { return shortSchedule_; }

inline const boost::shared_ptr<IborIndex>& TenorBasisSwap::shortIndex() const { return shortIndex_; }

inline Spread TenorBasisSwap::shortSpread() const { return shortSpread_; }

inline const Period& TenorBasisSwap::shortPayTenor() const { return shortPayTenor_; }

inline bool TenorBasisSwap::includeSpread() const { return includeSpread_; }

inline SubPeriodsCoupon::Type TenorBasisSwap::type() const { return type_; }

inline const Leg& TenorBasisSwap::shortLeg() const { return payLongIndex_ ? legs_[1] : legs_[0]; }
} // namespace QuantExt

#endif
