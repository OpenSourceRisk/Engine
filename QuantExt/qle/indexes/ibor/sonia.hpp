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

/*! \file sonia.hpp
    \brief %SONIA index
*/

#ifndef quantext_sonia_hpp
#define quantext_sonia_hpp

#include <qle/indexes/ibor/termrateindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Sonia term index, see https://www.bankofengland.co.uk/-/media/boe/files/markets/benchmarks/rfr/rfrwg-term-sonia-reference-rate-summary.pdf#
class SoniaTerm : public TermRateIndex {
public:
    SoniaTerm(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());
};

} // namespace QuantExt

#endif
