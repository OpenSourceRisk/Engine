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


/*! \file idridrfix.hpp
    \brief IDR-IDRFIX index
    \ingroup 
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
