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

/*! \file hkdhonia.hpp
    \brief HKD-HONIA index
    \ingroup indexes
*/

#pragma once

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/hongkong.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! HKD-HONIA index
/*! HKD-HONIA rate overseen by TMA
    https://www.tma.org.hk/en_newsevents_n1.aspx?NewsId=290
*/

class HKDHonia : public OvernightIndex {
public:
    explicit HKDHonia(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("HKDHonia", 0, HKDCurrency(), HongKong(), Actual365Fixed(), h) {}
};

} // namespace QuantExt
