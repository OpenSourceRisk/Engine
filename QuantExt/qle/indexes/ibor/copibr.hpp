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

/*! \file copibr.hpp
    \brief COP-IBR index
    \ingroup indexes
*/

#ifndef quantext_copibr_hpp
#define quantext_copibr_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/calendars/colombia.hpp>

namespace QuantExt {
using namespace QuantLib;

//! COP-IBR index
/*! COP-IBR is used in the COP-IBR-OIS-COMPOUND floating rate as outlined in Supplement number 31 to the 2006 ISDA
    Definitions.

    COP-IBR is the overnight Colombian Peso money market reference rate <em>Indicador Bancario de Referencia</em> as
    determined by the Banco de la Republica de Colombia and published at approximately 11:00 a.m. Bogota time on
    www.banrep.gov.co/series-estadisticas/see_tas_inter_ibr.htm.
*/

class COPIbr : public OvernightIndex {
public:
    explicit COPIbr(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("COP-IBR", 0, COPCurrency(), Colombia(), Actual360(), h) {}
};

} // namespace QuantExt

#endif
