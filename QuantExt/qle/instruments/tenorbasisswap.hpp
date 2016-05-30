/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file tenorbasisswap.hpp
    \brief Single currency tenor basis swap instrument
*/

#ifndef quantext_tenor_basis_swap_hpp
#define quantext_tenor_basis_swap_hpp

#include <ql/instruments/swap.hpp>
#include <ql/indexes/iborindex.hpp>

#include <qle/cashflows/subperiodscoupon.hpp>

using namespace QuantLib;

namespace QuantExt {
    //! Single currency tenor basis swap
    class TenorBasisSwap : public Swap {
      public:
        class results;
        class engine;
        //! \name Constructors
        //@{
        //! Constructor with conventions deduced from the indices
        TenorBasisSwap(const Date& effectiveDate,
            Real nominal,
            const Period& swapTenor,
            bool payLongIndex,
            const boost::shared_ptr<IborIndex>& longIndex,
            Spread longSpread,
            const boost::shared_ptr<IborIndex>& shortIndex,
            Spread shortSpread,
            const Period& shortPayTenor,
            DateGeneration::Rule rule = DateGeneration::Backward,
            bool includeSpread = false,
            SubPeriodsCoupon::Type type = SubPeriodsCoupon::Compounding);
        //! Constructor using Schedules with a full interface
        TenorBasisSwap(Real nominal,
            bool payLongIndex,
            const Schedule& longSchedule,
            const boost::shared_ptr<IborIndex>& longIndex,
            Spread longSpread,
            const Schedule& shortSchedule,
            const boost::shared_ptr<IborIndex>& shortIndex,
            Spread shortSpread,
            bool includeSpread = false,
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
    class TenorBasisSwap::results : public Swap::results {
      public:
        Spread fairLongSpread;
        Spread fairShortSpread;
        void reset();
    };

    class TenorBasisSwap::engine :
        public GenericEngine<Swap::arguments, TenorBasisSwap::results> {};

    // Inline definitions
    inline Real TenorBasisSwap::nominal() const {
        return nominal_;
    }

    inline bool TenorBasisSwap::payLongIndex() const {
        return payLongIndex_;
    }

    inline const Schedule& TenorBasisSwap::longSchedule() const {
        return longSchedule_;
    }

    inline const boost::shared_ptr<IborIndex>&
        TenorBasisSwap::longIndex() const {
        return longIndex_;
    }

    inline Spread TenorBasisSwap::longSpread() const {
        return longSpread_;
    }

    inline const Leg& TenorBasisSwap::longLeg() const {
        return payLongIndex_ ? legs_[0] : legs_[1];
    }

    inline const Schedule& TenorBasisSwap::shortSchedule() const {
        return shortSchedule_;
    }

    inline const boost::shared_ptr<IborIndex>&
        TenorBasisSwap::shortIndex() const {
        return shortIndex_;
    }

    inline Spread TenorBasisSwap::shortSpread() const {
        return shortSpread_;
    }

    inline const Period& TenorBasisSwap::shortPayTenor() const {
        return shortPayTenor_;
    }

    inline bool TenorBasisSwap::includeSpread() const {
        return includeSpread_;
    }

    inline SubPeriodsCoupon::Type TenorBasisSwap::type() const {
        return type_;
    }

    inline const Leg& TenorBasisSwap::shortLeg() const {
        return payLongIndex_ ? legs_[1] : legs_[0];
    }
}

#endif
