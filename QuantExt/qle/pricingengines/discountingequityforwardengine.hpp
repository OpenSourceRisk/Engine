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

/*! \file qle/pricingengines/discountingequityforwardengine.hpp
    \brief Engine to value an Equity Forward contract

    \ingroup engines
*/

#ifndef quantext_discounting_equity_forward_engine_hpp
#define quantext_discounting_equity_forward_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/equityforward.hpp>

namespace QuantExt {

//! Discounting Equity Forward Engine

/*! This class implements pricing of Equity Forwards by discounting the future
    nominal cash flows using the respective yield curves. The forward price is
    estimated using reference rate and dividend yield curves as input. The
    cashflows are discounted using a separate discounting curve input.

            \ingroup engines
*/
class DiscountingEquityForwardEngine : public EquityForward::engine {
public:
    /*! \param equityInterestRateCurve
               The IR rate curve for estimating forward price.
        \param dividendYieldCurve
               The dividend yield term structure for estimating
               forward price.
        \param equitySpot
               The market spot rate quote.
        \param discountCurve
               The discount curve
        \param includeSettlementDateFlows, settlementDate
               If includeSettlementDateFlows is true (false), cashflows
               on the settlementDate are (not) included in the NPV.
               If not given the settlement date is set to the
               npv date.
        \param npvDate
               Discount to this date. If not given the npv date
               is set to the evaluation date
    */
    DiscountingEquityForwardEngine(const Handle<YieldTermStructure>& equityInterestRateCurve,
                                   const Handle<YieldTermStructure>& dividendYieldCurve,
                                   const Handle<Quote>& equitySpot, const Handle<YieldTermStructure>& discountCurve,
                                   boost::optional<bool> includeSettlementDateFlows = boost::none,
                                   const Date& settlementDate = Date(), const Date& npvDate = Date());

    void calculate() const;

    const Handle<YieldTermStructure>& equityReferenceRateCurve() const { return equityRefRateCurve_; }
    const Handle<YieldTermStructure>& divYieldCurve() const { return divYieldCurve_; }
    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }

    const Handle<Quote>& equitySpot() const { return equitySpot_; }

private:
    Handle<YieldTermStructure> equityRefRateCurve_;
    Handle<YieldTermStructure> divYieldCurve_;
    Handle<Quote> equitySpot_;
    Handle<YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_;
    Date npvDate_;
};
} // namespace QuantExt

#endif
