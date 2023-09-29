/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file ored/utilties/fileio.cpp
    \brief Wrapper class for retrying file IO operations
    \ingroup
*/

#include <boost/filesystem/operations.hpp>
#include <chrono>
#include <ored/utilities/fileio.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <thread>
#include <vector>

namespace ore {
namespace data {

using QuantLib::Real;
using QuantLib::Size;

// Defaults
static Size _s_maxRetries = 7;
static Real _s_backoff = 0.5;
static Real _s_maxBackoff = 30;

Size FileIO::maxRetries() { return _s_maxRetries; }

Real FileIO::backoff() { return _s_backoff; }

Real FileIO::maxBackoff() { return _s_maxBackoff; }

void FileIO::setMaxRetries(Size n) {
    LOG("Setting FileOpen max retries to " << n);
    _s_maxRetries = n;
}

void FileIO::setBackoff(Real b) {
    LOG("Setting FileOpen backoff to " << b);
    _s_backoff = b;
}

void FileIO::setMaxBackoff(Real m) {
    LOG("Setting FileOpen max backoff to " << m);
    _s_maxBackoff = m;
}

FILE* FileIO::fopen(const char* filename, const char* mode) {
    FILE* fp;
    Real currentBackoff = backoff();

    for (Size i = 0; i <= maxRetries(); i++) {
        if (i > 0) {
            auto em = EventMessage("Error opening file '" + std::string(filename) + "'. Retrying...", "exception_message");
            em.set("retry_count", i);
            Real backoffMillis = currentBackoff * 1000;
            em.set("retry_interval", backoffMillis);
            WLOG(em);
            std::this_thread::sleep_for(std::chrono::duration<Real>(currentBackoff));
            Real nextBackoff = currentBackoff * 2;
            currentBackoff = (nextBackoff >= maxBackoff()) ? maxBackoff() : nextBackoff;
        }

        fp = std::fopen(filename, mode);
        if (fp)
            break;
    }

    return fp;
}

bool FileIO::create_directories(const path& p) {
    bool res = false;
    Real currentBackoff = backoff();

    for (Size i = 0; i <= maxRetries(); i++) {
        if (i > 0) {
            auto em = EventMessage("Error creating directory '" + p.string() + "'. Retrying...", "exception_message");
            em.set("retry_count", i);
            Real backoffMillis = currentBackoff * 1000;
            em.set("retry_interval", backoffMillis);
            WLOG(em);
            std::this_thread::sleep_for(std::chrono::duration<Real>(currentBackoff));
            Real nextBackoff = currentBackoff * 2;
            currentBackoff = (nextBackoff >= maxBackoff()) ? maxBackoff() : nextBackoff;
        }

        try {
            res = boost::filesystem::create_directories(p);
            if (res)
                break;
        } catch (...) {
        }
    }

    return res;
}

bool FileIO::remove_all(const path& p) {
    bool res = false;
    Real currentBackoff = backoff();

    for (Size i = 0; i <= maxRetries(); i++) {
        if (i > 0) {
            auto em = EventMessage("Error emptying directory '" + p.string() + "'. Retrying...", "exception_message");
            em.set("retry_count", i);
            Real backoffMillis = currentBackoff * 1000;
            em.set("retry_interval", backoffMillis);
            WLOG(em);
            std::this_thread::sleep_for(std::chrono::duration<Real>(currentBackoff));
            Real nextBackoff = currentBackoff * 2;
            currentBackoff = (nextBackoff >= maxBackoff()) ? maxBackoff() : nextBackoff;
        }

        try {
            res = boost::filesystem::remove_all(p);
            if (res)
                break;
        } catch (...) {
        }
    }

    return res;
}

} // namespace data
} // namespace ore
