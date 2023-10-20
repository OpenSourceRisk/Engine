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

#include <boost/core/null_deleter.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/phoenix/bind/bind_function.hpp>
#include <iomanip>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using namespace boost::filesystem;
using namespace boost::posix_time;
using namespace std;

namespace logging = boost::log;
namespace lexpr = boost::log::expressions;
namespace lsinks = boost::log::sinks;
namespace lsrc = boost::log::sources;
namespace lkeywords = boost::log::keywords;
namespace lattr = boost::log::attributes;

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", oreSeverity);
BOOST_LOG_ATTRIBUTE_KEYWORD(fileSource, "FileSource", std::string);
BOOST_LOG_ATTRIBUTE_KEYWORD(messageType, "MessageType", std::string);

namespace ore {
namespace data {

const string StderrLogger::name = "StderrLogger";
const string BufferLogger::name = "BufferLogger";
const string FileLogger::name = "FileLogger";
const string StructuredLogger::name = "StructuredLogger";
const string ProgressLogger::name = "ProgressLogger";

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

void ProgressLogger::removeSinks() { 
    auto core = logging::core::get();
    if (fileSink_)
        core->remove_sink(fileSink_);
    if (coutSink_)
        core->remove_sink(coutSink_);
}

void ProgressLogger::setFileLog(const string& filepath, const path& dir, Size rotationSize) {
    fileSink_ = logging::add_file_log(
        lkeywords::target = dir,
        lkeywords::file_name = filepath,
        lkeywords::filter = messageType == ProgressMessage::name,
        lkeywords::scan_method = lsinks::file::scan_matching,
        lkeywords::rotation_size = rotationSize,
        lkeywords::auto_flush = true
    );
}

void ProgressLogger::setCoutLog(const bool flag) {
    auto core = logging::core::get();
    if (flag && !coutSink_) {
        // Create backend and initialize it with a stream
        boost::shared_ptr<lsinks::text_ostream_backend> backend = boost::make_shared<lsinks::text_ostream_backend>();
        backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));

        // Wrap it into the frontend and register in the core
        boost::shared_ptr<cout_sink> sink(new cout_sink(backend));
        core->add_sink(sink);

        // Store the sink for removal/retrieval later
        coutSink_ = sink;
    } else if (coutSink_) {
        core->remove_sink(coutSink_);
        coutSink_ = nullptr;
    }
}

void StructuredLogger::removeSinks() {
    if (fileSink_)
        logging::core::get()->remove_sink(fileSink_);
}

void StructuredLogger::setFileLog(const string& filepath, const path& dir, Size rotationSize = 100 * 1024 * 1024) {
    fileSink_ = logging::add_file_log(
        lkeywords::target = dir,
        lkeywords::file_name = filepath,
        lkeywords::filter = messageType == StructuredMessage::name,
        lkeywords::scan_method = lsinks::file::scan_matching,
        lkeywords::rotation_size = rotationSize,
        lkeywords::auto_flush = true
    );
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

void Log::registerIndependentLogger(const boost::shared_ptr<IndependentLogger>& logger) {
    QL_REQUIRE(independentLoggers_.find(logger->name()) == independentLoggers_.end(),
               "Logger with name " << logger->name() << " already registered as independent logger");
    independentLoggers_[logger->name()] = logger;
}

boost::shared_ptr<Logger>& Log::logger(const string& name) {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(loggers_.find(name) != loggers_.end(), "No logger found with name " << name);
    return loggers_[name];
}

void Log::removeLogger(const string& name) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    map<string, boost::shared_ptr<Logger>>::iterator it = loggers_.find(name);
    map<string, boost::shared_ptr<IndependentLogger>>::iterator iit = independentLoggers_.find(name);
    if (it != loggers_.end()) {
        loggers_.erase(it);
    } else if (iit != independentLoggers_.end()) {
        iit->second->removeSinks();
        independentLoggers_.erase(iit);
    } else {
        QL_FAIL("No logger found with name " << name);
    }
}

void Log::removeAllLoggers() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    loggers_.clear();
    logging::core::get()->remove_all_sinks();
    independentLoggers_.clear();
}

string Log::source(const char* filename, int lineNo) {
    string filepath;
    if (rootPath_.empty()) {
        filepath = filename;
    } else {
        filepath = relative(path(filename), rootPath_).string();
    }
    int lineNoLen = (int)log10((double)lineNo) + 1;      // Length of the int as a string
    int len = 2 + filepath.length() + 1 + lineNoLen + 1; // " (" + file + ':' + line + ')'

    if (maxLen_ == 0) {
        return "(" + filepath + ':' + to_string(lineNo) + ')';
    } else {
        if (len <= maxLen_) {
            // pad out spaces
            return string(maxLen_ - len, ' ') + "(" + filepath + ':' + to_string(lineNo) + ')';
        } else {
            // need to trim the filename to fit into maxLen chars
            // need to remove (len - maxLen_) chars + 3 for the "..."
            return "(..." + filepath.substr(3 + len - maxLen_) + ':' + to_string(lineNo) + ')';
        }
    }
}

void Log::addExcludeFilter(const string& key, const std::function<bool(const std::string&)> func) {
    excludeFilters_[key] = func;
}

void Log::removeExcludeFilter(const string& key) { excludeFilters_.erase(key); }

bool Log::checkExcludeFilters(const std::string& msg) {
    for (const auto& f : excludeFilters_) {
        if (f.second(msg))
            return true;
    }

    return false;
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
    ls_ << "  " << source(filename, lineNo) << " : ";

    // log pid if given
    if (pid_ > 0)
        ls_ << " [" << pid_ << "] ";

    // update statistics
    if (lastLineNo_ == lineNo && lastFileName_ == filename) {
        ++sameSourceLocationSince_;
    } else {
        lastFileName_ = filename;
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

string JSONMessage::jsonify(const boost::any& obj) {
    if (obj.type() == typeid(map<string, boost::any>)) {
        string jsonStr = "{ ";
        Size i = 0;
        for (const auto& kv : boost::any_cast<map<string, boost::any>>(obj)) {
            if (i > 0)
                jsonStr += ", ";
            jsonStr += '\"' + kv.first + "\": " + jsonify(kv.second);
            i++;
        }
        jsonStr += " }";
        return jsonStr;
    } else if (obj.type() == typeid(vector<boost::any>)) {
        string arrayStr = "[ ";
        Size i = 0;
        for (const auto& v : boost::any_cast<vector<boost::any>>(obj)) {
            if (i > 0)
                arrayStr += ", ";
            arrayStr += jsonify(v);
            i++;
        }
        arrayStr += " ]";
        return arrayStr;
    } else if (obj.type() == typeid(string)) {
        string str = boost::any_cast<string>(obj);
        boost::replace_all(str, "\\", "\\\\"); // do this before the below otherwise we get \\"
        boost::replace_all(str, "\"", "\\\"");
        boost::replace_all(str, "\r", "\\r");
        boost::replace_all(str, "\n", "\\n");
        return '\"' + str + '\"';
    } else if (obj.type() == typeid(StructuredMessage::Category)) {
        return to_string(boost::any_cast<StructuredMessage::Category>(obj));
    } else if (obj.type() == typeid(StructuredMessage::Group)) {
        return to_string(boost::any_cast<StructuredMessage::Group>(obj));
    } else if (obj.type() == typeid(int)) {
        return to_string(boost::any_cast<int>(obj));
    } else if (obj.type() == typeid(bool)) {
        return to_string(boost::any_cast<bool>(obj));
    } else if (obj.type() == typeid(QuantLib::Size)) {
        return to_string(boost::any_cast<QuantLib::Size>(obj));
    } else if (obj.type() == typeid(QuantLib::Real)) {
        return to_string(boost::any_cast<QuantLib::Real>(obj));
    } else if (obj.type() == typeid(unsigned int)) {
        return to_string(boost::any_cast<unsigned int>(obj));
    } else if (obj.type() == typeid(unsigned short)) {
        return to_string(boost::any_cast<unsigned short>(obj));
    } else if (obj.type() == typeid(float)) {
        return to_string(boost::any_cast<float>(obj));
    } else {
        WLOG(StructuredLoggingErrorMessage("JSON Message Logging", "JSONMessage::jsonify() : Unrecognised value type"));
        return string();
    }
}

StructuredMessage::StructuredMessage(const Category& category, const Group& group, const string& message,
                                     const map<string, string>& subFields) {
    data_["category"] = to_string(category);
    data_["group"] = to_string(group);
    data_["message"] = message;

    if (!subFields.empty()) {
        vector<boost::any> subFieldsVector;
        bool addedSubField = false;
        for (const auto& sf : subFields) {
            if (!sf.second.empty()) {
                map<string, boost::any> subField({{"name", sf.first}, {"value", sf.second}});
                subFieldsVector.push_back(subField);
                addedSubField = true;
            }
        }
        if (addedSubField)
            data_["sub_fields"] = subFieldsVector;
    }
}

void StructuredMessage::log() const {
    lsrc::severity_logger_mt<oreSeverity> lg;
    lg.add_attribute(messageType.get_name(), lattr::constant<string>(name));
    BOOST_LOG_SEV(lg, oreSeverity::alert) << json();
}

void StructuredMessage::addSubFields(const map<string, string>& subFields) {
    if (!subFields.empty()) {

        // First check that there is at least one non-empty subfield
        bool hasNonEmptySubField = false;
        for (const auto& sf : subFields) {
            if (!sf.second.empty()) {
                hasNonEmptySubField = true;
                break;
            }
        }
        if (!hasNonEmptySubField)
            return;

        if (data_.find("sub_fields") == data_.end()) {
            data_["sub_fields"] = vector<boost::any>();
        }

        for (const auto& sf : subFields) {
            if (!sf.second.empty()) {
                map<string, boost::any> subField({{"name", sf.first}, {"value", sf.second}});
                boost::any_cast<vector<boost::any>&>(data_.at("sub_fields")).push_back(subField);
            }
        }

    }
}

void EventMessage::log() const {
    lsrc::severity_logger_mt<oreSeverity> lg;
    lg.add_attribute(messageType.get_name(), lattr::constant<string>(name));
    BOOST_LOG_SEV(lg, oreSeverity::alert) << json();
}

ProgressMessage::ProgressMessage(const string& key, const Size progressCurrent, const Size progressTotal) {
    data_["key"] = key;
    data_["progress"] = progressCurrent;
    data_["total"] = progressTotal;
    data_["@timestamp"] =
        boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());
}

void ProgressMessage::log() const {
    lsrc::severity_logger_mt<oreSeverity> lg;
    lg.add_attribute(messageType.get_name(), lattr::constant<string>(name));
    BOOST_LOG_SEV(lg, oreSeverity::alert) << json();
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
