/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file inrmifor.hpp
    \brief INR-MIFOR index
*/

#ifndef quantext_inrmifor_hpp
#define quantext_inrmifor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/calendars/india.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! INR-MIFOR index
    /*! INR-MIFOR rate overseen by FIMMDA.

        See <http://www.fimmda.org>.

        \remark Using India calendar, should be Mumbai (excluding Saturday).

        \warning Check roll convention and EOM.
    */
    class INRMifor : public IborIndex {
      public:
        INRMifor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("INR-MIFOR", tenor, 2, INRCurrency(), India(),
                  ModifiedFollowing, false, Actual365Fixed(), h) {}
    };

}

#endif
