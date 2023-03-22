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
#include <boost/filesystem.hpp>
#include <iomanip>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using namespace boost::filesystem;
using namespace boost::posix_time;
using namespace std;

namespace ore {
namespace data {

const string StderrLogger::name = "StderrLogger";
const string BufferLogger::name = "BufferLogger";
const string FileLogger::name = "FileLogger";

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

// The Log itself
Log::Log() : loggers_(), enabled_(false), mask_(255), ls_() {

    ls_.setf(ios::fixed, ios::floatfield);
    ls_.setf(ios::showpoint);
}

void Log::registerLogger(const boost::shared_ptr<Logger>& logger) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(loggers_.find(logger->name()) == loggers_.end(),
               "Logger with name " << logger->name() << " already registered");
    loggers_[logger->name()] = logger;
}

boost::shared_ptr<Logger>& Log::logger(const string& name) {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(loggers_.find(name) != loggers_.end(), "No logger found with name " << name);
    return loggers_[name];
}

void Log::removeLogger(const string& name) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    map<string, boost::shared_ptr<Logger>>::iterator it = loggers_.find(name);
    QL_REQUIRE(it != loggers_.end(), "No logger found with name " << name);
    loggers_.erase(it);
}

void Log::removeAllLoggers() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    loggers_.clear();
}

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
    case ORE_MEMORY:
        ls_ << "MEMORY   ";
        break;
    }

    // Timestamp
    // Use boost::posix_time microsecond clock to get better precision (when available).
    // format is "2014-Apr-04 11:10:16.179347"
    ls_ << '[' << to_simple_string(microsec_clock::local_time()) << ']';

    // Filename & line no
    // format is " (file:line)"
    string filepath;
    if (rootPath_.empty()) {
        filepath = filename;
    } else {
        filepath = relative(path(filename), rootPath_).string();
    }
    int lineNoLen = (int)log10((double)lineNo) + 1;     // Length of the int as a string
    int len = 2 + filepath.length() + 1 + lineNoLen + 1; // " (" + file + ':' + line + ')'

    if (maxLen_ == 0) {
        ls_ << " (" << filepath << ':' << lineNo << ')';
    } else {
        if (len <= maxLen_) {
            // pad out spaces
            ls_ << string(maxLen_ - len, ' ') << " (" << filepath << ':' << lineNo << ')';
        } else {
            // need to trim the filename to fit into maxLen chars
            // need to remove (len - maxLen_) chars + 3 for the "..."
            ls_ << " (..." << filepath.substr(3 + len - maxLen_) << ':' << lineNo << ')';
        }
    }

    ls_ << " : ";

    // log pid if given
    if (pid_ > 0)
        ls_ << " [" << pid_ << "] ";

    // update statistics
    if (lastLineNo_ == lineNo && lastFileName_ == filepath) {
        ++sameSourceLocationSince_;
    } else {
        lastFileName_ = filepath;
        lastLineNo_ = lineNo;
        sameSourceLocationSince_ = 0;
        writeSuppressedMessagesHint_ = true;
    }

}

void Log::log(unsigned m) {
    string msg = ls_.str();
    map<string, boost::shared_ptr<Logger>>::iterator it;
    if (sameSourceLocationSince_ <= sameSourceLocationCutoff_) {
        for (auto& l : loggers_) {
            l.second->log(m, msg);
        }
    } else if (writeSuppressedMessagesHint_) {
        std::string suffix;
        if (msg.find(StructuredMessage::name) == string::npos) {
            suffix = " ... suppressing more messages from same source code location (cutoff = " +
                     std::to_string(sameSourceLocationCutoff_) + " lines)";
        }
        for (auto& l : loggers_) {
            l.second->log(m, msg + suffix);
        }
        writeSuppressedMessagesHint_ = false;
    }
}

// --------

LoggerStream::LoggerStream(unsigned mask, const char* filename, unsigned lineNo)
    : mask_(mask), filename_(filename), lineNo_(lineNo), ss_() {
    QL_REQUIRE(mask == ORE_ALERT || mask == ORE_CRITICAL || mask == ORE_ERROR || mask == ORE_WARNING ||
                   mask == ORE_NOTICE || mask == ORE_DEBUG || mask == ORE_DATA,
               "Invalid log mask " << mask);
}

LoggerStream::~LoggerStream() {
    string text;
    while (getline(ss_, text)) {
        // we expand the MLOG macro here so we can overwrite __FILE__ and __LINE__
        if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(mask_)) {
            boost::unique_lock<boost::shared_mutex> lock(ore::data::Log::instance().mutex());
            ore::data::Log::instance().header(mask_, filename_, lineNo_);
            ore::data::Log::instance().logStream() << text;
            ore::data::Log::instance().log(mask_);
        }
    }
}

string StructuredMessage::json() const {
    string msg = "{ \"category\":\"" + ore::data::to_string(category_) + "\", \"group\":\"" + ore::data::to_string(group_) + "\"," + " \"message\":\"" +
                 jsonify(message_) + "\"";

    if (!subFields_.empty()) {
        msg += ", \"sub_fields\": [ ";
        QuantLib::Size i = 0;
        for (const auto& p : subFields_) {
            // Only include subFields that are non-empty.
            if (!p.second.empty()) {
                if (i > 0)
                    msg += ", ";
                msg += "{ \"name\": \"" + p.first + "\", \"value\": \"" + jsonify(p.second) + "\" }";
                i++;
            }
        }
        msg += " ]";
    }
    msg += " }";

    return msg;
}

string StructuredMessage::jsonify(const string& s) const {
    string str = s;
    boost::replace_all(str, "\\", "\\\\"); // do this before the below otherwise we get \\"
    boost::replace_all(str, "\"", "\\\"");
    boost::replace_all(str, "\r", "\\r");
    boost::replace_all(str, "\n", "\\n");
    return str;
}

string EventMessage::json() const {
    string msg = "{ \"exception_message\":\"" + jsonify(message_) + "\"";

    if (!data_.empty()) {
        msg += ", ";
        QuantLib::Size i = 0;
        for (const auto& p : data_) {
            string value;
            if (p.second.type() == typeid(string)) {
                value = "\"" + boost::any_cast<string>(p.second) + "\"";
            } else if (p.second.type() == typeid(boost::posix_time::ptime)) {
                auto time = boost::any_cast<boost::posix_time::ptime>(p.second);
                value = boost::posix_time::to_iso_extended_string(time);
            } else if (p.second.type() == typeid(int)) {
                value = to_string(boost::any_cast<int>(p.second));
            } else if (p.second.type() == typeid(unsigned int)) {
                value = to_string(boost::any_cast<unsigned int>(p.second));
            } else if (p.second.type() == typeid(unsigned short)) {
                value = to_string(boost::any_cast<unsigned short>(p.second));
            } else if (p.second.type() == typeid(float)) {
                value = to_string(boost::any_cast<float>(p.second));
            } else if (p.second.type() == typeid(QuantLib::Size)) {
                value = to_string(boost::any_cast<QuantLib::Size>(p.second));
            } else if (p.second.type() == typeid(QuantLib::Real)) {
                value = to_string(boost::any_cast<QuantLib::Real>(p.second));
            } else if (p.second.type() == typeid(bool)) {
                value = to_string(boost::any_cast<bool>(p.second));
            } else {
                WLOG(StructuredLoggingErrorMessage("Event Message Logging",
                                                   "Unrecognised value type for key '" + p.first + "'"));
            }

            if (i > 0)
                msg += ", ";
            msg += "\"" + p.first + "\": " + value;
            i++;
        }
    }
    msg += " }";

    return msg;
}

string EventMessage::jsonify(const string& s) const {
    string str = s;
    boost::replace_all(str, "\\", "\\\\"); // do this before the below otherwise we get \\"
    boost::replace_all(str, "\"", "\\\"");
    boost::replace_all(str, "\r", "\\r");
    boost::replace_all(str, "\n", "\\n");
    return str;
}


std::ostream& operator<<(std::ostream& out, const StructuredMessage::Category& category) {
    if (category == StructuredMessage::Category::Error)
        out << "Error";
    else if (category == StructuredMessage::Category::Warning)
        out << "Warning";
    else if (category == StructuredMessage::Category::Unknown)
        out << "UnknownType";
    else
        QL_FAIL("operator<<: Unsupported enum value for StructuredMessage::Category");

    return out;
}

std::ostream& operator<<(std::ostream& out, const StructuredMessage::Group& group) {
    if (group == StructuredMessage::Group::Analytics)
        out << "Analytics";
    else if (group == StructuredMessage::Group::Configuration)
        out << "Configuration";
    else if (group == StructuredMessage::Group::Model)
        out << "Model";
    else if (group == StructuredMessage::Group::Curve)
        out << "Curve";
    else if (group == StructuredMessage::Group::Trade)
        out << "Trade";
    else if (group == StructuredMessage::Group::Fixing)
        out << "Fixing";
    else if (group == StructuredMessage::Group::Logging)
        out << "Logging";
    else if (group == StructuredMessage::Group::ReferenceData)
        out << "Reference Data";
    else if (group == StructuredMessage::Group::Unknown)
        out << "UnknownType";
    else
        QL_FAIL("operator<<: Unsupported enum value for StructuredMessage::Group");

    return out;
}

} // namespace data
} // namespace ore
