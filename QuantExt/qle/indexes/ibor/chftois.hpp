/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file chftois.hpp
    \brief Swiss Franc T/N rate on Reuters page CHFTOIS
*/

#ifndef quantext_chftois_hpp
#define quantext_chftois_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/calendars/switzerland.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! %CHF TOIS rate
    /*! %CHF T/N rate appearing on page CHFTOIS and calculated by Cosmorex AG,
        a division of Tullet Prebon.

        \remark Using Switzerland calendar, should be Zurich.
    */
    class CHFTois : public OvernightIndex {
      public:
        CHFTois (const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
        : OvernightIndex("CHF-TOIS", 1, CHFCurrency(), Switzerland(),
              Actual360(), h) {}
    };
}

#endif
