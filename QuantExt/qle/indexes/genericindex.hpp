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

/*! \file qle/indexes/genericindex.hpp
    \brief generic index class for storing price histories
    \ingroup indexes
*/

#pragma once

#include <ql/index.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

//! Generic Index
/*! \ingroup indexes */
class GenericIndex : public QuantLib::Index {
public:
    explicit GenericIndex(const std::string& name) : name_(name) {}
    //! \name Index interface
    //@{
    std::string name() const { return name_; }
    QuantLib::Calendar fixingCalendar() const { return QuantLib::NullCalendar(); }
    bool isValidFixingDate(const QuantLib::Date& fixingDate) const { return true; }
    QuantLib::Real fixing(const QuantLib::Date& fixingDate, bool forecastTodaysFixing = false) const {
        QuantLib::Real tmp = timeSeries()[fixingDate];
        QL_REQUIRE(tmp != QuantLib::Null<QuantLib::Real>(), "Missing " << name() << " fixing for " << fixingDate);
        return tmp;
    }

private:
    std::string name_;
};

} // namespace QuantExt
