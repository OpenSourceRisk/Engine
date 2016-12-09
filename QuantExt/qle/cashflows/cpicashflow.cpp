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

/*! \file qle/cashflows/cpicashflow.cpp
    \brief An extended cpi cashflow
*/

#include <qle/cashflows/cpicashflow.hpp>

using namespace QuantLib;

namespace QuantExt {

Real CPICashFlow::amount() const {
    // As in QuantLib::CPICashFlow
    // I0 = base (as before)
    // I1 = fixing at time (t) fixingDate
    Real I0 = baseFixing();
    Real I1;

    // what interpolation do we use? Index / flat / linear
    if (interpolation() == CPI::AsIndex ) {
        I1 = index()->fixing(fixingDate());
    } else {
        // work out what it should be
        //std::cout << fixingDate() << " and " << frequency() << std::endl;
        //std::pair<Date,Date> dd = inflationPeriod(fixingDate(), frequency());
        //std::cout << fixingDate() << " and " << dd.first << " " << dd.second << std::endl;
        // work out what it should be
        std::pair<Date,Date> dd = inflationPeriod(fixingDate(), frequency());
        Real indexStart = index()->fixing(dd.first);
        if (interpolation() == CPI::Linear) {
            Real indexEnd = index()->fixing(dd.second+Period(1,Days));
            // linear interpolation
            //std::cout << indexStart << " and " << indexEnd << std::endl;
            I1 = indexStart + (indexEnd - indexStart) * (fixingDate() - dd.first)
            / ( (dd.second+Period(1,Days)) - dd.first); // can't get to next period's value within current period
        } else {
            // no interpolation, i.e. flat = constant, so use start-of-period value
            I1 = indexStart;
        }
    }

    // Now get Iprev which is the index value at time prevFixingDate_
    Real Iprev;
    if (interpolation() == CPI::AsIndex ) {
        Iprev = index()->fixing(prevFixingDate_);
    } else {
        std::pair<Date,Date> dd = inflationPeriod(prevFixingDate_, frequency());
        Real indexStart = index()->fixing(dd.first);
        if (interpolation() == CPI::Linear) {
            Real indexEnd = index()->fixing(dd.second+Period(1,Days));
            Iprev = indexStart + (indexEnd - indexStart) * (prevFixingDate_ - dd.first)
            / ( (dd.second+Period(1,Days)) - dd.first);
        } else {
            Iprev = indexStart;
        }
    }

    // Now calculate Amount
    return notional() * ((I1 - Iprev) / I0);
}
}
