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

/*! \file qle/currencies/asia.hpp
    \brief Extend QuantLib Asian currencies

        ISO number from http://fx.sauder.ubc.ca/currency_table.html \n
    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)
    \ingroup currencies
*/

#ifndef quantext_currencies_asia_hpp
#define quantext_currencies_asia_hpp

#include <ql/currency.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Kazakhstani tenge
/*! The ISO three-letter code is KZT; the numeric code is 398.
 It is divided into 100 tiyin.

 \ingroup currencies
*/
class KZTCurrency : public Currency {
public:
    KZTCurrency();
};

//! Qatari riyal
/*! The ISO three-letter code is QAR; the numeric code is 634.
 It is divided into 100 diram.

 \ingroup currencies
*/
class QARCurrency : public Currency {
public:
    QARCurrency();
};

//! Bahraini dinar
/*! The ISO three-letter code is BHD; the numeric code is 048.
 It is divided into 1000 fils.

 \ingroup currencies
*/
class BHDCurrency : public Currency {
public:
    BHDCurrency();
};

//! Omani rial
/*! The ISO three-letter code is OMR; the numeric code is 512.
 It is divided into 1000 baisa.

 \ingroup currencies
 */
class OMRCurrency : public Currency {
public:
    OMRCurrency();
};

//! United Arab Emirates dirham
/*! The ISO three-letter code is AED; the numeric code is 784.
 It is divided into 100 fils.

 \ingroup currencies
 */
class AEDCurrency : public Currency {
public:
    AEDCurrency();
};

//! Philippine piso
/*! The ISO three-letter code is PHP; the numeric code is 608.
 It is divided into 100 centavo.

 \ingroup currencies
 */
class PHPCurrency : public Currency {
public:
    PHPCurrency();
};
} // namespace QuantExt
#endif
