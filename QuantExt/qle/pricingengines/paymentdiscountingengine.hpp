/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/paymentdiscountingengine.hpp
    \brief Single payment discounting engine
    \ingroup engines
*/

#ifndef quantext_payment_discounting_engine_hpp
#define quantext_payment_discounting_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/payment.hpp>

namespace QuantExt {

//! Payment discounting engine

/*! This class implements a discounting engine for a single cash flow.
    The cash flow is discounted using its currency's discount curve.
    Optionally, the NPV is converted into a different NPV currency.
    The FX spot rate for that purpose converts the in-currency NPV by multiplication.

  \ingroup engines
*/
class PaymentDiscountingEngine : public Payment::engine {
public:
    //! \name Constructors
    //@{
    /*! \param discountCurve
               Discount curve for the cash flow
        \param spotFX
               The market spot rate quote for multiplicative conversion into the NPV currency, can be empty
        \param includeSettlementDateFlows, settlementDate
               If includeSettlementDateFlows is true (false), cashflows
               on the settlementDate are (not) included in the NPV.
               If not given the settlement date is set to the
               npv date.
        \param npvDate
               Discount to this date. If not given the npv date
               is set to the evaluation date
    */
    PaymentDiscountingEngine(const Handle<YieldTermStructure>& discountCurve,
                             const Handle<Quote>& spotFX = Handle<Quote>(),
                             boost::optional<bool> includeSettlementDateFlows = boost::none,
                             const Date& settlementDate = Date(), const Date& npvDate = Date());
    //@}

    //! \name PricingEngine interface
    //@{
    void calculate() const;
    //@}

    //! \name Inspectors
    //@{
    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }
    const Handle<Quote>& spotFX() const { return spotFX_; }
    //@}

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<Quote> spotFX_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_;
    Date npvDate_;
};
} // namespace QuantExt

#endif
