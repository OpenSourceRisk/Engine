/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file dkcpi.hpp
    \brief DKK CPI index
*/

#ifndef quantext_dkcpi_hpp
#define quantext_dkcpi_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <qle/indexes/region.hpp>

namespace QuantExt {

//! DK CPI index
/*! Both CPI and HICP are defined by ISDA for Danish inflation
 *   https://www.isda.org/a/EoMDE/2008-inflation-defs.pdf
 *  and FpML supports both
 *   http://www.fpml.org/spec/coding-scheme/fpml-schemes.html#s5.105
 *  However looking at thing on the internet, e.g.
 *   https://e-markets.nordea.com/research/attachment/15539
 *   https://e-markets.nordea.com/api/research/attachment/70696
 *  It appears that DK CPI is the most commonly used
 */
class DKCPI : public ZeroInflationIndex {
public:
    DKCPI(const Handle<ZeroInflationTermStructure>& ts = Handle<ZeroInflationTermStructure>())
        : ZeroInflationIndex("CPI", DenmarkRegion(), false, Monthly, Period(1, Months), // availability
                             DKKCurrency(), ts) {}
    QL_DEPRECATED_DISABLE_WARNING
    QL_DEPRECATED DKCPI(bool interpolated,
                        const Handle<ZeroInflationTermStructure>& ts = Handle<ZeroInflationTermStructure>())
        : ZeroInflationIndex("CPI", DenmarkRegion(), false, interpolated, Monthly, Period(1, Months), // availability
                             DKKCurrency(), ts) {}
    QL_DEPRECATED_ENABLE_WARNING
};

} // namespace QuantExt

#endif
