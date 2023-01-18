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

/*! \file ored/utilities/progressbar.hpp
    \brief Classes for progress reporting
    \ingroup utilities
*/

#pragma once

#include <ored/utilities/log.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>

#include <string>
#include <thread>
#include <set>

namespace ore {
namespace data {

//! Abstract Base class for a Progress Indicator
/*! \ingroup utilities
 */
class ProgressIndicator {
public:
    ProgressIndicator() {}
    virtual ~ProgressIndicator() {}
    virtual void updateProgress(const unsigned long progress, const unsigned long total) = 0;
    virtual void reset() = 0;
};

//! Base class for a Progress Reporter
/*! \ingroup utilities
 */
class ProgressReporter {
public:
    ProgressReporter() {}

    //! register a Progress Indicator
    void registerProgressIndicator(const boost::shared_ptr<ProgressIndicator>& indicator);

    //! unregister a Progress Indicator
    void unregisterProgressIndicator(const boost::shared_ptr<ProgressIndicator>& indicator);

    //! unregister all progress indicators
    void unregisterAllProgressIndicators();

    //! update progress
    void updateProgress(const unsigned long progress, const unsigned long total);

    //! reset
    void resetProgress();

    //! return progress indicators
    const std::set<boost::shared_ptr<ProgressIndicator>>& progressIndicators() const { return indicators_; }

private:
    std::set<boost::shared_ptr<ProgressIndicator>> indicators_;
};

//! Simple Progress Bar
/*! Simple Progress Bar that writes a message followed by a status
    bar to std::cout, no other output should be written to std::cout
    during the bar from this instance is displayed

    \ingroup utilities
*/
class SimpleProgressBar : public ProgressIndicator {
public:
    SimpleProgressBar(const std::string& message, const QuantLib::Size messageWidth = 40,
                      const QuantLib::Size barWidth = 40, const QuantLib::Size numberOfScreenUpdates = 100);

    //! ProgressIndicator interface
    void updateProgress(const unsigned long progress, const unsigned long total) override;
    void reset() override;

private:
    std::string message_;
    unsigned int messageWidth_, barWidth_, numberOfScreenUpdates_, updateCounter_;
    bool finalized_;
};

//! Progress Logger that writes the progress using the LOG macro
/*! \ingroup utilities
 */
class ProgressLog : public ProgressIndicator {
public:
    ProgressLog(const std::string& message, const unsigned int numberOfMessages = 100, const int logLevel = ORE_DEBUG);

    //! ProgressIndicator interface
    void updateProgress(const unsigned long progress, const unsigned long total) override;
    void reset() override;

private:
    std::string message_;
    unsigned int numberOfMessages_;
    int logLevel_;
    unsigned int messageCounter_;
};

/*! Progress Bar just writes the given message and flushes */
class NoProgressBar : public ProgressIndicator {
public:
    NoProgressBar(const std::string& message, const unsigned int messageWidth = 40);

    /*! ProgressIndicator interface */
    void updateProgress(const unsigned long progress, const unsigned long total) override {}
    void reset() override {}
};

/*! Progress Manager that consolidates updates from multiple threads */
class MultiThreadedProgressIndicator : public ProgressIndicator {
public:
    explicit MultiThreadedProgressIndicator(const std::set<boost::shared_ptr<ProgressIndicator>>& indicators);
    void updateProgress(const unsigned long progress, const unsigned long total) override;
    void reset() override;

private:
    mutable boost::shared_mutex mutex_;
    std::set<boost::shared_ptr<ProgressIndicator>> indicators_;
    std::map<std::thread::id, std::pair<unsigned long, unsigned long>> threadData_;
};

} // namespace data
} // namespace ore
