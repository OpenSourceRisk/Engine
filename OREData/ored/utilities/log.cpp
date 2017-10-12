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

/*! \file ored/data/log.cpp
    \brief Classes and functions for log message handling.
    \ingroup
*/

#include <boost/date_time/posix_time/posix_time.hpp>
#include <iomanip>
#include <ored/utilities/log.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/errors.hpp>

using namespace boost::posix_time;
using namespace std;

namespace ore {
namespace data {

const string StderrLogger::name = "StderrLogger";
const string BufferLogger::name = "BufferLogger";
const string FileLogger::name = "FileLogger";
const string BoostTestLogger::name = "BoostTestLogger";

// -- Buffer Logger

void BufferLogger::log(unsigned level, const string& s) {
    if (level <= minLevel_)
        buffer_.push(s);
}

bool BufferLogger::hasNext() { return !buffer_.empty(); }

string BufferLogger::next() {
    QL_REQUIRE(!buffer_.empty(), "Log Buffer is empty");
    string s = buffer_.front();
    buffer_.pop();
    return s;
}

// -- File Logger

FileLogger::FileLogger(const string& filename) : Logger(name), filename_(filename) {
    fout_.open(filename.c_str(), ios_base::out);
    QL_REQUIRE(fout_.is_open(), "Error opening file " << filename);
    fout_.setf(ios::fixed, ios::floatfield);
    fout_.setf(ios::showpoint);
}

FileLogger::~FileLogger() {
    if (fout_.is_open())
        fout_.close();
}

void FileLogger::log(unsigned, const string& msg) {
    if (fout_.is_open())
        fout_ << msg << endl;
}

void  BoostTestLogger::log(unsigned, const string& msg) {
    BOOST_TEST_MESSAGE(msg);
}

// The Log itself
Log::Log() : loggers_(), enabled_(false), mask_(255), ls_() {

    ls_.setf(ios::fixed, ios::floatfield);
    ls_.setf(ios::showpoint);
}

void Log::registerLogger(const boost::shared_ptr<Logger>& logger) {
    QL_REQUIRE(loggers_.find(logger->name()) == loggers_.end(),
               "Logger with name " << logger->name() << " already registered");
    loggers_[logger->name()] = logger;
}

boost::shared_ptr<Logger>& Log::logger(const string& name) {
    QL_REQUIRE(loggers_.find(name) != loggers_.end(), "No logger found with name " << name);
    return loggers_[name];
}

void Log::removeLogger(const string& name) {
    map<string, boost::shared_ptr<Logger>>::iterator it = loggers_.find(name);
    QL_REQUIRE(it != loggers_.end(), "No logger found with name " << name);
    loggers_.erase(it);
}

void Log::removeAllLoggers() { loggers_.clear(); }

void Log::header(unsigned m, const char* filename, int lineNo) {
    // 1. Reset stringstream
    ls_.str(string());
    ls_.clear();

    // Write the header to the stream
    // TYPE [Time Stamp] (file:line)
    switch (m) {
    case ORE_ALERT:
        ls_ << "ALERT    ";
        break;
    case ORE_CRITICAL:
        ls_ << "CRITICAL ";
        break;
    case ORE_ERROR:
        ls_ << "ERROR    ";
        break;
    case ORE_WARNING:
        ls_ << "WARNING  ";
        break;
    case ORE_NOTICE:
        ls_ << "NOTICE   ";
        break;
    case ORE_DEBUG:
        ls_ << "DEBUG    ";
        break;
    case ORE_DATA:
        ls_ << "DATA     ";
        break;
    }

    // Timestamp
    // Use boost::posix_time microsecond clock to get better precision (when available).
    // format is "2014-Apr-04 11:10:16.179347"
    ls_ << '[' << to_simple_string(microsec_clock::local_time()) << ']';

    // Filename & line no
    // format is " (file:line)"
    int lineNoLen = (int)log10((double)lineNo) + 1;     // Length of the int as a string
    int len = 2 + strlen(filename) + 1 + lineNoLen + 1; // " (" + file + ':' + line + ')'

    int maxLen = 30; // gives about 23 chars for the filename
    if (len <= maxLen) {
        ls_ << " (" << filename << ':' << lineNo << ')';
        // pad out spaces
        ls_ << string(maxLen - len, ' ');
    } else {
        // need to trim the filename to fit into maxLen chars
        // need to remove (len - maxLen) chars + 3 for the "..."
        ls_ << " (..." << string(filename).substr(3 + len - maxLen) << ':' << lineNo << ')';
    }

    ls_ << " : ";
}

void Log::log(unsigned m) {
    string msg = ls_.str();
    map<string, boost::shared_ptr<Logger>>::iterator it;
    for (it = loggers_.begin(); it != loggers_.end(); ++it)
        it->second->log(m, msg);
}

// --------

LoggerStream::LoggerStream(unsigned mask, const char* filename, unsigned lineNo)
    : mask_(mask), filename_(filename), lineNo_(lineNo), ss_() {
    QL_REQUIRE(mask == ORE_ALERT || mask == ORE_CRITICAL || mask == ORE_ERROR || mask == ORE_WARNING ||
                   mask == ORE_NOTICE || mask == ORE_DEBUG,
               "Invalid log mask " << mask);
}

LoggerStream::~LoggerStream() {
    string text;
    while (getline(ss_, text)) {
        // we expand the MLOG macro here so we can overwrite __FILE__ and __LINE__
        if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(mask_)) {
            ore::data::Log::instance().header(mask_, filename_, lineNo_);
            ore::data::Log::instance().logStream() << text;
            ore::data::Log::instance().log(mask_);
        }
    }
}
} // namespace data
} // namespace ore
