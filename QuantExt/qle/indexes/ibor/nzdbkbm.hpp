/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file nzdbkbm.hpp
    \brief NZD-BKBM index
*/

#ifndef quantext_nzdbkbm_hpp
#define quantext_nzdbkbm_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/oceania.hpp>
#include <ql/time/calendars/newzealand.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! NZD-BKBM index
    /*! NZD-BKBM rate published by NZFMA.

        See <http://www.nzfma.org>.

        \warning Check roll convention and EOM.
    */
    class NZDBKBM : public IborIndex {
      public:
        NZDBKBM(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("NZD-BKBM", tenor, 2, NZDCurrency(), NewZealand(),
                  ModifiedFollowing, false, ActualActual(), h) {}
    };

}

#endif
