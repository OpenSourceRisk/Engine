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

/*! \file ored/utilities/serializationdaycounter.hpp
    \brief support for QuantLib::DayCounter serialization
    \ingroup utilities
*/

#pragma once

#include <ored/utilities/parsers.hpp>

#include <ql/time/daycounter.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

namespace boost {
namespace serialization {

//! Allow for serialization of QuantLib::Period without amending its class (non-intrusive)
/*! \ingroup utilities
 */
template <class Archive> void serialize(Archive& ar, QuantLib::DayCounter& dc, const unsigned int) {
    // handle the day counter via the name
    std::string dcName;
    if (Archive::is_saving::value) {
        dcName = dc.empty() ? "" : dc.name();
        ar& dcName;
    } else {
        ar& dcName;
        if (!dcName.empty()) {
            try {
                dc = ore::data::parseDayCounter(dcName);
            } catch (const std::exception& e) {
                QL_FAIL("could not deserialize day counter, please extend parseDayCounter(): " << e.what());
            }
        }
    }
}
} // namespace serialization
} // namespace boost
