/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file test/swaptionmarketdata.hpp
    \brief structs containing swaption market data that can be used in tests
    \ingroup
*/

#ifndef quantext_test_swaptionmarketdata_hpp
#define quantext_test_swaptionmarketdata_hpp

#include <ql/math/matrix.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/period.hpp>
#include <boost/make_shared.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
struct AtmVolatilityEUR {
    // Constructor
    AtmVolatilityEUR() : optionTenors(4), swapTenors(4), nVols(optionTenors.size(), swapTenors.size(), 0.0), 
        lnVols(nVols), slnVols(nVols) {

        // Populate option tenors
        optionTenors[0] = Period(1, Years);
        optionTenors[1] = Period(5, Years);
        optionTenors[2] = Period(7, Years);
        optionTenors[3] = Period(10, Years);

        // Populate swap tenors
        swapTenors[0] = Period(1, Years);
        swapTenors[1] = Period(5, Years);
        swapTenors[2] = Period(10, Years);
        swapTenors[3] = Period(20, Years);

        // Populate the normal volatility matrix
        nVols[0][0] = 0.003543; nVols[0][1] = 0.005270; nVols[0][2] = 0.006978; nVols[0][3] = 0.007918;
        nVols[1][0] = 0.007013; nVols[1][1] = 0.007443; nVols[1][2] = 0.007820; nVols[1][3] = 0.007363;
        nVols[2][0] = 0.007519; nVols[2][1] = 0.007807; nVols[2][2] = 0.007698; nVols[2][3] = 0.007117;
        nVols[3][0] = 0.007668; nVols[3][1] = 0.007705; nVols[3][2] = 0.007611; nVols[3][3] = 0.006848;
    }

    // Members
    vector<Period> optionTenors;
    vector<Period> swapTenors;
    Matrix nVols;
    Matrix lnVols;
    Matrix slnVols;
};

struct conventionsEUR {
    // Constructor
    conventionsEUR() : settlementDays(2), fixedTenor(Period(1, Years)), fixedCalendar(TARGET()), 
        fixedConvention(ModifiedFollowing), fixedDayCounter(Thirty360(Thirty360::BondBasis)), 
        floatIndex(boost::make_shared<Euribor6M>()) {}
    
    // Members
    Natural settlementDays;
    Period fixedTenor;
    Calendar fixedCalendar;
    BusinessDayConvention fixedConvention;
    DayCounter fixedDayCounter;
    boost::shared_ptr<IborIndex> floatIndex;
};

}

#endif
