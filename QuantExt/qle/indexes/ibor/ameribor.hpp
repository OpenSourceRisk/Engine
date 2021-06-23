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

/*! \file ameribor.hpp
    \brief AMERIBOR overnight index, https://ameribor.net
    \ingroup indexes
*/

#ifndef quantext_ameribor_hpp
#define quantext_ameribor_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! AMERIBOR overnight rate

class USDAmeribor : public QuantLib::OvernightIndex {
public:
    explicit USDAmeribor(const QuantLib::Handle<YieldTermStructure>& h = QuantLib::Handle<YieldTermStructure>());
};

} // namespace QuantExt

#endif
