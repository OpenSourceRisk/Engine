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

/*! \file oiccbasisswap.hpp
    \brief Cross currency overnight index swap paying compounded overnight vs. float

        \ingroup instruments
*/

#ifndef quantlib_cc_ois_basis_swap_hpp
#define quantlib_cc_ois_basis_swap_hpp

#include <ql/currency.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {

// class Schedule;
// class OvernightIndex;
// class IborIndex;

//! Basis swap: compounded overnight rate in ccy 1 vs. compounded overnight rate in ccy 2
/*! \ingroup instruments
 */
class OvernightIndexedCrossCcyBasisSwap : public Swap {
public:
    class arguments;
    class results;
    class engine;
    OvernightIndexedCrossCcyBasisSwap(Real payNominal, Currency payCurrency, const Schedule& paySchedule,
                                      const boost::shared_ptr<OvernightIndex>& payIndex, Real paySpread,
                                      Real recNominal, Currency recCurrency, const Schedule& recSchedule,
                                      const boost::shared_ptr<OvernightIndex>& recIndex, Real recSpread);
    //! \name Inspectors
    //@{
    Real payNominal() const;
    Currency payCurrency() const;
    const Schedule& paySchedule() { return paySchedule_; }
    const boost::shared_ptr<OvernightIndex>& payIndex() { return payIndex_; }
    Real paySpread() const;

    Real recNominal() const;
    Currency recCurrency() const;
    const Schedule& recSchedule() { return recSchedule_; }
    const boost::shared_ptr<OvernightIndex>& recIndex() { return recIndex_; }
    Real recSpread() const;

    const Leg& payLeg() const { return legs_[0]; }
    const Leg& recLeg() const { return legs_[1]; }
    //@}

    //! \name Results
    //@{
    Real payLegBPS() const;
    Real payLegNPV() const;
    Real fairPayLegSpread() const;

    Real recLegBPS() const;
    Real recLegNPV() const;
    Spread fairRecLegSpread() const;
    //@}
    // other
    void setupArguments(PricingEngine::arguments* args) const;
    void fetchResults(const PricingEngine::results*) const;

private:
    void initialize();
    Real payNominal_, recNominal_;
    Currency payCurrency_, recCurrency_;
    Schedule paySchedule_, recSchedule_;
    boost::shared_ptr<OvernightIndex> payIndex_, recIndex_;
    Spread paySpread_, recSpread_;
    std::vector<Currency> currency_;

    mutable Real fairPayLegSpread_;
    mutable Real fairRecLegSpread_;
};

// inline

inline Real OvernightIndexedCrossCcyBasisSwap::payNominal() const { return payNominal_; }

inline Real OvernightIndexedCrossCcyBasisSwap::recNominal() const { return recNominal_; }

inline Currency OvernightIndexedCrossCcyBasisSwap::payCurrency() const { return payCurrency_; }

inline Currency OvernightIndexedCrossCcyBasisSwap::recCurrency() const { return recCurrency_; }

inline Real OvernightIndexedCrossCcyBasisSwap::paySpread() const { return paySpread_; }

inline Real OvernightIndexedCrossCcyBasisSwap::recSpread() const { return recSpread_; }

//! \ingroup instruments
class OvernightIndexedCrossCcyBasisSwap::arguments : public Swap::arguments {
public:
    std::vector<Currency> currency;
    Real paySpread;
    Real recSpread;
};

//! \ingroup instruments
class OvernightIndexedCrossCcyBasisSwap::results : public Swap::results {
public:
    Real fairPayLegSpread;
    Real fairRecLegSpread;
    void reset();
};

//! \ingroup instruments
class OvernightIndexedCrossCcyBasisSwap::engine
    : public GenericEngine<OvernightIndexedCrossCcyBasisSwap::arguments, OvernightIndexedCrossCcyBasisSwap::results> {};
} // namespace QuantExt

#endif
