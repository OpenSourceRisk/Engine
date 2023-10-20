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

/*! \file qle/indexes/cacpi.hpp
    \brief CAD CPI index
*/

#ifndef quantext_cacpi_hpp
#define quantext_cacpi_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <qle/indexes/region.hpp>

namespace QuantExt {

//! Canadian CPI index
class CACPI : public QuantLib::ZeroInflationIndex {
public:
    CACPI(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts =
              QuantLib::Handle<QuantLib::ZeroInflationTermStructure>())
        : QuantLib::ZeroInflationIndex("CPI", CanadaRegion(), false, QuantLib::Monthly,
                                       QuantLib::Period(1, QuantLib::Months), QuantLib::CADCurrency(), ts) {}

    QL_DEPRECATED_DISABLE_WARNING
    QL_DEPRECATED
    CACPI(bool interpolated, const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts =
                                 QuantLib::Handle<QuantLib::ZeroInflationTermStructure>())
        : QuantLib::ZeroInflationIndex("CPI", CanadaRegion(), false, interpolated, QuantLib::Monthly,
                                       QuantLib::Period(1, QuantLib::Months), QuantLib::CADCurrency(), ts) {}
    QL_DEPRECATED_ENABLE_WARNING
};

} // namespace QuantExt

#endif
