/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/serializationperiod.hpp
    \brief support for QuantLib::Period serialization
    \ingroup utilities
*/

#pragma once

#include <ql/time/period.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

namespace boost {
namespace serialization {

//! Allow for serialization of QuantLib::Period without amending its class (non-intrusive)
/*! \ingroup utilities
 */
template <class Archive> void serialize(Archive& ar, QuantLib::Period& p, const unsigned int) {
    QuantLib::Integer length;
    QuantLib::TimeUnit units;
    if (Archive::is_saving::value) {
        length = p.length();
        units = p.units();
        ar& length;
        ar& units;
    } else {
        ar& length;
        ar& units;
        p = QuantLib::Period(length, units);
    }
}
} // namespace serialization
} // namespace boost
