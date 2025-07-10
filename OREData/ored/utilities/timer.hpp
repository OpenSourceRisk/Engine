/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/timer.hpp
    \brief Utility function for recording times
    \ingroup utilities
*/

#pragma once
#include <boost/optional.hpp>
#include <boost/timer/timer.hpp>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace ore {
namespace data {

// Timer utility class to record runtimes.
// A Timer has to be instantiated, and start()/stop() are used together to record a runtime for a given key.
// Timers can be nested (e.g. TotalIMAnalytic -> SimmAnalytic -> SimmCalculator) using addTimer()
class Timer {

public:
    struct Statistics {
        boost::timer::nanosecond_type totalTime = 0;
        boost::timer::nanosecond_type maxTime = std::numeric_limits<boost::timer::nanosecond_type>::min();
        boost::timer::nanosecond_type minTime = std::numeric_limits<boost::timer::nanosecond_type>::max();
        QuantLib::Size count = 0;

        boost::timer::nanosecond_type avgTime() const { return count > 0 ? totalTime / count : 0; }

        void add(const Statistics& stats) {
            totalTime += stats.totalTime;
            maxTime = std::max(maxTime, stats.maxTime);
            minTime = std::min(minTime, stats.minTime);
            count += stats.count;
        }
    };

    Timer() {}

    // Start a timer for the given key
    void start(const std::string& key);
    // Stop the timer for the given key
    boost::optional<boost::timer::cpu_timer> stop(const std::string& key, bool returnTimer = false);
    // Either save the time stats from another timer, or (in the case of repeated calls/loops) if the key already exists, add the time stats
    void addTime(const Timer& timer);
    // Store a timer under a given key
    void addTimer(const std::string& key, const Timer& timer);
    bool empty() const { return timers_.empty() && stats_.empty(); }
    // Return a flat map of time stats, where each key describes the different levels of nesting
    std::map<std::vector<std::string>, Statistics> getTimes() const;
    const std::map<std::string, Statistics>& stats() const { return stats_; }

private:
    std::map<std::string, Timer> timers_;
    std::map<std::string, boost::timer::cpu_timer> runningTimers_;
    std::map<std::string, Statistics> stats_;
};

} // namespace data
} // namespace ore
