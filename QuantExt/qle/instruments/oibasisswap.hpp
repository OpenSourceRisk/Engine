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

/*! \file qle/instruments/oibasisswap.hpp
    \brief Overnight index swap paying compounded overnight vs. float

        \ingroup instruments
*/

#ifndef quantlib_overnight_indexed_basis_swap_hpp
#define quantlib_overnight_indexed_basis_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {

// class Schedule;
// class OvernightIndex;
// class IborIndex;

//! Overnight indexed basis swap: floating vs compounded overnight rate
/*! \ingroup instruments
 */
class OvernightIndexedBasisSwap : public Swap {
public:
    enum Type { Receiver = -1, Payer = 1 };
    OvernightIndexedBasisSwap(Type type, Real nominal, const Schedule& oisSchedule,
                              const boost::shared_ptr<OvernightIndex>& overnightIndex, const Schedule& iborSchedule,
                              const boost::shared_ptr<IborIndex>& iborIndex, Spread oisSpread = 0.0,
                              Spread iborSpread = 0.0);
    OvernightIndexedBasisSwap(Type type, std::vector<Real> nominals, const Schedule& oisSchedule,
                              const boost::shared_ptr<OvernightIndex>& overnightIndex, const Schedule& iborSchedule,
                              const boost::shared_ptr<IborIndex>& iborIndex, Spread oisSpread = 0.0,
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
    QL_REQUIRE(nominals_.size() == 1, "varying nominals");
    return nominals_[0];
}
} // namespace QuantExt

#endif
