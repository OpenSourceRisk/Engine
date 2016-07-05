/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/cashflows/fxlinkedcashflow.hpp
    \brief An FX linked cashflow
*/

#ifndef quantext_fx_linked_cashflow_hpp
#define quantext_fx_linked_cashflow_hpp

#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/quote.hpp>
#include <ql/time/date.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! FX Linked cash-flow
    /*!
     * Cashflow of Domestic currency where the amount is fx linked
     * to some fixed foreign amount.
     *
     * For example: a JPY flow based off 1M USD, if the USDJPY FX rate
     * is 123.45 then the JPY amount is 123.45 M JPY.
     *
     * FXLinkedCashFlow checks the FX fixing date against the eval date
     *
     * For furure fixings (date > eval) this class calcualates the FX Fwd
     * rate (using the provided FX Spot rate and FOR and DOM yield curves)
     *
     * For todays fixing (date = eval) this class converts the foreign
     * amount using the provided FX Spot rate.
     *
     * For previous fixings (date < eval) this class checks the QuantLib
     * IndexManager to get the FX fixing at which the foreign rate should be
     * converted at. The name of the index is a parameter to the constructor.
     *
     * This is not a lazy object.
     */
    class FXLinkedCashFlow : public CashFlow {
      public:
        FXLinkedCashFlow(const Date& cashFlowDate,
                         const Date& fixingDate,
                         Real foreignAmount,
                         boost::shared_ptr<FxIndex> fxIndex)
        : cashFlowDate_(cashFlowDate), fxFixingDate_(fixingDate), foreignAmount_(foreignAmount), fxIndex_(fxIndex) {}

        //! \name CashFlow interface
        //@{
        Date date() const {
            return cashFlowDate_;
        }
        Real amount() const {
            return foreignAmount_ * fxRate();
        }
        //@}

        Date fxFixingDate() const { return fxFixingDate_; }

      private:
        Date cashFlowDate_;
        Date fxFixingDate_;
        Real foreignAmount_;
        boost::shared_ptr<FxIndex> fxIndex_;

        Real fxRate() const {return fxIndex_->fixing(fxFixingDate_); }
    };
}

#endif
