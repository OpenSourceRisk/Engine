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
static std::vector<Real> _s_backoff = {0.5, 1, 2, 4, 8, 16, 30};

Size FileIO::maxRetries() { return _s_maxRetries; }

void FileIO::setMaxRetries(Size numOfRetries) {
    LOG("Setting FileOpen max retries to " << numOfRetries);
    _s_maxRetries = numOfRetries;
}

FILE* FileIO::fopen(const char* filename, const char* mode) {
    FILE* fp;
    int i = 0;

    do {
        if (i > 0) {
            Real backoff = (i >= _s_backoff.size()) ? _s_backoff.back() : _s_backoff[i - 1];
            int backoffMillis = backoff * 1000;
            auto em = EventMessage("Error opening file '" + std::string(filename) + "'.");
            em.set("retry_count", i);
            em.set("retry_interval", backoffMillis);
            WLOG(em);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMillis));
        }

        fp = std::fopen(filename, mode);

        i++;
    } while (fp == nullptr && i <= maxRetries());

    return fp;
}

bool FileIO::create_directories(const path& p) {
    bool res = false;
    int i = 0;

    do {
        if (i > 0) {
            Real backoff = (i >= _s_backoff.size()) ? _s_backoff.back() : _s_backoff[i - 1];
            int backoffMillis = backoff * 1000;
            auto em = EventMessage("Error creating directory '" + p.string() + "'.");
            em.set("retry_count", i);
            em.set("retry_interval", backoffMillis);
            WLOG(em);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMillis));
        }

        try {
            res = boost::filesystem::create_directories(p);
        } catch (...) {
        }

        i++;
    } while (res == false && i <= maxRetries());

    return res;
}

} // namespace data
} // namespace ore
