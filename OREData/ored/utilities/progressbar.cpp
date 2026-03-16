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

#include <ored/utilities/progressbar.hpp>

#include <iomanip>
#include <iostream>

namespace ore {
namespace data {

void ProgressReporter::registerProgressIndicator(const QuantLib::ext::shared_ptr<ProgressIndicator>& indicator) {
    indicators_.insert(indicator);
}

void ProgressReporter::unregisterProgressIndicator(const QuantLib::ext::shared_ptr<ProgressIndicator>& indicator) {
    indicators_.erase(indicator);
}

void ProgressReporter::unregisterAllProgressIndicators() { indicators_.clear(); }

void ProgressReporter::updateProgress(const unsigned long progress, const unsigned long total, const std::string& detail) {
    for (const auto& i : indicators_)
        i->updateProgress(progress, total, detail);
}

void ProgressReporter::resetProgress() {
    for (const auto& i : indicators_)
        i->reset();
}

SimpleProgressBar::SimpleProgressBar(const std::string& message, const QuantLib::Size messageWidth,
                                     const QuantLib::Size barWidth, const QuantLib::Size numberOfScreenUpdates)
    : key_(message), messageWidth_(messageWidth), barWidth_(barWidth),
      numberOfScreenUpdates_(numberOfScreenUpdates), updateCounter_(0), finalized_(false) {
    updateProgress(0, 1, "");
    updateCounter_--;
}

void SimpleProgressBar::updateProgress(const unsigned long progress, const unsigned long total, const std::string& detail) {
    if (!ConsoleLog::instance().enabled())
        return; 
    if (finalized_)
        return;
    double ratio = static_cast<double>(progress) / static_cast<double>(total);
    if (progress >= total) {
        std::cout << "\r" << std::setw(messageWidth_) << std::left << key_;
        for (unsigned int i = 0; i < barWidth_; ++i)
            std::cout << " ";
        std::cout << "       \r";
        std::cout << std::setw(messageWidth_) << std::left << key_;
        std::cout.flush();
        finalized_ = true;
        return;
    }
    if (updateCounter_ > 0 && progress * numberOfScreenUpdates_ < updateCounter_ * total) {
        return;
    }
    std::cout << "\r" << std::setw(messageWidth_) << std::left << key_;
    if (barWidth_ > 0)
        std::cout << "[";
    unsigned int pos = static_cast<unsigned int>(static_cast<double>(barWidth_) * ratio);
    for (unsigned int i = 0; i < barWidth_; ++i) {
        if (i < pos)
            std::cout << "=";
        else if (i == pos && pos != 0)
            std::cout << ">";
        else
            std::cout << " ";
    }
    if (barWidth_ > 0)
        std::cout << "] ";
    std::cout << static_cast<int>(ratio * 100.0) << " %\r";
    std::cout.flush();
    updateCounter_++;
}

void SimpleProgressBar::reset() {
    updateCounter_ = 0;
    finalized_ = false;
}

ProgressLog::ProgressLog(const std::string& message, const unsigned int numberOfMessages, const oreSeverity logLevel)
    : key_(message), numberOfMessages_(numberOfMessages), logLevel_(logLevel), messageCounter_(0) {}

void ProgressLog::updateProgress(const unsigned long progress, const unsigned long total, const std::string& detail) {
    if (messageCounter_ > 0 && progress * numberOfMessages_ < (messageCounter_ * total)) {
        return;
    }
    MLOG(logLevel_, key_ << " (" << detail << "): " << progress << " out of " << total << " steps ("
                             << static_cast<int>(static_cast<double>(progress) / static_cast<double>(total) * 100.0)
                             << "%) completed");
    ProgressMessage(key_, progress, total, detail).log();
    messageCounter_++;
}

void ProgressLog::reset() { messageCounter_ = 0; }

MultiThreadedProgressIndicator::MultiThreadedProgressIndicator(
    const std::set<QuantLib::ext::shared_ptr<ProgressIndicator>>& indicators)
    : indicators_(indicators) {}

void MultiThreadedProgressIndicator::updateProgress(const unsigned long progress, const unsigned long total, const std::string& detail) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    threadData_[std::this_thread::get_id()] = std::make_tuple(progress, total, detail);
    unsigned long progressTmp = 0;
    unsigned long totalTmp = 0;
    std::ostringstream detailTmp;
    for (auto const& d : threadData_) {
        progressTmp += std::get<0>(d.second);
        totalTmp += std::get<1>(d.second);

        if (detailTmp.tellp() != 0)
            detailTmp << "|";
        detailTmp << std::get<2>(d.second);
    }
    for (auto& i : indicators_)
        i->updateProgress(progressTmp, totalTmp, detailTmp.str());
}

void MultiThreadedProgressIndicator::reset() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    for (auto& i : indicators_)
        i->reset();
    threadData_.clear();
}

NoProgressBar::NoProgressBar(const std::string& message, const unsigned int messageWidth) {
    std::cout << std::setw(messageWidth) << message << std::flush;
}

} // namespace data
} // namespace ore
