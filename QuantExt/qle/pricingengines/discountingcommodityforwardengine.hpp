/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/discountingcommodityforwardengine.hpp
    \brief Engine to value a commodity forward contract

    \ingroup engines
*/

#ifndef quantext_discounting_commodity_forward_engine_hpp
#define quantext_discounting_commodity_forward_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/commodityforward.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

//! Discounting commodity forward engine

/*! This class implements pricing of a commodity forward by discounting the future
    nominal cash flows using the respective yield curve.

    \ingroup engines
*/
class DiscountingCommodityForwardEngine : public CommodityForward::engine {
public:
    //! \name Constructors
    //@{
    /*! \param discountCurve              The discount curve to discount the forward cashflow.
        \param includeSettlementDateFlows If true (false), cashflows on the forward maturity
                                          are (are not) included in the NPV.
        \param npvDate                    Discount to this date. If not given, is set to the evaluation date
    */
    DiscountingCommodityForwardEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                      boost::optional<bool> includeSettlementDateFlows = boost::none,
                                      const QuantLib::Date& npvDate = QuantLib::Date());
    //@}

    //! \name PricingEngine interface
    //@{
    void calculate() const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve() const { return discountCurve_; }
    //@}

private:
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
    QuantLib::Date npvDate_;
};
} // namespace QuantExt

#endif
