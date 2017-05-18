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

#include <qle/currencies/america.hpp>

using namespace QuantLib;

namespace QuantExt {

// Mexican Unidad de Inversion
MXVCurrency::MXVCurrency() {
    static boost::shared_ptr<Data> mxvData(
        new Data("Mexican Unidad de Inversion", "MXV", 979, "MXV", "", 1, Rounding(), "1$.2f %3%"));
    data_ = mxvData;
}

// Unidad de Fomento
CLFCurrency::CLFCurrency() {
    static boost::shared_ptr<Data> clfData(
        new Data("Unidad de Fomento (funds code)", "CLF", 990, "CLF", "", 1, Rounding(), "1$.2f %3%"));
    data_ = clfData;
}
} // namespace QuantExt
