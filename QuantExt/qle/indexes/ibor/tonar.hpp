/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file tonar.hpp
    \brief Toyko Overnight Average Rate (TONAR)
*/

#ifndef quantext_tonar_hpp
#define quantext_tonar_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/calendars/japan.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! %JPY %TONAR rate
    /*! Toyko Overnight Average Rate published by BOJ.
        
        See <http://www.boj.or.jp/en/statistics/market/short/mutan/>.

        \remark There is a publication lag of 1 business day.
                Using Japan calendar, should be Tokyo.
    */
    class Tonar : public OvernightIndex {
      public:
        Tonar (const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
        : OvernightIndex("TONAR", 0, JPYCurrency(), Japan(),
              Actual365Fixed(), h) {}
    };
}

#endif
