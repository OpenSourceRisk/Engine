/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file qle/currencies/europe.hpp
    \brief Extend QuantLib European currencies
    \ingroup currencies
*/

#ifndef quantext_currencies_europe_hpp
#define quantext_currencies_europe_hpp

#include <ql/currency.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Serbian dinar
/*! The ISO three-letter code is RSD; the numeric code is 941.
 It is divided into 100 para/napa.

 \ingroup currencies
*/
class RSDCurrency : public Currency {
public:
    RSDCurrency();
};

//! Croatian kuna
/*! The ISO three-letter code is HRK; the numeric code is 191.
 It is divided into 100 lipa.

 \ingroup currencies
*/
class HRKCurrency : public Currency {
public:
    HRKCurrency();
};

//! Bulgarian lev
/*! The ISO three-letter code is BGN; the numeric code is 975.
 It is divided into 100 stotinki.

 \ingroup currencies
*/
class BGNCurrency : public Currency {
public:
    BGNCurrency();
};

//! Georgian lari
/*! The ISO three-letter code is GEL; the numeric code is 981.
 It is divided into 100 tetri.

 \ingroup currencies
*/
class GELCurrency : public Currency {
public:
    GELCurrency();
};

} // namespace QuantExt
#endif
