/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#pragma once

#include <ored/portfolio/portfolio.hpp>

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Index;
using ore::data::Portfolio;

namespace ore {
namespace analytics {

//! Pseudo Fixings Manager
/*!
  A Pseudo Fixing is a future historical fixing. When pricing on T0 but asof T and we require a fixing on t with
  T0 < t < T then the QuantLib pricing engines will look to the IndexManager for a fixing at t.

  When moving between dates and simulation paths then the Fixings can change and should be populated in a path
  consistant manager

  The FixingManager controls this updating and reset of the QuantLib::IndexManager for the required set of fixings

  When stepping between simulation dated t_(n-1) and t_(n) and update a fixing t with t_(n-1) < t < t(n) than the fixing
  from t(n) will be backfilled. There is currently no interpolation of fixings.

  \ingroup simulation
 */
class FixingManager {
public:
    FixingManager(Date today) : today_(today), fixingsEnd_(today) {}

    //! Initialise the manager with these flows and indices from the given portfolio
    void initialise(const boost::shared_ptr<Portfolio>& portfolio);

    //! Update fixings to date d
    void update(Date d);

    //! Reset fixings to t0 (today)
    void reset();

private:
    void applyFixings(Date start, Date end);

    Date today_, fixingsEnd_;
    std::map<std::string, TimeSeries<Real>> fixingCache_;
    std::vector<boost::shared_ptr<Index>> indices_;
    std::map<std::string, std::vector<Date>> fixingMap_;
};
}
}
