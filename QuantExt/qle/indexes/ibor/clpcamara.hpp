/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file clpcamara.hpp
    \brief CLP-CAMARA index
    \ingroup indexes
*/

#ifndef quantext_clpcamara_hpp
#define quantext_clpcamara_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/calendars/chile.hpp>

namespace QuantExt {

//! CLP-CAMARA index
//! CLP-CAMARA rate

class CLPCamara : public QuantLib::OvernightIndex {
public:
    CLPCamara(
        const QuantLib::Handle<QuantLib::YieldTermStructure>& h = QuantLib::Handle<QuantLib::YieldTermStructure>())
        : QuantLib::OvernightIndex("CLP-CAMARA", 2, QuantLib::CLPCurrency(), QuantLib::Chile(), QuantLib::Actual360(),
                                   h) {}
};
} // namespace QuantExt

#endif
