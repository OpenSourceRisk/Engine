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
#include <boost/shared_ptr.hpp>
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

namespace ore {
namespace data {
using std::string;

//! The Base Custom Log Handler class
/*!
  This base log handler class can be used to define your own custom handler and then registered with the Log class.
  Once registered it will recieve all log messages as soon as they occur via it's log() method
  \ingroup utilities
  \see Log
 */
class Logger {
public:
    //! Desctructor
    virtual ~Logger() {}

    //! The Log call back function
    /*!
      This function will be called every time a log message is produced.
      \param level the log level
      \param s the log message
     */
    virtual void log(unsigned level, const string& s) = 0;

    //! Returns the Logger name
    const string& name() { return name_; }

protected:
    //! Constructor
    /*!
      Implementations must provide a logger name
      \param name the logger name
     */
    Logger(const string& name) : name_(name) {}

private:
    string name_;
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
    static const string name;
    //! Constructor
    /*!
      This logger writes all logs to stderr.
      If alertOnly is set to true, it will only write alerts.
     */
    StderrLogger(bool alertOnly = false) : Logger(name), alertOnly_(alertOnly) {}
    //! Desctructor
    virtual ~StderrLogger() {}
    //! The log callback that writes to stderr
    virtual void log(unsigned l, const string& s) {
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
    static const string name;
    //! Constructor
    /*!
      Construct a file logger using the given filename, this filename is passed to std::fostream::open()
      and this constructor will throw an exception if the file is not opened (e.g. if the filename is invalid)
      \param filename the log filename
     */
    FileLogger(const string& filename);
    //! Desctructor
    virtual ~FileLogger();
    //! The log callback
    virtual void log(unsigned, const string&);

private:
    string filename_;
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
    static const string name;
    //! Constructor
    BufferLogger(unsigned minLevel = ORE_DATA) : Logger(name), minLevel_(minLevel) {}
    //! Desctructor
    virtual ~BufferLogger() {}
    //! The log callback
    virtual void log(unsigned, const string&);

    //! Checks if Logger has new messages
    /*!
      \return True if this BufferLogger has any new log messages
     */
    bool hasNext();
    //! Retreive new messages
    /*!
      Retreive the next new message from the buffer, this will throw if the buffer is empty.
      Messages are returned in a FIFO order. Messages are deleted from the buffer once returned.
      \return The next message
     */
    string next();

private:
    std::queue<string> buffer_;
    unsigned minLevel_;
};

//! Global static Log class
/*!
  The Global Log class gets registered with individual loggers and recieves application log messages.
  Once a message is recieved, it is imediatly dispatched to each of the registered loggers, the order in which
  the loggers are called is not guarenteed.

  Logging is done by the calling thread and the LOG call blocks until all the loggers have returned.

  At start up, the Log class has no loggers and so will ignore any LOG() messages until it is configured.

  To configure the Log class to log to a file "/tmp/my_log.txt"
  <pre>
      Log::instance().removeAllLoggers();
      Log::instance().registerLogger(boost::shared_ptr<Logger>(new FileLogger("/tmp/my_log.txt")));
  </pre>

  To change the Log class to only use a BufferLogger the user must call
  <pre>
      Log::instance().removeAllLoggers();
      Log::instance().registerLogger(boost::shared_ptr<Logger>(new BufferLogger));
  </pre>
  and then to retreive log messages from the buffer and print them to stdout the user must call:
  <pre>
      std::cout << "Begin Log Messages:" << std::endl;

      boost::shared_ptr<BufferLogger> bl = boost::dynamic_pointer_cast<BufferLogger>
        (Log::instance().logger(BufferLogger::name));

      while (bl.hasNext())
          std::cout << bl.next() << std::endl;
      std::cout << "End Log Messages." << std::endl;
  </pre>
  \ingroup utilities
 */
class Log : public QuantLib::Singleton<Log> {
    friend class QuantLib::Singleton<Log>;

public:
    //! Add a new Logger.
    /*!
      Adds a new logger to the Log class, the logger will be stored by it's Logger::name().
      This method will throw if a logger with the same name is already registered.
      \param logger the logger to add
     */
    void registerLogger(const boost::shared_ptr<Logger>& logger);
    //! Retrieve a Logger.
    /*!
      Retrieve a Logger by it's name, for example to retrieve the StderrLogger (assuming it is registered)
      <pre>
      boost::shared_ptr<Logger> slogger = Log::instance().logger(StderrLogger::name);
      </pre>
      */
    boost::shared_ptr<Logger>& logger(const string& name);
    //! Remove a Logger
    /*!
      Remove a logger by name
      \param name the logger name
     */
    void removeLogger(const string& name);
    //! Remove all loggers
    /*!
      Removes all loggers. If called, all subsiquent log messages will be ignored.
     */
    void removeAllLoggers();

    //! macro utility function - do not use directly
    void header(unsigned m, const char* filename, int lineNo);
    //! macro utility function - do not use directly
    std::ostream& logStream() { return ls_; }
    //! macro utility function - do not use directly
    void log(unsigned m);

    // Avoid a large number of warnings in VS by adding 0 !=
    bool filter(unsigned mask) { return 0 != (mask & mask_); }
    unsigned mask() { return mask_; }
    void setMask(unsigned mask) { mask_ = mask; }

    bool enabled() { return enabled_; }
    void switchOn() { enabled_ = true; }
    void switchOff() { enabled_ = false; }

    //! if a PID is set for the logger, messages are tagged with [1234] if pid = 1234
    void setPid(const int pid) { pid_ = pid; }

private:
    Log();

    std::map<string, boost::shared_ptr<Logger>> loggers_;
    bool enabled_;
    unsigned mask_;
    std::ostringstream ls_;

    int pid_ = 0;
};

/*!
  Main Logging macro, do not use this directly, use on of the below 6 macros instead
 */
#define MLOG(mask, text)                                                                                               \
    if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(mask)) {                             \
        ore::data::Log::instance().header(mask, __FILE__, __LINE__);                                                   \
        ore::data::Log::instance().logStream() << text;                                                                \
        ore::data::Log::instance().log(mask);                                                                          \
    }

//! Logging Macro (Level = Alert)
#define ALOG(text) MLOG(ORE_ALERT, text);
//! Logging Macro (Level = Critical)
#define CLOG(text) MLOG(ORE_CRITICAL, text)
//! Logging Macro (Level = Error)
#define ELOG(text) MLOG(ORE_ERROR, text)
//! Logging Macro (Level = Warning)
#define WLOG(text) MLOG(ORE_WARNING, text)
//! Logging Macro (Level = Notice)
#define LOG(text) MLOG(ORE_NOTICE, text)
//! Logging Macro (Level = Debug)
#define DLOG(text) MLOG(ORE_DEBUG, text)
//! Logging Macro (Level = Data)
#define TLOG(text) MLOG(ORE_DATA, text)

//! Logging macro specifically for logging memory usage
#define MEM_LOG                                                                                                        \
    if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(ORE_MEMORY)) {                       \
        ore::data::Log::instance().header(ORE_MEMORY, __FILE__, __LINE__);                                             \
        ore::data::Log::instance().logStream() << std::to_string(ore::data::os::getPeakMemoryUsageBytes()) << "|";     \
        ore::data::Log::instance().logStream() << std::to_string(ore::data::os::getMemoryUsageBytes());                \
        ore::data::Log::instance().log(ORE_MEMORY);                                                                    \
    }

//! LoggerStream class that is a std::ostream replacment that will log each line
/*! LoggerStream is a simple wrapper around a string stream, it has an explicit
    cast std::ostream& method so it can be used in place of any std::ostream, this
    can be used with QuantExt methods that take a std::ostream& for logging purposes.

    Once the stream falls out of focus, it's desstructor will take the buffered log
    messages and pass them the main ore::data::Log::instance().

    Note the following
    - The timestamps for each log message will correspond to when the LoggerStream
      desstructor has been called, this may not correspond to the actual time the event
      occured.
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

#define ALOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_ALERT, __FILE__, __LINE__))
#define CLOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_CRITICAL, __FILE__, __LINE__))
#define ELOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_ERROR, __FILE__, __LINE__))
#define WLOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_WARNING, __FILE__, __LINE__))
#define LOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_NOTICE, __FILE__, __LINE__))
#define DLOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_DEBUG, __FILE__, __LINE__))
#define TLOGGERSTREAM ((std::ostream&)ore::data::LoggerStream(ORE_DATA, __FILE__, __LINE__))

//! Utility class for having structured Error messages
// This can be used directly in log messages, e.g.
// ALOG(StructuredTradeErrorMessage(trade->id(), trade->tradeType(), "Error Parsing Trade", "Invalid XML Node foo"));
// And in the log file you will get
//
// .... StructuredErrorMessage { "errorType":"Trade", "tradeId":"foo", "tradeType":"SWAP" }
class StructuredErrorMessage {
public:
    virtual ~StructuredErrorMessage() {}
    static constexpr const char* name = "StructuredErrorMessage";

    //! return a string for the log file
    std::string msg() const { return string(name) + string(" ") + json(); }

protected:
    // This should return a structured string, ideally in JSON, and should contain a field
    // errorType
    virtual std::string json() const = 0;

    // utility function to delimate string for json, handles \" and \\ and control characters
    std::string jsonify(const std::string& s) const;
};

inline std::ostream& operator<<(std::ostream& out, const StructuredErrorMessage& sem) { return out << sem.msg(); }

} // namespace data
} // namespace ore
