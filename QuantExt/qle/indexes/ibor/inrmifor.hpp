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

/*! \file inrmifor.hpp
    \brief INR-MIFOR index
    \ingroup indexes
*/

#ifndef quantext_inrmifor_hpp
#define quantext_inrmifor_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/india.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

//! INR-MIFOR index
/*! INR-MIFOR rate overseen by FIMMDA.

    See <http://www.fimmda.org>.

    \remark Using India calendar, should be Mumbai (excluding Saturday).

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class INRMifor : public IborIndex {
public:
    INRMifor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("INR-MIFOR", tenor, 2, INRCurrency(), India(), ModifiedFollowing, false, Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
