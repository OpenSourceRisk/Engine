/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file crossccyswap.hpp
    \brief Swap instrument with legs involving two currencies
*/

#ifndef quantext_cross_ccy_swap_hpp
#define quantext_cross_ccy_swap_hpp

#include <ql/instruments/swap.hpp>
#include <ql/currency.hpp>

using namespace QuantLib;

namespace QuantExt {

    class CrossCcySwap : public Swap {
      public:
        class arguments;
        class results;
        class engine;
        //! \name Constructors
        //@{
        //! First leg is paid and the second is received.
        CrossCcySwap(const Leg& firstLeg,
            const Currency& firstLegCcy,
            const Leg& secondLeg,
            const Currency& secondLegCcy);
        /*! Multi leg constructor. */
        CrossCcySwap(const std::vector<Leg>& legs,
             const std::vector<bool>& payer,
             const std::vector<Currency>& currencies);
        //@}
        //! \name Instrument interface
        //@{
        void setupArguments(PricingEngine::arguments* args) const;
        void fetchResults(const PricingEngine::results*) const;
        //@}
        //! \name Additional interface
        //@{
        const Currency& legCurrency(Size j) const {
            QL_REQUIRE(j < legs_.size(), "leg# " << j << " doesn't exist!");
            return currencies_[j];
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
        DiscountFactor npvDateDiscounts(Size j) const {
            QL_REQUIRE(j < legs_.size(), "leg #" << j << " doesn't exist!");
            calculate();
            return npvDateDiscounts_[j];
        }
        //@}
      protected:
        //! \name Constructors
        //@{
        /*! This constructor can be used by derived classes that will
            build their legs themselves.
        */
        CrossCcySwap(Size legs);
        //@}
        //! \name Instrument interface
        //@{
        void setupExpired() const;
        //@}

        std::vector<Currency> currencies_;

      private:
        mutable std::vector<Real> inCcyLegNPV_;
        mutable std::vector<Real> inCcyLegBPS_;
        mutable std::vector<DiscountFactor> npvDateDiscounts_;
    };

    class CrossCcySwap::arguments : public Swap::arguments {
      public:
        std::vector<Currency> currencies;
        void validate() const;
    };

    class CrossCcySwap::results : public Swap::results {
      public:
        std::vector<Real> inCcyLegNPV;
        std::vector<Real> inCcyLegBPS;
        std::vector<DiscountFactor> npvDateDiscounts;
        void reset();
    };

    class CrossCcySwap::engine : public
        GenericEngine<CrossCcySwap::arguments, CrossCcySwap::results> {};

}

#endif
