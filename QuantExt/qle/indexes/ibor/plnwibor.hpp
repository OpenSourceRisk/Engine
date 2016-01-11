/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file plnwibor.hpp
    \brief PLN-WIBOR index
*/

#ifndef quantext_plnwibor_hpp
#define quantext_plnwibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/poland.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! PLN-WIBOR index
    /*! PLN-WIBOR rate published by ACI Polska.

        See <http://acipolska.pl/english.html>.

        \remark Using Poland calendar, should be Warsaw.

        \warning Check roll convention and EOM.
    */
    class PLNWibor : public IborIndex {
      public:
        PLNWibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("PLN-WIBOR", tenor, 2, PLNCurrency(), Poland(),
                  ModifiedFollowing, false, Actual365Fixed(), h) {}
    };

}

#endif
