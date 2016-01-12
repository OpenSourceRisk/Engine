/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file audbbsw.hpp
    \brief AUD-BBSW index
*/

#ifndef quantext_audbbsw_hpp
#define quantext_audbbsw_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/oceania.hpp>
#include <ql/time/calendars/australia.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! AUD-BBSW index
    /*! AUD-BBSW rate fixed by the AFMA.

        See <http://www.afma.com.au/data/bbsw.html>.

        \remark Using Australia calendar, should be Sydney.

        \warning Convention should be Modified Following Bimonthly.
        \warning Check EOM.
    */
    class AUDbbsw : public IborIndex {
      public:
        AUDbbsw(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("AUD-BBSW", tenor, 0, AUDCurrency(), Australia(),
                  ModifiedFollowing, false, Actual365Fixed(), h) {}
    };

}

#endif
