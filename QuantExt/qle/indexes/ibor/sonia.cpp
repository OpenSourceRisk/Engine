/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/indexes/ibor/sonia.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/sonia.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {

SoniaTerm::SoniaTerm(const Period& tenor, const Handle<YieldTermStructure>& h)
    : TermRateIndex("GBP-SONIATerm", tenor, 2, GBPCurrency(), UnitedKingdom(UnitedKingdom::Exchange), ModifiedFollowing,
                    false, Actual360(), h, QuantLib::ext::make_shared<QuantLib::Sonia>(h)) {}

} // namespace QuantExt
