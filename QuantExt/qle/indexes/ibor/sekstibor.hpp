/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file sekstibor.hpp
    \brief SEK-STIBOR index
*/

#ifndef quantext_sekstibor_hpp
#define quantext_sekstibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/sweden.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! SEK-STIBOR index
    /*! SEK-STIBOR rate published by Swedish Bankers' Association.

        See <http://www.swedishbankers.se/web/bf.nsf/pages/startpage_eng.html>.

        \remark Using Sweden calendar, should be Stockholm.

        \warning Check roll convention and EOM.
    */
    class SEKStibor : public IborIndex {
      public:
        SEKStibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("SEK-STIBOR", tenor, 2, SEKCurrency(), Sweden(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
