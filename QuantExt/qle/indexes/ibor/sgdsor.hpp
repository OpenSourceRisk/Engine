/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file sgdsor.hpp
    \brief SGD-SOR index
*/

#ifndef quantext_sgdsor_hpp
#define quantext_sgdsor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/calendars/singapore.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! SGD-SOR index
    /*! SGD-SOR rate published by ABS.

        See <http://www.abs.org.sg/index.php>.

        \warning Check roll convention and EOM.
    */
    class SGDSor : public IborIndex {
      public:
        SGDSor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("SGD-SOR", tenor, 2, SGDCurrency(), Singapore(),
                  ModifiedFollowing, false, Actual365Fixed(), h) {}
    };

}

#endif
