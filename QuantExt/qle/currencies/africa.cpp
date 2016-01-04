/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/currencies/africa.hpp>

using namespace QuantLib;

namespace QuantExt {

    // Tunisian dinar
    TNDCurrency::TNDCurrency() {
        static boost::shared_ptr<Data> tndData(
                                    new Data("Tunisian dinar", "TND", 788,
                                             "TND", "", 1000, Rounding(), "1$.2f %3%"));
        data_ = tndData;
    }
    // Egyptian pound
    EGPCurrency::EGPCurrency() {
        static boost::shared_ptr<Data> egpData(
                                     new Data("Egyptian pound", "EGP", 818,
                                              "EGP", "", 100, Rounding(), "1$.2f %3%"));
        data_ = egpData;
    }

    // Nigerian naira
    NGNCurrency::NGNCurrency() {
        static boost::shared_ptr<Data> ngnData(
                                     new Data("Nigerian naira", "NGN", 566,
                                              "NGN", "", 100, Rounding(), "1$.2f %3%"));
        data_ = ngnData;
    }

    // Moroccan dirham
    MADCurrency() {
        static boost::shared_ptr<Data> madData(
                                     new Data("Moroccan dirham", "MAD", 504,
                                              "MAD", "", 100, Rounding(), "1$.2f %3%"));
        data_ = madData;
    }
}
