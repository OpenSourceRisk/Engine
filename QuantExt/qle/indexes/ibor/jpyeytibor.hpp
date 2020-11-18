/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file jpyeytibor.hpp
    \brief %JPY %Euroyen %TIBOR index
*/

#ifndef quantext_jpyeytibor_hpp
#define quantext_jpyeytibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/japan.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/currencies/asia.hpp>

namespace QuantExt {
using namespace QuantLib;

    //! %JPY %Euroyen %TIBOR index
    /*! Tokyo Euroyen Interbank Offered Rate.
        
        See http://www.jbatibor.or.jp/english/rate/

        \warning This is the offshore rate fixed by JBA. 

        \todo check end-of-month adjustment.
    */
    class JPYEYTIBOR: public IborIndex {
      public:
        JPYEYTIBOR(const Period& tenor,
              const Handle<YieldTermStructure>& h =
                                    Handle<YieldTermStructure>())
        : IborIndex("JPY-EYTIBOR", tenor, 2, JPYCurrency(),
                    Japan(), ModifiedFollowing,
                    false, Actual360(), h) {}
    };

}


#endif
