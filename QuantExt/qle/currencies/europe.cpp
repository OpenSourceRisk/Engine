/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/currencies/europe.hpp>

using namespace QuantLib;

namespace QuantExt {

    //Ukrainian hryvnia
    UAHCurrency::UAHCurrency() {
        static boost::shared_ptr<Data> uahData(
                                new Data("Ukrainian hryvnia", "UAH", 980,
                                         "UAH", "", 100, Rounding(), "1$.2f %3%"));
        data_ = uahData;
    }
};


