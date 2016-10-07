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

/*! \file ored/marketdata/loader.hpp
    \brief Market Datum Loader Interface
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/marketdatum.hpp>
#include <ql/time/date.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace openriskengine {
namespace data {

//! Fixing data structure
/*!
  \ingroup marketdata
*/
struct Fixing {
    //! Fixing date
    QuantLib::Date date;
    //! Index name
    std::string name;
    //! Fixing amount
    QuantLib::Real fixing;

    //! Constructor
    Fixing(const QuantLib::Date& d, const std::string& s, Real& f) : date(d), name(s), fixing(f) {}
};

//! Utility to write a vector of fixings in the QuantLib index manager's fixing history
void applyFixings(const std::vector<Fixing>& fixings);
}
}
