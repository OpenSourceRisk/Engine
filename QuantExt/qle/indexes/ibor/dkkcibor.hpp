/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file dkkcibor.hpp
    \brief DKK-CIBOR index
*/

#ifndef quantext_dkkcibor_hpp
#define quantext_dkkcibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/denmark.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! DKK-CIBOR index
    /*! DKK-CIBOR rate overseen by Danish Bankers Association.

        See <http://www.finansraadet.dk/Pages/forside.aspx>.

        \remark Using Denmark calendar, should be Copenhagen.
                There is another index, DKK-CIBOR2, that has a spot lag of 2D.

        \warning Check roll convention and EOM.
    */
    class DKKCibor : public IborIndex {
      public:
        DKKCibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("DKK-CIBOR", tenor, 0, DKKCurrency(), Denmark(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
