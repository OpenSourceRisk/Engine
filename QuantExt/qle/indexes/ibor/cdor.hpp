/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file cdor.hpp
    \brief %CDOR rate
*/

#ifndef quantext_cdor_hpp
#define quantext_cdor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/canada.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/currencies/america.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! %CDOR rate
    /*! Canadian Dollar Offered Rate fixed by IDA.

        \warning This is the rate fixed in Canada by IDA.    

    */
    class Cdor : public IborIndex {
    public:
        Cdor(const Period& tenor,
            const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("CDOR", tenor, 0, CADCurrency(),
                Canada(), ModifiedFollowing, false,
                Actual365Fixed(), h) {}
    };

}


#endif
