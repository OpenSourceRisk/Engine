/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/


/*! \file sekstibor.hpp
    \brief SEK-STIBOR index
    \ingroup indexes 
*/

#ifndef quantext_sekstibor_hpp
#define quantext_sekstibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/time/calendars/sweden.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! SEK-STIBOR index
    /*! SEK-STIBOR rate published by Swedish Bankers' Association.

        See <http://www.swedishbankers.se/web/bf.nsf/pages/startpage_eng.html>.

        \remark Using Sweden calendar, should be Stockholm.

        \warning Check roll convention and EOM.

		\ingroup indexes
    */
    class SEKStibor : public IborIndex {
      public:
        SEKStibor(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
            : IborIndex("SEK-STIBOR", tenor, 2, SEKCurrency(), Sweden(),
                  ModifiedFollowing, false, Actual360(), h) {}
    };

}

#endif
