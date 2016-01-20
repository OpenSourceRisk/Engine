/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/instruments/oibasisswap.hpp
    \brief Overnight index swap paying compounded overnight vs. float
*/

#ifndef quantlib_overnight_indexed_basis_swap_hpp
#define quantlib_overnight_indexed_basis_swap_hpp

#include <ql/instruments/swap.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {

    class Schedule;
    class OvernightIndex;
    class IborIndex;

    //! Overnight indexed basis swap: floating vs compounded overnight rate
    class OvernightIndexedBasisSwap : public Swap {
      public:
        enum Type { Receiver = -1, Payer = 1 };
        OvernightIndexedBasisSwap(
                    Type type,
                    Real nominal,
                    const Schedule& oisSchedule,
                    const boost::shared_ptr<OvernightIndex>& overnightIndex,
                    const Schedule& iborSchedule,
                    const boost::shared_ptr<IborIndex>& iborIndex,
                    Spread oisSpread = 0.0,
                    Spread iborSpread = 0.0);
        OvernightIndexedBasisSwap(
                    Type type,
                    std::vector<Real> nominals,
                    const Schedule& oisSchedule,
                    const boost::shared_ptr<OvernightIndex>& overnightIndex,
                    const Schedule& iborSchedule,
                    const boost::shared_ptr<IborIndex>& iborIndex,
                    Spread oisSpread = 0.0,
                    Spread iborSpread = 0.0);
        //! \name Inspectors
        //@{
        Type type() const { return type_; }
        Real nominal() const;
        std::vector<Real> nominals() const { return nominals_; }
        
        const Schedule& oisSchedule() { return oisSchedule_; }
        const boost::shared_ptr<OvernightIndex>& overnightIndex();

        const Schedule& iborSchedule() { return iborSchedule_; }
        const boost::shared_ptr<IborIndex>& iborIndex();

        Spread oisSpread() { return oisSpread_; }
        Spread iborSpread() { return iborSpread_; }

        const Leg& iborLeg() const { return legs_[0]; }
        const Leg& overnightLeg() const { return legs_[1]; }
        //@}

        //! \name Results
        //@{
        Real iborLegBPS() const;
        Real iborLegNPV() const;
        Real fairIborSpread() const;

        Real overnightLegBPS() const;
        Real overnightLegNPV() const;
        Spread fairOvernightSpread() const;
        //@}
      private:
        void initialize();
        Type type_;
        std::vector<Real> nominals_;
        Schedule oisSchedule_;
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        Schedule iborSchedule_;
        boost::shared_ptr<IborIndex> iborIndex_;
        Spread oisSpread_, iborSpread_;
    };

    // inline

    inline Real OvernightIndexedBasisSwap::nominal() const {
        QL_REQUIRE(nominals_.size()==1, "varying nominals");
        return nominals_[0];
    }

}

#endif
