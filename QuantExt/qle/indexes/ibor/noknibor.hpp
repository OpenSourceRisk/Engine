/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file noknibor.hpp
    \brief NOK-NIBOR index
*/

#ifndef quantext_noknibor_hpp
#define quantext_noknibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/norway.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! NOK-NIBOR index
    /*! NOK-NIBOR rate published by Oslo Børs.

        See <http://www.oslobors.no/ob_eng>.

        \remark Using Norway calendar, should be Oslo.

        \warning Check roll convention and EOM.
    */
    class NOKNibor : public IborIndex {
      public:
        NOKNibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("NOK-NIBOR", tenor, 2, NOKCurrency(), Norway(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
