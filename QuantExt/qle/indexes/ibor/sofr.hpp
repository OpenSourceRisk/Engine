/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file sofr.hpp
    \brief %SOFR index, straight copy from QuantLib 1.16
*/

#ifndef quantext_sofr_hpp
#define quantext_sofr_hpp

#include <ql/indexes/iborindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Sofr (Secured Overnight Financing Rate) index.
class Sofr : public OvernightIndex {
public:
    explicit Sofr(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());
};

} // namespace QuantExt

#endif
