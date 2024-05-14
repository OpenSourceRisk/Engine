/*
 Copyright (C) 2020 Quaternion Risk Management Ltd.
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

/*! \file cbo.hpp
    \brief collateralized bond obligation instrument
*/

#pragma once

#include <ql/event.hpp>
#include <ql/instrument.hpp>

#include <qle/cashflows/cashflowtable.hpp>
#include <qle/instruments/bondbasket.hpp>
#include <ql/time/schedule.hpp>
namespace QuantExt {
using namespace QuantLib;

//! Collateralized Bond Obligation, Cash Flow CBO
/*!
  This class holds the term sheet information on a generic Cashflow CBO
  with arbitrary number of tranches.

  The underlying bond basket is assumed to be in a single currency.

  \ingroup instruments
*/
struct Tranche {
    std::string name;
    double faceAmount;
    double icRatio;
    double ocRatio;
    QuantLib::Leg leg;
};

class CBO : public Instrument {
public:
    class arguments;
    class results;
    class engine;
    //! \name Constructors
    //@{
    CBO( //! Underlying bond basket
        const QuantLib::ext::shared_ptr<BondBasket>& basket,
        //! CBO schedule
        const QuantLib::Schedule& schedule,
        //! Senior fee rate to be paid before any cash flow goes to the tranches
        Rate seniorFee,
        //! Fee day counter
        const DayCounter& feeDayCounter,
        //! Tranche description
        const std::vector<Tranche>& tranches,
        //! Subordinated fee rate to be paid late in the waterfal
        Rate subordinatedFee,
        //! Equity kicker
        Rate equityKicker,
        //! CBOs currency
        const Currency& ccy,
        //! invested trancheName
        const std::string& investedTrancheName);
    //@}
    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<BondBasket> basket() const { return basket_; }
    //@}

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    void fetchResults(const PricingEngine::results*) const override;
    //@}

    //! \name Results
    //@{
    Real basketValue() const;
    std::vector<Real> trancheValue() const;
    Rate feeValue() const;
    Rate subfeeValue() const;
    Real basketValueStd() const;
    std::vector<Real> trancheValueStd() const;
    Rate feeValueStd() const;
    Rate subfeeValueStd() const;
    const std::vector<CashflowTable>& trancheCashflows() const;
    //@}

private:
    void setupExpired() const override;

    QuantLib::ext::shared_ptr<BondBasket> basket_;
    Schedule schedule_;
    Rate seniorFee_;
    DayCounter feeDayCounter_;
    std::vector<Tranche> tranches_;
    Rate subordinatedFee_;
    Rate equityKicker_;
    Currency ccy_;
    std::string investedTrancheName_;

    mutable Real basketValue_;
    mutable std::vector<Real> trancheValue_;
    mutable Real feeValue_;
    mutable Real subfeeValue_;
    mutable Real basketValueStd_;
    mutable std::vector<Real> trancheValueStd_;
    mutable Real feeValueStd_;
    mutable Real subfeeValueStd_;
    mutable std::vector<CashflowTable> trancheCashflows_;
};

class CBO::arguments : public virtual PricingEngine::arguments {
public:
    void validate() const override;

    QuantLib::ext::shared_ptr<BondBasket> basket;
    Schedule schedule;
    Rate seniorFee;
    Rate subordinatedFee;
    DayCounter feeDayCounter;
    std::vector<Tranche> tranches;
    Real equityKicker;
    Currency ccy;
    std::string investedTrancheName;

};

class CBO::results : public Instrument::results {
public:
    void reset() override;
    Real basketValue;
    std::vector<Real> trancheValue;
    Real feeValue;
    Real subfeeValue;
    Real basketValueStd;
    std::vector<Real> trancheValueStd;
    Real feeValueStd;
    Real subfeeValueStd;
    std::vector<CashflowTable> trancheCashflows;
};

} // namespace QuantExt
