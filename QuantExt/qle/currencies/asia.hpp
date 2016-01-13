/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/currencies/asia.hpp
    \brief Extend QuantLib Asian currencies

    ISO number from http://fx.sauder.ubc.ca/currency_table.html

    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)
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
}
#endif
