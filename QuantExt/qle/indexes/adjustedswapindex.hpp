/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*!
  \file qle/indexes/adjustedswapindex.hpp
  \brief swap index with convexity adjustment
*/

#ifndef quantext_adjustedswapindex_hpp
#define quantext_adjustedswapindex_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/utilities.hpp>
#include <qle/error.hpp>
#include <ql/currency.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/indexes/swapindex.hpp>

namespace QuantExt {

  //==========================================================================
    //! Convexity-adjusted Swap Index
    /*! 
      This class implements convexity adjustment using the Hull approximation.
     
     The adjusted swap index forecasts the forward CMS rate adjusted by the convexity
     quantity described in Hull (6th ed.), p. 637. The first and second derivative
     of the value of the swap's fixed leg is calculated using the duration and
     convexity functions of the CashFlows class

      \todo quanto adjustment
      \ingroup indexes
    */

    class AdjustedSwapIndex : public SwapIndex {
    public:
        AdjustedSwapIndex (const std::string& familyName,
                       const Period& tenor,
                       Natural settlementDays,
                       Currency currency,
                       const Calendar& calendar,
                       const Period& fixedLegTenor,
                       BusinessDayConvention fixedLegConvention,
                       const DayCounter& fixedLegDayCounter,
                       const boost::shared_ptr<IborIndex>& iborIndex,
                       const boost::shared_ptr<SwaptionVolatilityMatrix>& vola 
                           = boost::shared_ptr<SwaptionVolatilityMatrix> ()) :
            SwapIndex(familyName, tenor, settlementDays,
                      currency, calendar, fixedLegTenor,
                      fixedLegConvention, fixedLegDayCounter,
                      iborIndex),
            vola_(vola) {}
    
      const boost::shared_ptr<SwaptionVolatilityMatrix> volatility() {
          return vola_;
      }

      /*!
        forecastFixing returns a convexity-adjusted forecast using the Hull 
        approximation.
        
        If volatility matrix is not passed, no convexity adjustment is 
        calculated.
       */
      Rate forecastFixing(const Date& fixingDate) const; 

    private:
        boost::shared_ptr<SwaptionVolatilityMatrix> vola_;
    };

}

#endif
