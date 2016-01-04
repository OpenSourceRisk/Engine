/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/currencies/africa.hpp
    \brief Extend QuantLib African currencies

    ISO number from http://fx.sauder.ubc.ca/currency_table.html

    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)
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
}
#endif
