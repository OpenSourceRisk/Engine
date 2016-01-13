/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/currencies/america.hpp
    \brief Extend QuantLib American currencies

    ISO number from http://fx.sauder.ubc.ca/currency_table.html

    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)
*/

#ifndef quantext_currencies_america_hpp
#define quantext_currencies_america_hpp

#include <ql/currency.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Mexican Unidad de Inversion
    /*! The ISO three-letter code is MXV; the numeric code is 979.
     A unit of account used in Mexico.
     
     \ingroup currencies
    */
    class MXVCurrency : public Currency {
    public:
        MXVCurrency();
    };

    //! Unidad de Fomento (funds code)
    /*! The ISO three-letter code is CLF; the numeric code is 990.
     A unit of account used in Chile.
     
     \ingroup currencies
     */
    class CLFCurrency : public Currency {
    public:
        CLFCurrency();
    };
}
#endif

