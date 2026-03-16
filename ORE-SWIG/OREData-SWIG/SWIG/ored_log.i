/*
 Copyright (C) 2019, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_log_i
#define ored_log_i

//          accumulated 'filter' for 'external' DEBUG_MASK
#define ORE_ALERT 1    // 00000001   1 = 2^1-1
#define ORE_CRITICAL 2 // 00000010   3 = 2^2-1
#define ORE_ERROR 4    // 00000100   7
#define ORE_WARNING 8  // 00001000  15
#define ORE_NOTICE 16  // 00010000  31
#define ORE_DEBUG 32   // 00100000  63 = 2^6-1
#define ORE_DATA 64    // 01000000  127

%{
using ore::data::Log;
using ore::data::Logger;
%}

%shared_ptr(Log)
class Log {
  private:
    Log();
  public:
    static Log& instance();
    %extend {
        void registerLogger(const ext::shared_ptr<Logger>& logger) {
            self->registerLogger(logger);
        }
        ext::shared_ptr<Logger>& logger(const std::string& name) {
            return self->logger(name);
        }
        void removeLogger(const std::string& name) {
            self->removeLogger(name);
        }
        void removeAllLoggers() {
            self->removeAllLoggers();
        }
        /*
        void header(unsigned m, const char* filename, int lineNo) {
            self->header(m, filename, lineNo);
        }
        void std::ostream& logStream() {
            return self->logStream();
        }
        void log(unsigned m) {
            self->log(m);
        }*/
        bool filter(unsigned mask) {
            return self->filter(mask);
        }
        unsigned mask() {
            return self->mask();
        }
        void setMask(unsigned mask) {
            self->setMask(mask);
        }
        bool enabled() {
            return self->enabled();
        }
        void switchOn() {
            self->switchOn();
        }
        void switchOff() {
            self->switchOff();
        }

    }
};

%shared_ptr(Logger)
class Logger {
  private:
    Logger();
};

%{
using ore::data::FileLogger;
%}

%shared_ptr(FileLogger)
class FileLogger : public Logger {
  public:
    FileLogger(const std::string& filename);
};

%{
using ore::data::BufferLogger;
%}

%shared_ptr(BufferLogger)
class BufferLogger : public Logger {
  public:
    BufferLogger(unsigned minLevel = ORE_DATA);
    bool hasNext();
    std::string next();
};

%rename(MLOG) MLOGSWIG;
%inline %{
static void MLOGSWIG(unsigned mask, const std::string& text, const char* filename, int lineNo) {
    QL_REQUIRE(lineNo > 0, "lineNo must be greater than 0");
    if (ore::data::Log::instance().enabled() && ore::data::Log::instance().filter(mask)) {
        ore::data::Log::instance().header(mask, filename, lineNo);
        ore::data::Log::instance().logStream() << text;
        ore::data::Log::instance().log(mask);
    }
}
%}

%define export_log(Name,Mask)

#if defined(SWIGPYTHON)
%pythoncode %{
def Name##(text):
    import inspect
    current_frame = inspect.currentframe()
    call_frame = inspect.getouterframes(current_frame, 2)[-1]
    MLOG(Mask##, text, call_frame.filename, call_frame.lineno)
%}
#endif

%enddef

export_log(ALOG, ORE_ALERT);
export_log(CLOG, ORE_CRITICAL);
export_log(ELOG, ORE_ERROR);
export_log(WLOG, ORE_WARNING);
export_log(LOG, ORE_NOTICE);
export_log(DLOG, ORE_DEBUG);
export_log(TLOG, ORE_DATA);

#endif
