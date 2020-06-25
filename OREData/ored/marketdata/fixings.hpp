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

/*! \file ored/marketdata/loader.hpp
    \brief Market Datum Loader Interface
    \ingroup marketdata
*/

#pragma once

#include <ql/time/date.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/serializationdate.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/shared_ptr.hpp>

#include <vector>

namespace ore {
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
    Fixing() {}
    Fixing(const QuantLib::Date& d, const std::string& s, const QuantLib::Real f) : date(d), name(s), fixing(f) {}

private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version) {
        ar& date;
        ar& name;
        ar& fixing;
    }
};

//! Utility to write a vector of fixings in the QuantLib index manager's fixing history
void applyFixings(const std::vector<Fixing>& fixings, const data::Conventions& conventions);

//! Utility to write a vector of fixings in the QuantLib index manager's fixing history
void applyDividends(const std::vector<Fixing>& dividends);

} // namespace data
} // namespace ore
