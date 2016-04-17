/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2015 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file qle/pricingengines/discountingswapenginemulticurve.hpp
    \brief Swap engine employing assumptions to speed up calculation
*/

#ifndef quantext_discounting_swap_engine_multi_curve_hpp
#define quantext_discounting_swap_engine_multi_curve_hpp

#include <ql/instruments/swap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {
    
    /*! This class prices a swap with numerous simplifications in the case of 
        an ibor coupon leg to speed up the calculations:
        - the index of an IborCoupon is assumed to be fixing in 
          advance and to have a tenor from accrual start date to accrual end 
          date.
        - start and end discounts of Swap::results not populated.
    */
    class DiscountingSwapEngineMultiCurve : public Swap::engine {
      public:
        DiscountingSwapEngineMultiCurve(
            const Handle<YieldTermStructure>& discountCurve = 
                Handle<YieldTermStructure>(),
            bool minimalResults = true,
            boost::optional<bool> includeSettlementDateFlows = boost::none,
            Date settlementDate = Date(),
            Date npvDate = Date());
        void calculate() const;
        Handle<YieldTermStructure> discountCurve() const {
            return discountCurve_;
        }
      private:
        Handle<YieldTermStructure> discountCurve_;
        bool minimalResults_;
        boost::optional<bool> includeSettlementDateFlows_;
        Date settlementDate_;
        Date npvDate_;
        
        class AmountImpl;
        boost::shared_ptr<AmountImpl> impl_;
    };

}

#endif
