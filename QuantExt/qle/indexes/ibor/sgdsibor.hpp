/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file sgdsibor.hpp
    \brief SGD-SIBOR index
*/

#ifndef quantext_sgdsibor_hpp
#define quantext_sgdsibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/calendars/singapore.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! SGD-SIBOR index
    /*! SGD-SIBOR rate published by ABS.

        See <http://www.abs.org.sg/index.php>.

        \warning Check roll convention and EOM.
    */
    class SGDSibor : public IborIndex {
      public:
        SGDSibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("SGD-SIBOR", tenor, 2, SGDCurrency(), Singapore(),
                  ModifiedFollowing, false, Actual365Fixed(), h) {}
    };

}

#endif
