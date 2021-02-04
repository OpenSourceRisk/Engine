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

/*! \file boebaserate.hpp
    \brief Bank of England base rate index,
    https://www.bankrate.com/uk/mortgages/bank-of-england-base-rate/
    \ingroup indexes
*/

#ifndef quantext_boebaserateindex_hpp
#define quantext_boebaserateindex_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Bank of England Base Rate index index

class BOEBaseRateIndex : public QuantLib::OvernightIndex {
public:
    explicit BOEBaseRateIndex(const QuantLib::Handle<YieldTermStructure>& h = QuantLib::Handle<YieldTermStructure>());
};

} // namespace QuantExt

#endif
