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

/*! \file ored/utilities/log.hpp
    \brief Classes and functions for log message handling.
    \ingroup utilities
*/

#pragma once

// accumulated 'filter' for 'external' DEBUG_MASK
#define ORE_ALERT 1    // 00000001    1 = 2^1-1
#define ORE_CRITICAL 2 // 00000010    3 = 2^2-1
#define ORE_ERROR 4    // 00000100    7
#define ORE_WARNING 8  // 00001000   15
#define ORE_NOTICE 16  // 00010000   31
#define ORE_DEBUG 32   // 00100000   63 = 2^6-1
#define ORE_DATA 64    // 01000000  127
#define ORE_MEMORY 128 // 10000000  255

#include <fstream>
#include <iostream>
#include <string>
#include <time.h>

#include <boost/algorithm/string.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/filesystem.hpp>
#include <ql/shared_ptr.hpp>
#include <map>
#include <ql/qldefines.hpp>
#include <queue>

#ifndef BOOST_MSVC
#include <unistd.h>
#endif

#include <iomanip>
#include <ored/utilities/osutils.hpp>
#include <ql/patterns/singleton.hpp>
#include <sstream>

#include <boost/any.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

enum oreSeverity {
    alert = ORE_ALERT,
    critical = ORE_CRITICAL,
    error = ORE_ERROR,
    warning = ORE_WARNING,
    notice = ORE_NOTICE,
    debug = ORE_DEBUG,
    data = ORE_DATA,
    memory = ORE_MEMORY
};


//! Outputs stringized representation of the severity level to the stream
template <typename CharT, typename TraitsT>
inline std::basic_ostream<CharT, TraitsT>& operator<<(std::basic_ostream<CharT, TraitsT>& strm, oreSeverity lvl) {
    switch (lvl) {
    case oreSeverity::alert:
        strm << "ALERT";
        break;
    case oreSeverity::critical:
        strm << "CRITICAL";
        break;
    case oreSeverity::error:
        strm << "ERROR";
        break;
    case oreSeverity::warning:
        strm << "WARNING";
        break;
    case oreSeverity::notice:
        strm << "NOTICE";
        break;
    case oreSeverity::debug:
        strm << "DEBUG";
        break;
    case oreSeverity::data:
        strm << "DATA";
        break;
    case oreSeverity::memory:
        strm << "MEMORY";
        break;
    default:
        strm << "UNKNOWN";
    }

    return strm;
}

namespace ore {
namespace data {

typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink;
typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> text_sink;

//! The Base Custom Log Handler class
/*!
  This base log handler class can be used to define your own custom handler and then registered with the Log class.
  Once registered it will receive all log messages as soon as they occur via it's log() method
  \ingroup utilities
  \see Log
 */
class Logger {
public:
    //! Destructor
    virtual ~Logger() {}

    //! The Log call back function
    /*!
      This function will be called every time a log message is produced.
      \param level the log level
      \param s the log message
     */
    virtual void log(unsigned level, const std::string& s) = 0;

    //! Returns the Logger name
    const std::string& name() { return name_; }

protected:
    //! Constructor
    /*!
      Implementations must provide a logger name
      \param name the logger name
     */
    Logger(const std::string& name) : name_(name) {}

private:
    std::string name_;
};

//! Stderr Logger
/*!
  This logger writes each log message out to stderr (std::cerr)
  \ingroup utilities
  \see Log
 */
class StderrLogger : public Logger {
public:
    //! the name "StderrLogger"
    static const std::string name;
    //! Constructor
    /*!
      This logger writes all logs to stderr.
      If alertOnly is set to true, it will only write alerts.
     */
    StderrLogger(bool alertOnly = false) : Logger(name), alertOnly_(alertOnly) {}
    //! Destructor
    virtual ~StderrLogger() {}
    //! The log callback that writes to stderr
    virtual void log(unsigned l, const std::string& s) override {
        if (!alertOnly_ || l == ORE_ALERT)
            std::cerr << s << std::endl;
    }

private:
    bool alertOnly_;
};

//! FileLogger
/*!
  This logger writes each log message out to the given file.
  The file is flushed, but not closed, after each log message.
  \ingroup utilities
  \see Log
 */
class FileLogger : public Logger {
public:
    //! the name "FileLogger"
    static const std::string name;
    //! Constructor
    /*!
      Construct a file logger using the given filename, this filename is passed to std::fostream::open()
      and this constructor will throw an exception if the file is not opened (e.g. if the filename is invalid)
      \param filename the log filename
     */
    FileLogger(const std::string& filename);
    //! Destructor
    virtual ~FileLogger();
    //! The log callback
    virtual void log(unsigned, const std::string&) override;

private:
    std::string filename_;
    std::fstream fout_;
};

//! BufferLogger
/*!
  This logger stores each log message in an internal buffer, it can then be probed for log messages at a later point.
  Log messages are always returned in a FIFO order.

  Typical usage to display log messages would be
  <pre>
      while (bLogger.hasNext()) {
          MsgBox("Log Message", bLogger.next());
      }
  </pre>
  \ingroup utilities
  \see Log
 */
class BufferLogger : public Logger {
public:
    //! the name "BufferLogger"
    static const std::string name;
    //! Constructor
    BufferLogger(unsigned minLevel = ORE_DATA) : Logger(name), minLevel_(minLevel) {}
    //! Destructor
    virtual ~BufferLogger() {}
    //! The log callback
    virtual void log(unsigned, const std::string&) override;

    //! Checks if Logger has new messages
    /*!
      \return True if this BufferLogger has any new log messages
     */
    bool hasNext();
    //! Retrieve new messages
    /*!
      Retrieve the next new message from the buffer, this will throw if the buffer is empty.
      Messages are returned in a FIFO order. Messages are deleted from the buffer once returned.
      \return The next message
     */
    std::string next();

private:
    std::queue<std::string> buffer_;
    unsigned minLevel_;
};

//! Base Log handler class that utilises Boost logging to create log sinks
/*!
  This type of logger should only be received via Log::registerIndependentLoggers().
  It is responsible for creating and maintaining Boost log sink/s and nothing more.
  \ingroup utilities
  \see Log
 */
class IndependentLogger {
public:
    //! Destructor
    virtual ~IndependentLogger() {}
    virtual void removeSinks() = 0;
    std::vector<std::string>& messages() { return messages_; }
    void clear();

    //! Returns the Logger name
    const std::string& name() const { return name_; }

protected:
    //! Constructor
    /*!
      Implementations must provide a logger name
      \param name the logger name
     */
    IndependentLogger(const std::string& name) : name_(name) {}
    std::vector<std::string> messages_;

private:
    std::string name_;
};

//! ProgressLogger
//! //! This logger stores each log message in a separate location indicating progress of analytic runs.
class ProgressLogger : public IndependentLogger {

public:
    //! the name "ProgressLogger"
    static const std::string name;
    //! Constructors
    ProgressLogger();
    ProgressLogger(const bool coutLog) : ProgressLogger() { setCoutLog(coutLog); }
    const QuantLib::ext::shared_ptr<file_sink>& fileSink() { return fileSink_; }
    const QuantLib::ext::shared_ptr<text_sink>& coutSink() { return coutSink_; }
    const QuantLib::ext::shared_ptr<text_sink>& cacheSink() { return cacheSink_; }
    //! Destructor
    virtual void removeSinks() override;
    void setCoutLog(bool flag);
    void setFileLog(const std::string& filepath, const boost::filesystem::path& dir, QuantLib::Size rotationSize = 0);

private:
    QuantLib::ext::shared_ptr<file_sink> fileSink_;
    QuantLib::ext::shared_ptr<text_sink> coutSink_;
    QuantLib::ext::shared_ptr<text_sink> cacheSink_;
};

//! StructuredLogger
//! //! This logger stores each structured error/warning message in a separate location
class StructuredLogger : public IndependentLogger {

public:
    //! the name "StructuredLogger"
    static const std::string name;
    //! Constructors
    StructuredLogger();
    const QuantLib::ext::shared_ptr<file_sink>& fileSink() { return fileSink_; }
    const QuantLib::ext::shared_ptr<text_sink>& cacheSink() { return cacheSink_; }
    //! Destructor
    virtual void removeSinks() override;
    void setFileLog(const std::string& filepath, const boost::filesystem::path& dir, QuantLib::Size rotationSize = 0);

private:
    QuantLib::ext::shared_ptr<file_sink> fileSink_;
    QuantLib::ext::shared_ptr<text_sink> cacheSink_;
};

//! EventLogger
/*!
This logger listens for EventMessage logs from within ORE to support event logging for ORE components
*/
class EventLogger : public ore::data::IndependentLogger {
public:
    //! the name "EventLogger"
    static const std::string name;

    //! Constructor
    //! This logger will only write event logs. All other logs will be ignored.
    EventLogger() : IndependentLogger(name) {}

    void setFormatter(const std::function<void(const boost::log::record_view&, boost::log::formatting_ostream&)>&);
    const QuantLib::ext::shared_ptr<file_sink>& fileSink() { return fileSink_; }
    //! Destructor
    virtual void removeSinks() override;
    void setFileLog(const std::string& filepath);

private:
    QuantLib::ext::shared_ptr<file_sink> fileSink_;
};

//! Global static Log class
/*!
  The Global Log class gets registered with individual loggers and receives application log messages.
  Once a message is received, it is immediately dispatched to each of the registered loggers, the order in which
  the loggers are called is not guaranteed.

  Logging is done by the calling thread and the LOG call blocks until all the loggers have returned.

  At start up, the Log class has no loggers and so will ignore any LOG() messages until it is configured.

  To configure the Log class to log to a file "/tmp/my_log.txt"
  <pre>
      Log::instance().removeAllLoggers();
      Log::instance().registerLogger(QuantLib::ext::shared_ptr<Logger>(new FileLogger("/tmp/my_log.txt")));
  </pre>

  To change the Log class to only use a BufferLogger the user must call
  <pre>
      Log::instance().removeAllLoggers();
      Log::instance().registerLogger(QuantLib::ext::shared_ptr<Logger>(new BufferLogger));
  </pre>
  and then to retrieve log messages from the buffer and print them to stdout the user must call:
  <pre>
      std::cout << "Begin Log Messages:" << std::endl;

      QuantLib::ext::shared_ptr<BufferLogger> bl = QuantLib::ext::dynamic_pointer_cast<BufferLogger>
        (Log::instance().logger(BufferLogger::name));

      while (bl.hasNext())
          std::cout << bl.next() << std::endl;
      std::cout << "End Log Messages." << std::endl;
  </pre>
  \ingroup utilities
 */
class Log : public QuantLib::Singleton<Log, std::integral_constant<bool, true>> {

    friend class QuantLib::Singleton<Log, std::integral_constant<bool, true>>;

public:
    //! Add a new Logger.
    /*!
      Adds a new logger to the Log class, the logger will be stored by it's Logger::name().
      This method will throw if a logger with the same name is already registered.
      \param logger the logger to add
     */
    void registerLogger(const QuantLib::ext::shared_ptr<Logger>& logger);
    void registerIndependentLogger(const QuantLib::ext::shared_ptr<IndependentLogger>& logger);
    void clearAllIndependentLoggers();

    //! Check if logger exists
    const bool hasLogger(const std::string& name) const;
    const bool hasIndependentLogger(const std::string& name) const;

    //! Retrieve a Logger.
    /*!
      Retrieve a Logger by it's name, for example to retrieve the StderrLogger (assuming it is registered)
      <pre>
      QuantLib::ext::shared_ptr<Logger> slogger = Log::instance().logger(StderrLogger::name);
      </pre>
      */
    QuantLib::ext::shared_ptr<Logger>& logger(const std::string& name);
    QuantLib::ext::shared_ptr<IndependentLogger>& independentLogger(const std::string& name);

    //! Remove a Logger
    /*!
      Remove a logger by name
      \param name the logger name
     */
    void removeLogger(const std::string& name);
    void removeIndependentLogger(const std::string& name);
    
    //! Remove all loggers
    /*!
      Removes all loggers. If called, all subsequent log messages will be ignored.
     */
    void removeAllLoggers();

    void addExcludeFilter(const std::string&, const std::function<bool(const std::string&)>);

    void removeExcludeFilter(const std::string&);

    bool checkExcludeFilters(const std::string&);

    //! macro utility function - do not use directly, not thread safe
    void header(unsigned m, const char* filename, int lineNo);
    //! macro utility function - do not use directly, not thread safe
    std::ostream& logStream() { return ls_; }
    //! macro utility function - do not use directly, not thread safe
    void log(unsigned m);

    //! mutex to acquire locks
    boost::shared_mutex& mutex() { return mutex_; }

    // Avoid a large number of warnings in VS by adding 0 !=
    bool filter(unsigned mask) {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return 0 != (mask & mask_);
    }
    unsigned mask() {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return mask_;
    }
    void setMask(unsigned mask) {
        boost::unique_lock<boost::shared_mutex> lock(mutex());
        mask_ = mask;
    }
    const boost::filesystem::path& rootPath() {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return rootPath_;
    }
    void setRootPath(const boost::filesystem::path& pth) {
        boost::unique_lock<boost::shared_mutex> lock(mutex());
        rootPath_ = pth;
    }
    int maxLen() {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return maxLen_;
    }
    void setMaxLen(const int n) {
        boost::unique_lock<boost::shared_mutex> lock(mutex());
        maxLen_ = n;
    }

    bool enabled() {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return enabled_;
    }
    void switchOn() {
        boost::unique_lock<boost::shared_mutex> lock(mutex());
        enabled_ = true;
    }
    void switchOff() {
        boost::unique_lock<boost::shared_mutex> lock(mutex());
        enabled_ = false;
    }

    bool writeSuppressedMessagesHint() {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return writeSuppressedMessagesHint_;
    }

    //! if a PID is set for the logger, messages are tagged with [1234] if pid = 1234
    void setPid(const int pid) { pid_ = pid; }

private:
    Log();

    // not thread safe
    std::string source(const char* filename, int lineNo) const;

    std::map<std::string, QuantLib::ext::shared_ptr<Logger>> loggers_;
    std::map<std::string, QuantLib::ext::shared_ptr<IndependentLogger>> independentLoggers_;
    bool enabled_;
    unsigned mask_;
    boost::filesystem::path rootPath_;
    std::ostringstream ls_;

    int maxLen_ = 45;
    std::size_t sameSourceLocationSince_ = 0;
    bool writeSuppressedMessagesHint_ = true;
    std::size_t sameSourceLocationCutoff_ = 1000;
    std::string lastFileName_;
    int lastLineNo_ = 0;

    int pid_ = 0;

    mutable boost::shared_mutex mutex_;

    std::map<std::string, std::function<bool(const std::string&)>> excludeFilters_;
};

/*!
  Main Logging macro, do not use this directly, use on of the below 6 macros instead
 */                               
#define MLOG(mask, text)                                                                                               \
    {                                                                                                                  \
        if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(mask)) {                         \
            std::ostringstream __ore_mlog_tmp_stringstream__;                                                          \
            __ore_mlog_tmp_stringstream__ << text;                                                                     \
            if (!ore::data::Log::instance().checkExcludeFilters(__ore_mlog_tmp_stringstream__.str())) {                \
                boost::unique_lock<boost::shared_mutex> lock(ore::data::Log::instance().mutex());                      \
                ore::data::Log::instance().header(mask, __FILE__, __LINE__);                                           \
                ore::data::Log::instance().logStream() << __ore_mlog_tmp_stringstream__.str();                         \
                ore::data::Log::instance().log(mask);                                                                  \
            }                                                                                                          \
        }                                                                                                              \
    }

//! Logging Macro (Level = Alert)
#define ALOG(text) MLOG(oreSeverity::alert, text);
//! Logging Macro (Level = Critical)
#define CLOG(text) MLOG(oreSeverity::critical, text)
//! Logging Macro (Level = Error)
#define ELOG(text) MLOG(oreSeverity::error, text)
//! Logging Macro (Level = Warning)
#define WLOG(text) MLOG(oreSeverity::warning, text)
//! Logging Macro (Level = Notice)
#define LOG(text) MLOG(oreSeverity::notice, text)
//! Logging Macro (Level = Debug)
#define DLOG(text) MLOG(oreSeverity::debug, text)
//! Logging Macro (Level = Data)
#define TLOG(text) MLOG(oreSeverity::data, text)

//! Logging macro specifically for logging memory usage
#define MEM_LOG MEM_LOG_USING_LEVEL(oreSeverity::memory)

#define MEM_LOG_USING_LEVEL(LEVEL)                                                                                      \
    {                                                                                                                   \
        if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(LEVEL)) {                         \
            boost::unique_lock<boost::shared_mutex> lock(ore::data::Log::instance().mutex());                           \
            ore::data::Log::instance().header(LEVEL, __FILE__, __LINE__);                                               \
            ore::data::Log::instance().logStream() << std::to_string(ore::data::os::getPeakMemoryUsageBytes()) << "|";  \
            ore::data::Log::instance().logStream() << std::to_string(ore::data::os::getMemoryUsageBytes());             \
            ore::data::Log::instance().log(LEVEL);                                                                      \
        }                                                                                                               \
    }

//! LoggerStream class that is a std::ostream replacement that will log each line
/*! LoggerStream is a simple wrapper around a std::string stream, it has an explicit
    cast std::ostream& method so it can be used in place of any std::ostream, this
    can be used with QuantExt methods that take a std::ostream& for logging purposes.

    Once the stream falls out of focus, it's destructor will take the buffered log
    messages and pass them the main ore::data::Log::instance().

    Note the following
    - The timestamps for each log message will correspond to when the LoggerStream
      destructor has been called, this may not correspond to the actual time the event
      occurred.
    - The filename and linenumber in the ore::data::Log() have to be explicitly passed to
      the LoggerStream, as such the log messages will not correspond to any references
      in QuantExt (or any other library).

    The LoggerStream relies on falling out of scope to trigger the logging, as such they
    should not be created using the new operator.

    The proper usage is to use the macro LOGGERSTREAM and DLOGGERSTREAM, if a function takes
    a std::ostream& as a parameter, use the macro instead.

    \code{.cpp}
    void function(int x, int y, std::ostream& out);

    void main () {

      // call function
      function (3, 4, LOGGERSTREAM);
      // All logging will be completed before this line
    }
    \endcode
 */
class LoggerStream {
public:
    LoggerStream(unsigned mask, const char* filename, unsigned lineNo);

    //! destructor - this is when the logging takes place.
    ~LoggerStream();

    //! cast this LoggerStream as a ostream&
    operator std::ostream &() { return ss_; }

private:
    unsigned mask_;
    const char* filename_;
    unsigned lineNo_;
    std::stringstream ss_;
};

#define CHECKED_LOGGERSTREAM(LEVEL, text)                                                       \
    if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(LEVEL)) {     \
        (std::ostream&)ore::data::LoggerStream(LEVEL, __FILE__, __LINE__) << text;              \
    }

#define ALOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_ALERT, text)
#define CLOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_CRITICAL, text)
#define ELOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_ERROR, text)
#define WLOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_WARNING, text)
#define LOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_NOTICE, text)
#define DLOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_DEBUG, text)
#define TLOGGERSTREAM(text) CHECKED_LOGGERSTREAM(ORE_DATA, text)

class JSONMessage {
public:
    virtual ~JSONMessage() {}
    //! return a std::string for the log file
    virtual std::string msg() const = 0;
    //! generate Boost log record to pass to corresponding sinks
    void log() const;
    //! create JSON-like output from the data
    const std::string json() const { return jsonify(data_); }
    void set(const std::string& key, const boost::any& value) { data_[key] = value; }

protected:
    //! generate Boost log record - this method is called by log()
    virtual void emitLog() const = 0;
    static std::string jsonify(const boost::any&);

    std::map<std::string, boost::any> data_;
};

// This can be used directly in log messages, e.g.
// ALOG(StructuredTradeErrorMessage(trade->id(), trade->tradeType(), "Error Parsing Trade", "Invalid XML Node foo"));
// And in the log file you will get
//
// .... StructuredMessage {
//    "category": "Error",
//    "group": "Trade",
//    "message": "Invalid XML Node foo",
//    "subFields": [
//        {
//            "fieldName": "exceptionType",
//            "fieldValue": "Error Parsing Trade"
//        },
//        {
//            "fieldName": "tradeId",
//            "fieldValue": "foo"
//        },
//        {
//            "fieldName": "tradeType",
//            "fieldValue": "Swap"
//        }
//    ]
//}
class StructuredMessage : public JSONMessage {
public:
    enum class Category { Error, Warning, Unknown };

    enum class Group { Analytics, Configuration, Model, Curve, Trade, Fixing, Logging, ReferenceData, Unknown };

    StructuredMessage(const Category& category, const Group& group, const std::string& message,
                      const std::map<std::string, std::string>& subFields = std::map<std::string, std::string>());

    StructuredMessage(const Category& category, const Group& group, const std::string& message,
                      const std::pair<std::string, std::string>& subField = std::pair<std::string, std::string>())
        : StructuredMessage(category, group, message, std::map<std::string, std::string>({subField})) {}

    virtual ~StructuredMessage() {}

    static constexpr const char* name = "StructuredMessage";

    //! return a std::string for the log file
    std::string msg() const { return std::string(name) + std::string(" ") + json(); }
    //! generate Boost log record to pass to corresponding sinks
    void emitLog() const;

protected:
    void addSubFields(const std::map<std::string, std::string>&);
};

std::ostream& operator<<(std::ostream& out, const StructuredMessage::Category&);

std::ostream& operator<<(std::ostream& out, const StructuredMessage::Group&);

class StructuredLoggingErrorMessage : public StructuredMessage {
public:
    StructuredLoggingErrorMessage(const std::string& exceptionType, const std::string& exceptionWhat = "")
        : StructuredMessage(Category::Error, Group::Logging, exceptionWhat,
                            std::pair<std::string, std::string>({"exceptionType", exceptionType})){};
};

class EventMessage : public JSONMessage {
public:
    EventMessage(const std::string& msg, const std::string& msgKey, const std::map<std::string, boost::any> data = {}) {
        data_ = data;
        data_[msgKey] = msg;
    }

    static constexpr const char* name = "EventMessage";

    //! return a std::string for the log file
    std::string msg() const { return std::string(name) + std::string(" ") + json(); }
    //! generate Boost log record to pass to corresponding sinks
    void emitLog() const;

private:
    std::string message_;
};

class ProgressMessage : public JSONMessage {
public:
    ProgressMessage(const std::string&, const QuantLib::Size, const QuantLib::Size, const std::string& detail = "");

    static constexpr const char* name = "ProgressMessage";

    //! return a std::string for the log file
    std::string msg() const { return std::string(name) + std::string(" ") + json(); }
    //! generate Boost log record to pass to corresponding sinks
    void emitLog() const;
};

//! Singleton to control console logging
//
class ConsoleLog : public QuantLib::Singleton<ConsoleLog, std::integral_constant<bool, true>> {
    friend class QuantLib::Singleton<ConsoleLog, std::integral_constant<bool, true>>;
private:
    // may be empty but never uninitialised
    ConsoleLog() : enabled_(false), width_(50), progressBarWidth_(40) {}

    bool enabled_;
    QuantLib::Size width_;
    QuantLib::Size progressBarWidth_;
    mutable boost::shared_mutex mutex_;

public:
    bool enabled() {
        boost::shared_lock<boost::shared_mutex> lock(mutex());
        return enabled_;
    }
    QuantLib::Size width() {
        boost::shared_lock<boost::shared_mutex> lock(mutex_);
        return width_;
    }
    QuantLib::Size progressBarWidth() {
        boost::shared_lock<boost::shared_mutex> lock(mutex_);
        return progressBarWidth_;
    }
    void switchOn() {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        enabled_ = true;
    }
    void switchOff() {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        enabled_ = false;
    }
    void setWidth(QuantLib::Size w) {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        width_ = w;
    }
    void setProgressBarWidth(QuantLib::Size w) {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        progressBarWidth_ = w;
    }
    //! mutex to acquire locks
    boost::shared_mutex& mutex() { return mutex_; }
};

#define CONSOLEW(text)                                                                                                 \
    {                                                                                                                  \
        if (ore::data::ConsoleLog::instance().enabled()) {                                                             \
            Size w = ore::data::ConsoleLog::instance().width();                                                        \
            std::ostringstream oss;                                                                                    \
            oss << text;                                                                                               \
            Size len = oss.str().length();                                                                             \
            Size wsLen = w > len ? w - len : 1;                                                                        \
            oss << std::string(wsLen, ' ');                                                                            \
            boost::unique_lock<boost::shared_mutex> lock(ore::data::ConsoleLog::instance().mutex());                   \
            std::cout << oss.str();                                                                                    \
            std::cout << std::flush;                                                                                   \
        }                                                                                                              \
    }

#define CONSOLE(text)                                                                                                  \
    {                                                                                                                  \
        if (ore::data::ConsoleLog::instance().enabled()) {                                                             \
            std::ostringstream oss;                                                                                    \
            oss << text;                                                                                               \
            boost::unique_lock<boost::shared_mutex> lock(ore::data::ConsoleLog::instance().mutex());                   \
            std::cout << oss.str() << "\n";                                                                            \
            std::cout << std::flush;                                                                                   \
        }                                                                                                              \
    }

} // namespace data
} // namespace ore
