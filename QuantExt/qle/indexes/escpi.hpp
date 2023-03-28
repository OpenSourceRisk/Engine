/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file escpi.hpp
    \brief Spain CPI index
*/

#ifndef quantext_escpi_hpp
#define quantext_escpi_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <qle/indexes/region.hpp>

namespace QuantExt {

//! Spain CPI index
class ESCPI : public ZeroInflationIndex {
public:
    ESCPI(const Handle<ZeroInflationTermStructure>& ts = Handle<ZeroInflationTermStructure>())
        : ZeroInflationIndex("CPI", SpainRegion(), false, Monthly, Period(1, Months), // availability
                             EURCurrency(), ts) {}

    QL_DEPRECATED_DISABLE_WARNING
    QL_DEPRECATED
    ESCPI(bool interpolated, const Handle<ZeroInflationTermStructure>& ts = Handle<ZeroInflationTermStructure>())
        : ZeroInflationIndex("CPI", SpainRegion(), false, interpolated, Monthly, Period(1, Months), // availability
                             EURCurrency(), ts) {}
    QL_DEPRECATED_ENABLE_WARNING
};

} // namespace QuantExt

#endif
