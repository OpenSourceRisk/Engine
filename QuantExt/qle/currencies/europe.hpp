/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file qle/currencies/europe.hpp
    \brief Extend QuantLib European currencies

    ISO number from http://fx.sauder.ubc.ca/currency_table.html

    We assume all currencies have a format of "%1$.2f %3%" (2 decimal places)
*/

#ifndef quantext_currencies_europe_hpp
#define quantext_currencies_europe_hpp

#include <ql/currency.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Ukrainian hryvnia
    /*! The ISO three-letter code is UAH; the numeric code is 980.
     It is divided into 100 kopiyok.
     
     \ingroup currencies
    */
    class UAHCurrency : public Currency {
    public:
        UAHCurrency();
    };
}
#endif


