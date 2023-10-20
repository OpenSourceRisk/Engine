/*
 Copyright (C) 2019 Piotr Siejda
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

/*! \file plnpolonia.hpp
    \brief PLN-POLONIA index
    \ingroup indexes
*/

#ifndef quantext_plnpolonia_hpp
#define quantext_plnpolonia_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/poland.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! PLN-POLONIA index
/*! PLN-POLONIA rate published by NBP (National Bank of Poland).

    See <http://www.nbp.pl/homen.aspx?f=/en/aktualnosci/2017/polonia.html>, data:
   <http://www.nbp.pl/Polonia/Stawka_POLONIA.xlsx>.

            \ingroup indexes
*/
class PLNPolonia : public OvernightIndex {
public:
    PLNPolonia(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("PLN-POLONIA", 1, PLNCurrency(), Poland(), Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
