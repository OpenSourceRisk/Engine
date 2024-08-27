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

/*! \file ored/utilities/serializationdate.hpp
    \brief support for QuantLib::Date serialization
    \ingroup utilities
*/

#pragma once

#include <ql/time/date.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

namespace boost {
namespace serialization {

//! Allow for serialization of QuantLib::Date without amending its class (non-intrusive)
/*! \ingroup utilities
 */
template <class Archive> void serialize(Archive& ar, QuantLib::Date& d, const unsigned int) {
    QuantLib::Date::serial_type big;
    if (Archive::is_saving::value) {
        // When serializing, convert to long and save
        big = d.serialNumber();
        ar& big;
    } else {
        // When deserializing, write out saved long and convert to Date
        ar& big;
        if (big > 109500 || big == 0)
            d = QuantLib::Date();
        else
            d = QuantLib::Date(big);
    }
}
} // namespace serialization
} // namespace boost
