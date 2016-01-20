/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file idridrfix.hpp
    \brief IDR-IDRFIX index
*/

#ifndef quantext_idridrfix_hpp
#define quantext_idridrfix_hpp

#include <ql/indexes/iborindex.hpp>
#include <qle/currencies/asia.hpp>
#include <ql/time/calendars/indonesia.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! IDR-IDRFIX index
    /*! IDR-IDRFIX rate.

        See <http://www.fimmda.org>.

        \remark Using Indonesia calendar, should be Jakarta.

        \warning Check roll convention and EOM.
    */
    class IDRIdrfix : public IborIndex {
      public:
        IDRIdrfix(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("IDR-IDRFIX", tenor, 2, IDRCurrency(), Indonesia(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
