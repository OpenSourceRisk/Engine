/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file czkpribor.hpp
    \brief CZK-PRIBOR index
*/

#ifndef quantext_czkpribor_hpp
#define quantext_czkpribor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/czechrepublic.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! CZK-PRIBOR index
    /*! CZK-PRIBOR rate overseen by the Czech National Bank.

        See <http://www.cnb.cz/en/financial_markets/money_market/pribor/>.

        \remark Using CzechRepublic calendar, should be Prague.

        \warning Check roll convention and EOM.
    */
    class CZKPribor : public IborIndex {
      public:
        CZKPribor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("CZK-PRIBOR", tenor, 2, CZKCurrency(), CzechRepublic(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
