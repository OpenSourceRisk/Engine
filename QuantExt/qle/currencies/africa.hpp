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

/*! \file qle/currencies/africa.hpp
    \brief Extend QuantLib African currencies

        ISO number from http://fx.sauder.ubc.ca/currency_table.html \n
    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)

        \ingroup currencies
*/

#ifndef quantext_currencies_africa_hpp
#define quantext_currencies_africa_hpp

#include <ql/currency.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Tunisian dinar
/*! The ISO three-letter code is TND; the numeric code is 788.
 It is divided into 1000 millim.

 \ingroup currencies
 */
class TNDCurrency : public Currency {
public:
    TNDCurrency();
};

//! Egyptian pound
/*! The ISO three-letter code is EGP; the numeric code is 818.
 It is divided into 100 piastres.

 \ingroup currencies
*/
class EGPCurrency : public Currency {
public:
    EGPCurrency();
};

//! Nigerian naira
/*! The ISO three-letter code is NGN; the numeric code is 566.
 It is divided into 100 kobo.

 \ingroup currencies
*/
class NGNCurrency : public Currency {
public:
    NGNCurrency();
};

//! Moroccan dirham
/*! The ISO three-letter code is MAD; the numeric code is 504.
 It is divided into 100 santim.

 \ingroup currencies
*/
class MADCurrency : public Currency {
public:
    MADCurrency();
};
} // namespace QuantExt
#endif
