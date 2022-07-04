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

/*! \file ester.hpp
    \brief Ester index
    \ingroup indexes
*/

#ifndef quantext_ester_hpp
#define quantext_ester_hpp

#include <ql/indexes/iborindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Ester (Euro short-term rate) rate fixed by the ECB.
class Ester : public OvernightIndex {
public:
    explicit Ester(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());
};

} // namespace QuantExt

#endif
