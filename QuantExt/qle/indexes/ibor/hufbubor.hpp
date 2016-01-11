/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file hufbubor.hpp
    \brief HUF-BUBOR index
*/

#ifndef quantext_hufbubor_hpp
#define quantext_hufbubor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/hungary.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! HUF-BUBOR index
    /*! HUF-BUBOR rate overseen by MFT in association with the National 
        Bank of Hungary.

        See <http://www.acihungary.hu/en/index.php?id=bubor>.

        \remark Using Hungary calendar, should be Budapest.

        \warning Check roll convention and EOM.
    */
    class HUFBubor : public IborIndex {
      public:
        HUFBubor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("HUF-BUBOR", tenor, 2, HUFCurrency(), Hungary(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
