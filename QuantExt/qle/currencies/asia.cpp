/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/currencies/asia.hpp>

using namespace QuantLib;

namespace QuantExt {

    //Kazakhstani tenge
    KZTCurrency::KZTCurrency() {
        static boost::shared_ptr<Data> kztData(
                                     new Data("Kazakhstani tenge", "KZT", 398,
                                              "KZT", "", 100, Rounding(), "1$.2f %3%"));
        data_ = kztData;
    }

    //Qatari riyal
    QARCurrency::QARCurrency() {
        static boost::shared_ptr<Data> qarData(
                                     new Data("Qatari riyal", "QAR", 634,
                                              "QAR", "", 100, Rounding(), "1$.2f %3%"));
        data_ = qarData;
    }

    //Bahraini dinar
    BHDCurrency::BHDCurrency() {
        static boost::shared_ptr<Data> bhdData(
                                     new Data("Bahraini dinar", "BHD", 48,
                                              "BHD", "", 1000, Rounding(), "1$.2f %3%"));
        data_ = bhdData;
    }

    //Omani rial
    OMRCurrency::OMRCurrency() {
        static boost::shared_ptr<Data> omrData(
                                     new Data("Omani rial", "OMR", 512,
                                              "OMR", "", 1000, Rounding(), "1$.2f %3%"));
        data_ = omrData;
    }

    //United Arab Emirates dirham
    AEDCurrency::AEDCurrency() {
        static boost::shared_ptr<Data> aedData(
                                     new Data("United Arab Emirates dirham", "AED", 784,
                                              "AED", "", 100, Rounding(), "1$.2f %3%"));
        data_ = aedData;
    }

    //Philippine peso
    PHPCurrency::PHPCurrency() {
        static boost::shared_ptr<Data> phpData(
                                     new Data("Philippine peso", "PHP", 608,
                                              "PHP", "", 100, Rounding(), "1$.2f %3%"));
        data_ = phpData;
    }
}
