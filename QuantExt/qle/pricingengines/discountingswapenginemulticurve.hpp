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

/*! \file qle/pricingengines/discountingswapenginemulticurve.hpp
    \brief Swap engine employing assumptions to speed up calculation

        \ingroup engines
*/

#ifndef quantext_discounting_swap_engine_multi_curve_hpp
#define quantext_discounting_swap_engine_multi_curve_hpp

#include <ql/instruments/swap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Discounting Swap Engine - Multi Curve
/*! This class prices a swap with numerous simplifications in the case of
    an ibor coupon leg to speed up the calculations:
    - the index of an IborCoupon is assumed to be fixing in
      advance and to have a tenor from accrual start date to accrual end
      date.
    - start and end discounts of Swap::results not populated.

    \warning if an IborCoupon with non-natural fixing and/or accrual
             period is present, the NPV will be false

    \ingroup engines
*/
class DiscountingSwapEngineMultiCurve : public QuantLib::Swap::engine {
public:
    DiscountingSwapEngineMultiCurve(const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                    bool minimalResults = true,
                                    boost::optional<bool> includeSettlementDateFlows = boost::none,
                                    Date settlementDate = Date(), Date npvDate = Date());
    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    bool minimalResults_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_;
    Date npvDate_;

    class AmountImpl;
    boost::shared_ptr<AmountImpl> impl_;
};
} // namespace QuantExt

#endif
