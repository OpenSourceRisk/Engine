/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file mxntiie.hpp
    \brief MXN-TIIE index
*/

#ifndef quantext_mxntiie_hpp
#define quantext_mxntiie_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/america.hpp>
#include <ql/time/calendars/mexico.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! MXN-TIIE index
    /*! MXN-TIIE rate published by Banco de México.

        See <http://www.banxico.org.mx/indexEn.html>.

        \remark Using Mexico calendar, should be Meixco City.

        \warning Check roll convention and EOM.
    */
    class MXNTiie : public IborIndex {
      public:
        MXNTiie(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("MXN-TIIE", tenor, 1, MXNCurrency(), Mexico(),
                  Following, false, Actual360(), h) {}
    };

}

#endif
