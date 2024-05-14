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

namespace QuantExt {
using namespace QuantLib;
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
    TenorBasisSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor,
                   const QuantLib::ext::shared_ptr<IborIndex>& payIndex, Spread paySpread, const Period& payFrequency,
                   const QuantLib::ext::shared_ptr<IborIndex>& recIndex, Spread recSpread, const Period& recFrequency,
                   DateGeneration::Rule rule = DateGeneration::Backward, bool includeSpread = false,
                   bool spreadOnRec = true,
                   QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding,
                   const bool telescopicValueDates = false);
    //! Constructor using Schedules with a full interface
    TenorBasisSwap(Real nominal, const Schedule& paySchedule, const QuantLib::ext::shared_ptr<IborIndex>& payIndex,
                   Spread paySpread, const Schedule& recSchedule, const QuantLib::ext::shared_ptr<IborIndex>& recIndex,
                   Spread recSpread, bool includeSpread = false, bool spreadOnRec = true,
                   QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding,
                   const bool telescopicValueDates = false);
    TenorBasisSwap(const std::vector<Real>& nominals, const Schedule& paySchedule, const QuantLib::ext::shared_ptr<IborIndex>& payIndex,
                   Spread paySpread, const Schedule& recSchedule, const QuantLib::ext::shared_ptr<IborIndex>& recIndex,
                   Spread recSpread, bool includeSpread = false, bool spreadOnRec = true,
                   QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding,
                   const bool telescopicValueDates = false);

    //@}
    //! \name Inspectors
    //@{
    Real nominal() const;
    const std::vector<Real>& nominals() const { return nominals_; }

    const Schedule& paySchedule() const;
    const QuantLib::ext::shared_ptr<IborIndex>& payIndex() const;
    Spread paySpread() const;
    const Leg& payLeg() const;

    const Schedule& recSchedule() const;
    const QuantLib::ext::shared_ptr<IborIndex>& recIndex() const;
    Spread recSpread() const;
    const Leg& recLeg() const;
    const Period& recFrequency() const;
    const Period& payFrequency() const;

    bool includeSpread() const;
    bool spreadOnRec() const { return spreadOnRec_; }
    QuantExt::SubPeriodsCoupon1::Type type() const;
    //@}
    //! \name Results
    //@{
    Real payLegBPS() const;
    Real payLegNPV() const;
    Rate fairPayLegSpread() const;

    Real recLegBPS() const;
    Real recLegNPV() const;
    Spread fairRecLegSpread() const;
    //@}
    void fetchResults(const PricingEngine::results*) const override;

private:
    void initializeLegs();
    void setupExpired() const override;

    std::vector<Real> nominals_;

    Schedule paySchedule_;
    QuantLib::ext::shared_ptr<IborIndex> payIndex_;
    Spread paySpread_;
    Period payFrequency_;

    Schedule recSchedule_;
    QuantLib::ext::shared_ptr<IborIndex> recIndex_;
    Spread recSpread_;
    Period recFrequency_;

    bool includeSpread_;
    bool spreadOnRec_;
    QuantExt::SubPeriodsCoupon1::Type type_;
    bool telescopicValueDates_;

    bool noSubPeriod_;

    mutable std::vector<Spread> fairSpread_;

    Calendar recIndexCalendar_, payIndexCalendar_;
    Size idxRec_, idxPay_;
};

//! %Results from tenor basis swap calculation
//! \ingroup instruments
class TenorBasisSwap::results : public Swap::results {
public:
    std::vector<Spread> fairSpread;
    void reset() override;
};

//! \ingroup instruments
class TenorBasisSwap::engine : public GenericEngine<Swap::arguments, TenorBasisSwap::results> {};

// Inline definitions
inline Real TenorBasisSwap::nominal() const {
    QL_REQUIRE(nominals_.size() == 1, "varying nominals");
    return nominals_[0];
}
inline const Schedule& TenorBasisSwap::paySchedule() const { return paySchedule_; }

inline const QuantLib::ext::shared_ptr<IborIndex>& TenorBasisSwap::payIndex() const { return payIndex_; }

inline Spread TenorBasisSwap::paySpread() const { return paySpread_; }

inline const Leg& TenorBasisSwap::payLeg() const { return legs_[idxPay_]; }

inline const Schedule& TenorBasisSwap::recSchedule() const { return recSchedule_; }

inline const QuantLib::ext::shared_ptr<IborIndex>& TenorBasisSwap::recIndex() const { return recIndex_; }

inline Spread TenorBasisSwap::recSpread() const { return recSpread_; }

inline const Period& TenorBasisSwap::recFrequency() const { return recFrequency_; }

inline const Period& TenorBasisSwap::payFrequency() const { return payFrequency_; }

inline bool TenorBasisSwap::includeSpread() const { return includeSpread_; }

inline QuantExt::SubPeriodsCoupon1::Type TenorBasisSwap::type() const { return type_; }

inline const Leg& TenorBasisSwap::recLeg() const { return legs_[idxRec_]; }
} // namespace QuantExt

#endif
