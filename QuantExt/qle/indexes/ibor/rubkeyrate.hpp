/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file rubkeyrate.hpp
    \brief RUB-KEYRATE index
    \ingroup indexes
*/

#ifndef quantext_rubkeyrate_hpp
#define quantext_rubkeyrate_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/russia.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! RUB-KEYRATE index
/*! RUB-KEYRATE rate.
*/
class RUBKeyRate : public IborIndex {
public:
    RUBKeyRate(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("RUB-KEYRATE", tenor, (tenor == 1 * Days ? 0 : 1), RUBCurrency(),
                    Russia(), ModifiedFollowing, false, ActualActual(ActualActual::ISDA), h) {}
};
} // namespace QuantExt

#endif
