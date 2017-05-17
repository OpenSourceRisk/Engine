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

/*! \file qle/pricingengines/crossccyswapengine.hpp
    \brief Cross currency swap engine

        \ingroup engines
*/

#ifndef quantext_cross_ccy_swap_engine_hpp
#define quantext_cross_ccy_swap_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {

//! Cross currency swap engine

/*! This class implements an engine for pricing swaps comprising legs that
    invovlve two currencies. The npv is expressed in ccy1. The given currencies
    ccy1 and ccy2 are matched to the correct swap legs. The evaluation date is the
    reference date of either discounting curve (which must be equal).

            \ingroup engines
*/
class CrossCcySwapEngine : public CrossCcySwap::engine {
public:
    //! \name Constructors
    //@{
    /*! \param ccy1
               Currency 1
        \param currency1DiscountCurve
               Discount curve for cash flows in currency 1
        \param ccy2
               Currency 2
        \param currency2DiscountCurve
               Discount curve for cash flows in currency 2
        \param spotFX
               The market spot rate quote, given as units of ccy1
               for one unit of cc2. The spot rate must be given
               w.r.t. a settlement equal to the npv date.
        \param includeSettlementDateFlows, settlementDate
               If includeSettlementDateFlows is true (false), cashflows
               on the settlementDate are (not) included in the NPV.
               If not given the settlement date is set to the
               npv date.
        \param npvDate
               Discount to this date. If not given the npv date
               is set to the evaluation date
    */
    CrossCcySwapEngine(const Currency& ccy1, const Handle<YieldTermStructure>& currency1DiscountCurve,
                       const Currency& ccy2, const Handle<YieldTermStructure>& currency2DiscountCurve,
                       const Handle<Quote>& spotFX, boost::optional<bool> includeSettlementDateFlows = boost::none,
                       const Date& settlementDate = Date(), const Date& npvDate = Date());
    //@}

    //! \name PricingEngine interface
    //@{
    void calculate() const;
    //@}

    //! \name Inspectors
    //@{
    const Handle<YieldTermStructure>& currency1DiscountCurve() const { return currency1Discountcurve_; }
    const Handle<YieldTermStructure>& currency2DiscountCurve() const { return currency2Discountcurve_; }

    const Currency& currency1() const { return ccy1_; }
    const Currency& currency2() const { return ccy2_; }

    const Handle<Quote>& spotFX() const { return spotFX_; }
    //@}

private:
    Currency ccy1_;
    Handle<YieldTermStructure> currency1Discountcurve_;
    Currency ccy2_;
    Handle<YieldTermStructure> currency2Discountcurve_;
    Handle<Quote> spotFX_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_;
    Date npvDate_;
};
} // namespace QuantExt

#endif
