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

/*! \file qle/indexes/decpi.hpp
    \brief German CPI inflation index
*/

#pragma once

#include <ql/currencies/europe.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <qle/indexes/region.hpp>

namespace QuantExt {

//! German CPI index
class DECPI : public QuantLib::ZeroInflationIndex {
public:
    DECPI(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts =
              QuantLib::Handle<QuantLib::ZeroInflationTermStructure>())
        : QuantLib::ZeroInflationIndex("CPI", QuantExt::GermanyRegion(), false, QuantLib::Monthly,
                                       QuantLib::Period(1, QuantLib::Months), QuantLib::EURCurrency(), ts) {}
    QL_DEPRECATED_DISABLE_WARNING
    QL_DEPRECATED DECPI(bool interpolated, const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts =
                                               QuantLib::Handle<QuantLib::ZeroInflationTermStructure>())
        : QuantLib::ZeroInflationIndex("CPI", QuantExt::GermanyRegion(), false, interpolated, QuantLib::Monthly,
                                       QuantLib::Period(1, QuantLib::Months), QuantLib::EURCurrency(), ts) {}
    QL_DEPRECATED_ENABLE_WARNING
};

} // namespace QuantExt
