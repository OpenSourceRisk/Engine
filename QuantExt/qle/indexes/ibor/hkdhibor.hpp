/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file hkdhibor.hpp
    \brief HKD-HIBOR index
*/

#ifndef quantext_hkd_hibor_hpp
#define quantext_hkd_hibor_hpp

#include <ql/currencies/asia.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/calendars/hongkong.hpp>
#include <ql/indexes/iborindex.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! HKD-HIBOR index
    /*! HKD-HIBOR rate overseen by The Hong Kong Association of Banks.

        See <http://www.hkab.org.hk>.

        \warning Check roll convention and EOM.
    */
    class HKDHibor : public IborIndex {
      public:
        HKDHibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
        : IborIndex("HKD-HIBOR", tenor, 0, HKDCurrency(), HongKong(),
              ModifiedFollowing, false, Actual365Fixed(), h) {}
    };

}

#endif

