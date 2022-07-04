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

namespace QuantExt {
using namespace QuantLib;

//! Tunisian dinar
/*! The ISO three-letter code is TND; the numeric code is 788.
 It is divided into 1000 millim.

 \ingroup currencies
 */
// class TNDCurrency : public Currency {
// public:
//     TNDCurrency();
// };

//! Egyptian pound
/*! The ISO three-letter code is EGP; the numeric code is 818.
 It is divided into 100 piastres.

 \ingroup currencies
*/
// class EGPCurrency : public Currency {
// public:
//     EGPCurrency();
// };

//! Mauritian rupee
/*! The ISO three-letter code is MUR; the numeric code is 480.
 It is divided into 100 cents.

 \ingroup currencies
*/
// class MURCurrency : public Currency {
// public:
//     MURCurrency();
// };

//! Ugandan shilling
/*! The ISO three-letter code is UGX; the numeric code is 800.
It is the smallest unit.

 \ingroup currencies
*/
// class UGXCurrency : public Currency {
// public:
//     UGXCurrency();
// };

//! Zambian kwacha
/*! The ISO three-letter code is ZMW; the numeric code is 967.
It is divided into 100 ngwee.

 \ingroup currencies
*/
// class ZMWCurrency : public Currency {
// public:
//     ZMWCurrency();
// };

//! Moroccan dirham
/*! The ISO three-letter code is MAD; the numeric code is 504.
 It is divided into 100 santim.

 \ingroup currencies
*/
// class MADCurrency : public Currency {
// public:
//     MADCurrency();
// };

//! Kenyan shilling
/*! The ISO three-letter code is KES; the numeric code is 404.
 It is divided into 100 cents.

 \ingroup currencies
*/
// class KESCurrency : public Currency {
// public:
//     KESCurrency();
// };

//! Ghanaian cedi
/*! The ISO three-letter code is GHS; the numeric code is 936.
 It is divided into 100 pesewas.

 \ingroup currencies
*/
// class GHSCurrency : public Currency {
// public:
//     GHSCurrency();
// };

// Angolan kwanza
/*! The ISO three-letter code is AOA; the numeric code is 973.
 It is divided into 100 cêntimo.

 \ingroup currencies
*/
// class AOACurrency : public Currency {
// public:
//     AOACurrency();
// };

// Ethiopian birr
/*! The ISO three-letter code is ETB; the numeric code is 230.
 It is divided into 100 santim.

 \ingroup currencies
*/
// class ETBCurrency : public Currency {
// public:
//     ETBCurrency();
// };

// West African CFA franc
/*! The ISO three-letter code is XOF; the numeric code is 952.
 It is divided into 100 centime.

 \ingroup currencies
*/
// class XOFCurrency : public Currency {
// public:
//     XOFCurrency();
// };

} // namespace QuantExt
#endif
