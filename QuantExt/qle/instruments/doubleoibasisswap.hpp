/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/doubleoibasisswap.hpp
    \brief Overnight index swap paying compounded overnight vs.
    another compounded overnight

        \ingroup instruments
*/

#ifndef quantlib_double_overnight_indexed_basis_swap_hpp
#define quantlib_double_overnight_indexed_basis_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Double Overnight indexed basis swap: compounded overnight rate vs compounded overnight rate
/*! \ingroup instruments
 */
class DoubleOvernightIndexedBasisSwap : public Swap {
public:
    DoubleOvernightIndexedBasisSwap(Real nominal, 
        const Schedule& paySchedule, const boost::shared_ptr<OvernightIndex>& payIndex, 
        const Schedule& recSchedule, const boost::shared_ptr<OvernightIndex>& recIndex, 
        Spread paySpread = 0.0, Spread recSpread = 0.0, const bool telescopicValueDates = false);
    DoubleOvernightIndexedBasisSwap(std::vector<Real> nominals, const Schedule& paySchedule,
                              const boost::shared_ptr<OvernightIndex>& payIndex, const Schedule& recSchedule,
                              const boost::shared_ptr<OvernightIndex>& recIndex, Spread paySpread = 0.0,
                              Spread secondLegSpread = 0.0, const bool telescopicValueDates = false);
    //! \name Inspectors
    //@{
    Type type() const { return type_; }
    Real nominal() const;
    std::vector<Real> nominals() const { return nominals_; }

    const Schedule& paySchedule() { return paySchedule_; }
    const boost::shared_ptr<OvernightIndex>& payIndex() { return payIndex_; }

    const Schedule& recSchedule() { return recSchedule_; }
    const boost::shared_ptr<OvernightIndex>& recIndex() { return recIndex_; }

    Spread paySpread() { return paySpread_; }
    Spread secondLegSpread() { return recSpread_; }

    const Leg& payLeg() const { return legs_[0]; }
    const Leg& recLeg() const { return legs_[1]; }
    //@}

    //! \name Results
    //@{
    Real payBPS() const;
    Real payNPV() const;
    Real fairPaySpread() const;

    Real recBPS() const;
    Real recNPV() const;
    Spread fairRecSpread() const;
    //@}
private:
    void initialize();
    Type type_;
    std::vector<Real> nominals_;
    Schedule paySchedule_;
    boost::shared_ptr<OvernightIndex> payIndex_;
    Schedule recSchedule_;
    boost::shared_ptr<OvernightIndex> recIndex_;
    Spread paySpread_, recSpread_;
    bool telescopicValueDates_;
};

// inline

inline Real DoubleOvernightIndexedBasisSwap::nominal() const {
    QL_REQUIRE(nominals_.size() == 1, "varying nominals");
    return nominals_[0];
}
} // namespace QuantExt

#endif
