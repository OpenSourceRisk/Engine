/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/discountingcurrencyswapengine.hpp
    \brief discounting currency swap engine
*/

#ifndef quantext_discounting_currencyswap_engine_hpp
#define quantext_discounting_currencyswap_engine_hpp

#include <ql/currency.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/handle.hpp>

#include <qle/instruments/currencyswap.hpp>

namespace QuantExt {

    //! %Discounting %CurrencySwap Engine

    /*! This class generalizes QuantLib's DiscountingSwapEngine. It takes
        leg currencies into account and converts into the provided "npv
        currency", which must be one of the leg currencies. The evaluation
        date is the reference date of either of the discounting curves (which
        must be equal).

        \ingroup engines
    */
    class DiscountingCurrencySwapEngine : public CurrencySwap::engine {
      public:
        /*! The FX spots must be given as units of npvCurrency per respective
          currency. The spots must be given w.r.t. a settlement date equal
          to the npv date. */
        DiscountingCurrencySwapEngine(
            const std::vector<Handle<YieldTermStructure> >& discountCurves,
            const std::vector<Handle<Quote> >& fxQuotes,
            const std::vector<Currency>& currencies,
            const Currency& npvCurrency,
            boost::optional<bool> includeSettlementDateFlows = boost::none,
            Date settlementDate = Date(),
            Date npvDate = Date());
        void calculate() const;
        std::vector<Handle<YieldTermStructure> > discountCurves() {
            return discountCurves_;
        }
        std::vector<Currency> currencies() {
            return currencies_;
        }
        Currency npvCurrency() {
            return npvCurrency_;
        }
      private:
        Handle<YieldTermStructure> fetchTS(Currency ccy) const;
        Handle<Quote> fetchFX(Currency ccy) const;

        std::vector<Handle<YieldTermStructure> > discountCurves_;
        std::vector<Handle<Quote> > fxQuotes_;
        std::vector<Currency> currencies_;
        Currency npvCurrency_;
        boost::optional<bool> includeSettlementDateFlows_;
        Date settlementDate_;
        Date npvDate_;
    };

}

#endif
