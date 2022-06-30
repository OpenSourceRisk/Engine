/*
 Copyright (C) 2022 Quaternion Risk Management Ltd

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

/*! \file ored/utilities/daycounterparser.hpp
    \brief daycounter parser singleton class
    \ingroup utilities
*/

#pragma once

#include <ql/patterns/singleton.hpp>
#include <ql/time/daycounter.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ore {
namespace data {

class DayCounterParser : public QuantLib::Singleton<DayCounterParser, std::integral_constant<bool, true>> {
public:
    DayCounterParser();
    QuantLib::DayCounter parseDayCounter(const std::string& name) const;
    void reset();

private:
    mutable boost::shared_mutex mutex_;
    std::map<std::string, QuantLib::DayCounters> dayCounters_;
};

} // namespace data
} // namespace ore
