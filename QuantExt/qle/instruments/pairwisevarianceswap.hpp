/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/pairwisevarianceswap.hpp
    \brief Pirwise Variance swap
*/

#pragma once

#include <qle/instruments/pairwisevarianceswap.hpp>
#include <ql/option.hpp>
#include <ql/position.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Pairwise Variance swap
/*! \ingroup instruments
 */
class PairwiseVarianceSwap : public Instrument {
public:
    class arguments;
    class results;
    class engine;
    PairwiseVarianceSwap(const Position::Type position, const Real strike1, const Real strike2, const Real basketStrike,
                         const Real notional1, const Real notional2, const Real basketNotional, const Real cap,
                         const Real floor, const Real payoffLimit, const int accrualLag,
                         const Schedule valuationSchedule, const Schedule laggedValuationSchedule,
                         const Date settlementDate);
    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    //@}
    //! \name Additional interface
    //@{
    // inspectors
    Position::Type position() const;
    Real strike1() const;
    Real strike2() const;
    Real basketStrike() const;
    Real notional1() const;
    Real notional2() const;
    Real basketNotional() const;
    Real cap() const;
    Real floor() const;
    Real payoffLimit() const;
    int accrualLag() const;
    Schedule valuationSchedule() const;
    Schedule laggedValuationSchedule() const;
    Date settlementDate() const;
    // results
    Real variance1() const;
    Real variance2() const;
    Real basketVariance() const;
    //@}
    // other
    void setupArguments(PricingEngine::arguments* args) const override;
    void fetchResults(const PricingEngine::results*) const override;

protected:
    void setupExpired() const override;
    // data members
    Position::Type position_;
    Real strike1_;
    Real strike2_;
    Real basketStrike_;
    Real notional1_;
    Real notional2_;
    Real basketNotional_;
    Real cap_;
    Real floor_;
    Real payoffLimit_;
    int accrualLag_;
    Schedule valuationSchedule_, laggedValuationSchedule_;
    Date settlementDate_;
    // results
    mutable Real variance1_;
    mutable Real finalVariance1_;
    mutable Real variance2_;
    mutable Real finalVariance2_;
    mutable Real basketVariance_;
    mutable Real finalBasketVariance_;
};

//! %Arguments
class PairwiseVarianceSwap::arguments : public virtual PricingEngine::arguments {
public:
    arguments()
        : strike1(Null<Real>()), strike2(Null<Real>()), basketStrike(Null<Real>()), notional1(Null<Real>()),
          notional2(Null<Real>()), basketNotional(Null<Real>()), cap(Null<Real>()), floor(Null<Real>()),
          payoffLimit(Null<Real>()), accrualLag(Null<int>()) {}
    void validate() const override;
    Position::Type position;
    Real strike1;
    Real strike2;
    Real basketStrike;
    Real notional1;
    Real notional2;
    Real basketNotional;
    Real cap;
    Real floor;
    Real payoffLimit;
    int accrualLag;
    Schedule valuationSchedule;
    Schedule laggedValuationSchedule;
    Date settlementDate;
};

//! %Results from pairwise variance-swap calculation
class PairwiseVarianceSwap::results : public Instrument::results {
public:
    Real variance1, finalVariance1;
    Real variance2, finalVariance2;
    Real basketVariance, finalBasketVariance;
    Real equityAmount1;
    Real equityAmount2;
    Real equityAmountBasket;
    Real pairwiseEquityAmount, finalEquityAmount;
    void reset() override {
        Instrument::results::reset();
        variance1 = Null<Real>();
        variance2 = Null<Real>();
        basketVariance = Null<Real>();
        finalVariance1 = Null<Real>();
        finalVariance2 = Null<Real>();
        finalBasketVariance = Null<Real>();
        finalVariance1 = Null<Real>();
        equityAmount1 = Null<Real>();
        equityAmount2 = Null<Real>();
        equityAmountBasket = Null<Real>();
        pairwiseEquityAmount = Null<Real>();
        finalEquityAmount = Null<Real>();
    }
};

//! base class for pairwise variance-swap engines
class PairwiseVarianceSwap::engine
    : public GenericEngine<QuantExt::PairwiseVarianceSwap::arguments, QuantExt::PairwiseVarianceSwap::results> {};

// inline definitions

inline Position::Type PairwiseVarianceSwap::position() const { return position_; }

inline Real PairwiseVarianceSwap::strike1() const { return strike1_; }

inline Real PairwiseVarianceSwap::strike2() const { return strike2_; }

inline Real PairwiseVarianceSwap::basketStrike() const { return basketStrike_; }

inline Real PairwiseVarianceSwap::notional1() const { return notional1_; }

inline Real PairwiseVarianceSwap::notional2() const { return notional2_; }

inline Real PairwiseVarianceSwap::basketNotional() const { return basketNotional_; }

inline Real PairwiseVarianceSwap::cap() const { return cap_; }

inline Real PairwiseVarianceSwap::floor() const { return floor_; }

inline Real PairwiseVarianceSwap::payoffLimit() const { return payoffLimit_; }

inline int PairwiseVarianceSwap::accrualLag() const { return accrualLag_; }

inline Schedule PairwiseVarianceSwap::valuationSchedule() const { return valuationSchedule_; }

inline Schedule PairwiseVarianceSwap::laggedValuationSchedule() const { return laggedValuationSchedule_; }

inline Date PairwiseVarianceSwap::settlementDate() const { return settlementDate_; }

} // namespace QuantExt
