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

/*! \file qle/cashflows/trscashflow.hpp
    \brief cashflow paying the total return of an asset
    \ingroup cashflows
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounter.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! bond trs cashflow
class TRSCashFlow : public CashFlow, public Observer {
public:
    TRSCashFlow(const Date& paymentDate, const Date& fixingStartDate, const Date& fixingEndDate,
                const Real notional, const QuantLib::ext::shared_ptr<Index>& Index,
                const Real initialPrice = Null<Real>(), const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    //! \name CashFlow interface
    //@{
    Real amount() const override;
    Date date() const override;
    //@}

    //! \name Inspectors
    //@{
    const Date& fixingStartDate() const { return fixingStartDate_; }
    const Date& fixingEndDate() const { return fixingEndDate_; }
    virtual const Real notional() const { return notional_; }
    virtual const Real notional(Date date) const { return notional_; }
    const QuantLib::ext::shared_ptr<Index>& index() const { return index_; }
    const Real initialPrice() const { return initialPrice_; }
    const QuantLib::ext::shared_ptr<FxIndex>& fxIndex() const { return fxIndex_; }
    Real fxStart() const;
    Real fxEnd() const;
    Real assetStart() const;
    Real assetEnd() const;
    //@}

    //! \name Observer interface
    //@{
    void update() override { notifyObservers(); }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

protected:
    Date paymentDate_, fixingStartDate_, fixingEndDate_;
    Real notional_;
    QuantLib::ext::shared_ptr<Index> index_;
    Real initialPrice_ = QuantLib::Null<Real>();
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
};

//! helper class building a sequence of trs cashflows
/*! \ingroup cashflows
 */
class TRSLeg {
public:
    TRSLeg(const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates, const Real notional,
               const QuantLib::ext::shared_ptr<Index>& index, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr);
    TRSLeg& withInitialPrice(Real);
    operator Leg() const;

private:
    std::vector<Date> valuationDates_;
    std::vector<Date> paymentDates_;
    Real notional_;
    QuantLib::ext::shared_ptr<Index> index_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Real initialPrice_;
};

} // namespace QuantExt
