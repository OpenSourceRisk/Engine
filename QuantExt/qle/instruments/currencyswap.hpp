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

/*! \file qle/instruments/currencyswap.hpp
    \brief Interest rate swap with extended interface

        \ingroup instruments
*/

#ifndef quantext_currencyswap_hpp
#define quantext_currencyswap_hpp

#include <ql/cashflows/cashflows.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/currency.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/money.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {

//! %Currency Interest Rate Swap
/*!
  This instrument generalizes the QuantLib Swap instrument in that
  it allows multiple legs with different currencies (one per leg)

  \ingroup instruments
*/
class CurrencySwap : public Instrument {
public:
    class arguments;
    class results;
    class engine;
    //! \name Constructors
    //@{
    /*! Multi leg constructor. */
    CurrencySwap(const std::vector<Leg>& legs, const std::vector<bool>& payer, const std::vector<Currency>& currency);
    //@}
    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}
    //! \name Additional interface
    //@{
    Date startDate() const;
    Date maturityDate() const;
    Real legBPS(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg# " << j << " doesn't exist!");
        calculate();
        return legBPS_[j];
    }
    Real legNPV(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
        calculate();
        return legNPV_[j];
    }
    Real inCcyLegBPS(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg# " << j << " doesn't exist!");
        calculate();
        return inCcyLegBPS_[j];
    }
    Real inCcyLegNPV(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
        calculate();
        return inCcyLegNPV_[j];
    }
    DiscountFactor startDiscounts(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
        calculate();
        return startDiscounts_[j];
    }
    DiscountFactor endDiscounts(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
        calculate();
        return endDiscounts_[j];
    }
    DiscountFactor npvDateDiscount() const {
        calculate();
        return npvDateDiscount_;
    }
    const Leg& leg(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
        return legs_[j];
    }
    const Currency& legCurrency(Size j) const {
        QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
        return currency_[j];
    }
    std::vector<Leg> legs() { return legs_; }
    std::vector<Currency> currencies() { return currency_; }
    //@}
protected:
    //! \name Constructors
    //@{
    /*! This constructor can be used by derived classes that will
        build their legs themselves.
    */
    CurrencySwap(Size legs);
    //! \name Instrument interface
    //@{
    void setupExpired() const;
    //@}
    // data members
    std::vector<Leg> legs_;
    std::vector<Real> payer_;
    std::vector<Currency> currency_;
    mutable std::vector<Real> legNPV_, inCcyLegNPV_;
    mutable std::vector<Real> legBPS_, inCcyLegBPS_;
    mutable std::vector<DiscountFactor> startDiscounts_, endDiscounts_;
    mutable DiscountFactor npvDateDiscount_;
};

//! \ingroup instruments
class CurrencySwap::arguments : public virtual PricingEngine::arguments {
public:
    std::vector<Leg> legs;
    std::vector<Real> payer;
    std::vector<Currency> currency;
    void validate() const;
};

//! \ingroup instruments
class CurrencySwap::results : public Instrument::results {
public:
    std::vector<Real> legNPV, inCcyLegNPV;
    std::vector<Real> legBPS, inCcyLegBPS;
    std::vector<DiscountFactor> startDiscounts, endDiscounts;
    DiscountFactor npvDateDiscount;
    void reset();
};

//! \ingroup instruments
class CurrencySwap::engine : public GenericEngine<CurrencySwap::arguments, CurrencySwap::results> {};

//! Vanilla cross currency interest rate swap
/*! Specialised CurrencySwap: Two currencies, fixed vs. floating,
  constant notionals, rate and spread.

      \ingroup instruments{
*/
class VanillaCrossCurrencySwap : public CurrencySwap {
public:
    VanillaCrossCurrencySwap(bool payFixed, Currency fixedCcy, Real fixedNominal, const Schedule& fixedSchedule,
                             Rate fixedRate, const DayCounter& fixedDayCount, Currency floatCcy, Real floatNominal,
                             const Schedule& floatSchedule, const boost::shared_ptr<IborIndex>& iborIndex,
                             Rate floatSpread, boost::optional<BusinessDayConvention> paymentConvention = boost::none);
};

//! Cross currency swap
/*! Specialised CurrencySwap: Two currencies, variable notionals,
    rates and spreads; flavours fix/float, fix/fix, float/float

        \ingroup instruments
*/
class CrossCurrencySwap : public CurrencySwap {
public:
    // fixed/floating
    CrossCurrencySwap(bool payFixed, Currency fixedCcy, std::vector<Real> fixedNominals, const Schedule& fixedSchedule,
                      std::vector<Rate> fixedRates, const DayCounter& fixedDayCount, Currency floatCcy,
                      std::vector<Real> floatNominals, const Schedule& floatSchedule,
                      const boost::shared_ptr<IborIndex>& iborIndex, std::vector<Rate> floatSpreads,
                      boost::optional<BusinessDayConvention> paymentConvention = boost::none);

    // fixed/fixed
    CrossCurrencySwap(bool pay1, Currency ccy1, std::vector<Real> nominals1, const Schedule& schedule1,
                      std::vector<Rate> fixedRates1, const DayCounter& fixedDayCount1, Currency ccy2,
                      std::vector<Real> nominals2, const Schedule& schedule2, std::vector<Rate> fixedRates2,
                      const DayCounter& fixedDayCount2,
                      boost::optional<BusinessDayConvention> paymentConvention = boost::none);

    // floating/floating
    CrossCurrencySwap(bool pay1, Currency ccy1, std::vector<Real> nominals1, const Schedule& schedule1,
                      const boost::shared_ptr<IborIndex>& iborIndex1, std::vector<Rate> spreads1, Currency ccy2,
                      std::vector<Real> nominals2, const Schedule& schedule2,
                      const boost::shared_ptr<IborIndex>& iborIndex2, std::vector<Rate> spreads2,
                      boost::optional<BusinessDayConvention> paymentConvention = boost::none);
};
} // namespace QuantExt

#endif
