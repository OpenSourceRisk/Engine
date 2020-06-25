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

/*! \file qle/cashflows/fxlinkedcashflow.hpp
    \brief An FX linked cashflow

        \ingroup cashflows
*/

#ifndef quantext_fx_linked_cashflow_hpp
#define quantext_fx_linked_cashflow_hpp

#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/quote.hpp>
#include <ql/time/date.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Base class for FX Linked cashflows
class FXLinked {
public:
    FXLinked(const Date& fixingDate, Real foreignAmount, boost::shared_ptr<FxIndex> fxIndex);
    Date fxFixingDate() const { return fxFixingDate_; }
    Real foreignAmount() const { return foreignAmount_; }
    const boost::shared_ptr<FxIndex>& fxIndex() const { return fxIndex_; }
    Real fxRate() const;

private:
    Date fxFixingDate_;
    Real foreignAmount_;
    boost::shared_ptr<FxIndex> fxIndex_;
};

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

     \ingroup cashflows
 */
class FXLinkedCashFlow : public CashFlow, public FXLinked, public Observer {
public:
    FXLinkedCashFlow(const Date& cashFlowDate, const Date& fixingDate, Real foreignAmount,
                     boost::shared_ptr<FxIndex> fxIndex);

    //! \name CashFlow interface
    //@{
    Date date() const { return cashFlowDate_; }
    Real amount() const { return foreignAmount() * fxRate(); }
    //@}

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

    //! \name Observer interface
    //@{
    void update() { notifyObservers(); }
    //@}

private:
    Date cashFlowDate_;
};

// inline definitions

inline void FXLinkedCashFlow::accept(AcyclicVisitor& v) {
    Visitor<FXLinkedCashFlow>* v1 = dynamic_cast<Visitor<FXLinkedCashFlow>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        CashFlow::accept(v);
}
} // namespace QuantExt

#endif
