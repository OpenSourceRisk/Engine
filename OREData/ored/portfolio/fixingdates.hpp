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

/*! \file ored/portfolio/fixingdates.hpp
    \brief Logic for calculating required fixing dates on legs
*/

#pragma once

#include <ql/time/date.hpp>
#include <ql/cashflow.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/patterns/visitor.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>

#include <map>
#include <set>
#include <string>

namespace ore {
namespace data {

/*! Class that gets relevant fixing dates from coupons
    
    Each type of FloatingRateCoupon that we wish to cover should be added here 
    and a \c visit method implemented against it
*/
class FixingDateGetter : public QuantLib::AcyclicVisitor,
    public QuantLib::Visitor<QuantLib::CashFlow>,
    public QuantLib::Visitor<QuantLib::FloatingRateCoupon>,
    public QuantLib::Visitor<QuantLib::IborCoupon>,
    public QuantLib::Visitor<QuantLib::CappedFlooredCoupon>,
    public QuantLib::Visitor<QuantLib::IndexedCashFlow>,
    public QuantLib::Visitor<QuantLib::CPICashFlow>,
    public QuantLib::Visitor<QuantLib::CPICoupon>,
    public QuantLib::Visitor<QuantLib::YoYInflationCoupon>,
    public QuantLib::Visitor<QuantLib::OvernightIndexedCoupon>,
    public QuantLib::Visitor<QuantLib::AverageBMACoupon>,
    public QuantLib::Visitor<QuantExt::AverageONIndexedCoupon>,
    public QuantLib::Visitor<QuantExt::EquityCoupon>,
    public QuantLib::Visitor<QuantExt::FloatingRateFXLinkedNotionalCoupon>,
    public QuantLib::Visitor<QuantExt::FXLinkedCashFlow>,
    public QuantLib::Visitor<QuantExt::SubPeriodsCoupon> {

public:
    //! Constructor
    FixingDateGetter(const QuantLib::Date& settlementDate = QuantLib::Date());

    //! \name Visitor interface
    //@{
    void visit(QuantLib::CashFlow& c);
    void visit(QuantLib::FloatingRateCoupon& c);
    void visit(QuantLib::IborCoupon& c);
    void visit(QuantLib::CappedFlooredCoupon& c);
    void visit(QuantLib::IndexedCashFlow& c);
    /*! Not added in QuantLib so will never be hit automatically!
        Managed by passing off from IndexedCashFlow.
    */
    void visit(QuantLib::CPICashFlow& c);
    void visit(QuantLib::CPICoupon& c);
    void visit(QuantLib::YoYInflationCoupon& c);
    void visit(QuantLib::OvernightIndexedCoupon& c);
    void visit(QuantLib::AverageBMACoupon& c);
    void visit(QuantExt::AverageONIndexedCoupon& c);
    void visit(QuantExt::EquityCoupon& c);
    void visit(QuantExt::FloatingRateFXLinkedNotionalCoupon& c);
    void visit(QuantExt::FXLinkedCashFlow& c);
    void visit(QuantExt::SubPeriodsCoupon& c);
    //@}

    //! Get the settlement date
    const QuantLib::Date& today() const { return today_; }

    //! Set the settlement date
    QuantLib::Date& today() { return today_; }

    //! Return the retrieved fixing dates and the corresponding index name.
    std::map<std::string, std::set<QuantLib::Date>> fixingDatesIndices() const { return indicesDates_; }

protected:
    //! If \p settlementDate provided in the ctor, takes its value. If not, set to current evaluation date
    QuantLib::Date today_;

    /*! Stores the retrieved fixing dates and the corresponding index name. Useful in cases where a coupon may 
        reference multiple indices. The key is the index name and the value is the set of fixing dates for that index.
    */
    std::map<std::string, std::set<QuantLib::Date>> indicesDates_;
};

/*! Gives back the dates for which fixings will be required to price the \p leg assuming a given \p settlementDate. If
    the \p settlementDate is not provided or is set equal to \c QuantLib::Date(), the settlement date in the
    implementation is assumed to be the \c Settings::instance().evaluationDate().

    If a cashflow payment is deemed to have already occured relative to the settlement date, then no fixing is needed.
    The determination of whether a cashflow has or has not occurred will in general rely on a call to
    \c CashFlow::hasOccurred which is important in cases where the cash flow payment date falls on the settlement date.

    Another important case is where a cash flow fixing date occurs on the settlement date. In this case, we should
    always add the fixing date to the set of fixing dates regardless of
    \c Settings::instance().enforcesTodaysHistoricFixings().
*/
std::set<QuantLib::Date> fixingDates(const QuantLib::Leg& leg,
    const QuantLib::Date& settlementDate = QuantLib::Date());

std::set<QuantLib::Date> fixingDates(const QuantLib::Leg& leg,
    const QuantLib::Date& settlementDate, FixingDateGetter& fdg);

/*! Gives back the indices and associated dates for which fixings will be required to price the \p leg assuming a 
    given \p settlementDate. If the \p settlementDate is not provided or is set equal to \c QuantLib::Date(), the 
    settlement date in the implementation is assumed to be the \c Settings::instance().evaluationDate().

    If a cashflow payment is deemed to have already occured relative to the settlement date, then no fixing is needed.
    The determination of whether a cashflow has or has not occurred will in general rely on a call to
    \c CashFlow::hasOccurred which is important in cases where the cash flow payment date falls on the settlement date.

    Another important case is where a cash flow fixing date occurs on the settlement date. In this case, we should
    always add the fixing date to the set of fixing dates regardless of
    \c Settings::instance().enforcesTodaysHistoricFixings().
*/
std::map<std::string, std::set<QuantLib::Date>> fixingDatesIndices(const QuantLib::Leg& leg,
    const QuantLib::Date& settlementDate = QuantLib::Date());

std::map<std::string, std::set<QuantLib::Date>> fixingDatesIndices(const QuantLib::Leg& leg,
    const QuantLib::Date& settlementDate, FixingDateGetter& fdg);

/*! Inflation fixings are generally available on a monthly, or coarser, frequency. When a portfolio is asked for its 
    fixings, and it contains inflation fixings, ORE will by convention put the fixing date as the 1st of the 
    applicable month. Some market data providers by convention supply the inflation fixings with the date as the last 
    date of the month. This function scans the \p fixings map, and moves any inflation fixing dates from the 1st of 
    the month to the last day of the month. The key in the \p fixings map is the index name and the value is the set 
    of dates for which we require the fixings. 
*/
void amendInflationFixingDates(std::map<std::string, std::set<QuantLib::Date>>& fixings);

/*! Add index and fixing date pairs to \p fixings that will be potentially needed to build a TodaysMarket.
    
    These additional index and fixing date pairs are found by scanning the \p mktParams and:
    - for MarketObject::IndexCurve, take the ibor index name and add the dates for each weekday between settlement 
      date minus \p iborLookback period and settlement date
    - for MarketObject::ZeroInflationCurve, take the inflation index and add the first of each month between 
      settlement date minus \p inflationLookback period and settlement date
    - for MarketObject::YoYInflationCurve, take the inflation index and add the first of each month between
      settlement date minus \p inflationLookback period and settlement date
    - for MarketObject::CommodityCurve, add \e fixings for future contracts expiring 2 months either side of the 
      settlement date. The fixing dates are added for each weekday going back to the first day of the month that 
      precedes the settlement date by 2 months. The approach here will give rise to some spot commodities being 
      given a future contract name and dates added against them - this should not be a problem as there will be 
      no fixings found for them in any case.

    The original \p fixings map may be empty.
*/
void addMarketFixingDates(std::map<std::string, std::set<QuantLib::Date>>& fixings, 
    const TodaysMarketParameters& mktParams, const QuantLib::Period& iborLookback = 5 * QuantLib::Days,
    const QuantLib::Period& inflationLookback = 1 * QuantLib::Years, 
    const std::string& configuration = Market::defaultConfiguration);

}

}
