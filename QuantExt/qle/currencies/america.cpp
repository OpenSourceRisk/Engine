/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/currencies/america.hpp>

using namespace QuantLib;

namespace QuantExt {

    //Mexican Unidad de Inversion
    MXVCurrency::MXVCurrency() {
        static boost::shared_ptr<Data> mxvData(
                                     new Data("Mexican Unidad de Inversion", "MXV", 979,
                                              "MXV", "", 1, Rounding(), "1$.2f %3%"));
        data_ = mxvData;
    }

    //Unidad de Fomento
    CLFCurrency::CLFCurrency() {
        static boost::shared_ptr<Data> clfData(
                                     new Data("Unidad de Fomento (funds code)", "CLF", 990,
                                              "CLF", "", 1, Rounding(), "1$.2f %3%"));
        data_ = clfData;
    }

}

