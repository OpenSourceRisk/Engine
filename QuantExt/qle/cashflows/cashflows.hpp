/*
 Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
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

/*! \file qle/cashflows/cashflows.hpp
    \brief additional cash-flow analysis functions
*/

#ifndef quantext_cashflows_hpp
#define quantext_cashflows_hpp

#include <ql/cashflow.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %cashflow-analysis functions in addition to those in QuantLib
class CashFlows {
private:
    CashFlows();
    CashFlows(const CashFlows&);

public:
    //! \name YieldTermStructure functions
    //@{
    //! NPV due to any spreads on a leg.
    /*! The spread NPV is the sum of the spread-related cash flows on the
        leg, each discounted according to the given term structure.

        - If there are no spreads on the leg, then zero is returned.
        - Only applicable to FloatingRateCoupon. Should be expanded if
          needed for other coupon types e.g. %YoYInflationCoupon.
    */
    static Real spreadNpv(const Leg& leg, const YieldTermStructure& discountCurve, bool includeSettlementDateFlows,
                          Date settlementDate = Date(), Date npvDate = Date());
    //@}

    //! Return the sum of the cashflows on \p leg after \p startDate and before or on \p endDate
    static Real sumCashflows(const Leg& leg, const Date& startDate, const Date& endDate);

    /*! Return only the coupon rates from a \p leg i.e. only Cashflow that casts to Coupon
        Maintains the order of the coupon rates
     */
    static std::vector<Rate> couponRates(const Leg& leg);

    /*! Return the coupon rates multiplied by day count fraction from a \p leg i.e. only Cashflow
        that casts to Coupon. Maintains the order of the coupon rates
     */
    static std::vector<Rate> couponDcfRates(const Leg& leg);
};

} // namespace QuantExt

#endif
