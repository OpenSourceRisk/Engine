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

/*! \file qle/currencies/metals.hpp
    \brief Extend QuantLib currencies for precious metal codes

    ISO number from http://fx.sauder.ubc.ca/currency_table.html \n
    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)
    \ingroup currencies
*/

#ifndef quantext_currencies_metals_hpp
#define quantext_currencies_metals_hpp

#include <ql/currency.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Troy ounce of Gold
/*! The ISO three-letter code is XAU; the numeric code is 959.
 \ingroup currencies
 */
class XAUCurrency : public Currency {
public:
    XAUCurrency();
};

//! Troy ounce of Silver
/*! The ISO three-letter code is XAG; the numeric code is 961.
 \ingroup currencies
 */
class XAGCurrency : public Currency {
public:
    XAGCurrency();
};

//! Troy ounce of Platinum
/*! The ISO three-letter code is XPT; the numeric code is 962.
 \ingroup currencies
 */
class XPTCurrency : public Currency {
public:
    XPTCurrency();
};

//! Troy ounce of Palladium
/*! The ISO three-letter code is XPD; the numeric code is 964.
 \ingroup currencies
 */
class XPDCurrency : public Currency {
public:
    XPDCurrency();
};

//! Check if a given \p currency is a metal.
bool isMetal(const QuantLib::Currency& currency);

} // namespace QuantExt
#endif
