/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file portfolio/fixingdates.hpp
    \brief Logic for calculating required fixing dates on legs
*/

#pragma once

#include <ql/time/date.hpp>
#include <ql/cashflow.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/patterns/visitor.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>

#include <set>

namespace ore {
namespace data {

/*! Gives back the dates for which fixings will be required to price the 
    \p leg.
*/
std::set<QuantLib::Date> fixingDates(const QuantLib::Leg& leg, 
    bool includeSettlementDateFlows, QuantLib::Date settlementDate = QuantLib::Date(),
    std::function<boost::shared_ptr<QuantLib::CashFlow>(boost::shared_ptr<QuantLib::CashFlow>)> f = {});

/*! Class that gets relevant fixing dates from coupons
    
    Each type of FloatingRateCoupon that we wish to cover should be added here 
    and a \c visit method implemented against it
*/
class FixingDateGetter : public QuantLib::AcyclicVisitor,
    public QuantLib::Visitor<QuantLib::CashFlow>,
    public QuantLib::Visitor<QuantLib::FloatingRateCoupon>,
    public QuantLib::Visitor<QuantLib::IndexedCashFlow>,
    public QuantLib::Visitor<QuantLib::CPICashFlow>,
    public QuantLib::Visitor<QuantLib::CPICoupon>,
    public QuantLib::Visitor<QuantLib::OvernightIndexedCoupon>,
    public QuantLib::Visitor<QuantLib::AverageBMACoupon>,
    public QuantLib::Visitor<QuantExt::AverageONIndexedCoupon>,
    public QuantLib::Visitor<QuantExt::EquityCoupon>,
    public QuantLib::Visitor<QuantExt::FloatingRateFXLinkedNotionalCoupon>,
    public QuantLib::Visitor<QuantExt::FXLinkedCashFlow> {
public:
    //! Constructor
    FixingDateGetter(bool includeSettlementDateFlows, 
        const QuantLib::Date& settlementDate = QuantLib::Date());

    //! \name Visitor interface
    //@{
    void visit(QuantLib::CashFlow& c);
    void visit(QuantLib::FloatingRateCoupon& c);
    void visit(QuantLib::IndexedCashFlow& c);
    /*! Not added in QuantLib so will never be hit automatically!
        Managed by passing off from IndexedCashFlow.
    */
    void visit(QuantLib::CPICashFlow& c);
    void visit(QuantLib::CPICoupon& c);
    void visit(QuantLib::OvernightIndexedCoupon& c);
    void visit(QuantLib::AverageBMACoupon& c);
    void visit(QuantExt::AverageONIndexedCoupon& c);
    void visit(QuantExt::EquityCoupon& c);
    void visit(QuantExt::FloatingRateFXLinkedNotionalCoupon& c);
    void visit(QuantExt::FXLinkedCashFlow& c);
    //@}

    //! Return the retrieved fixing dates
    std::set<QuantLib::Date> fixingDates() const { return fixingDates_; }

private:
    //! Flag to determine if flows happening on the settlement date are relevant
    bool includeSettlementDateFlows_;
    //! If \p settlementDate provided in the ctor, takes its value. If not, set to current evaluation date
    QuantLib::Date today_;
    //! Stores the retrieved fixing dates
    std::set<QuantLib::Date> fixingDates_;
};

}
}
