/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file skkbribor.hpp
    \brief SKK-BRIBOR index
*/

#ifndef quantext_skkbribor_hpp
#define quantext_skkbribor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/slovakia.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! SKK-BRIBOR index
    /*! SKK-BRIBOR rate overseen by the central bank of the Slovak Republic.

        See <http://www.nbs.sk/en/home>.

        \remark Using Slovakia calendar, should be Bratislava.

        \warning Check roll convention and EOM.
    */
    class SKKBribor : public IborIndex {
      public:
        SKKBribor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("SKK-BRIBOR", tenor, 2, SKKCurrency(), Slovakia(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
