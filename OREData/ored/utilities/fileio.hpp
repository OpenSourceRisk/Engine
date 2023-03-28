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

/*! \file ored/utilities/fileio.hpp
    \brief Wrapper class for retrying file IO operations
    \ingroup utilities
*/

#pragma once

#include <boost/filesystem/path.hpp>
#include <ql/types.hpp>
#include <stdio.h>

namespace ore {
namespace data {

using boost::filesystem::path;

class FileIO {

public:
    FileIO() = delete;

    //! The maximum number of retries, defaults to 7
    static QuantLib::Size maxRetries();
    static QuantLib::Real backoff();
    static QuantLib::Real maxBackoff();
    static void setMaxRetries(QuantLib::Size);
    static void setBackoff(QuantLib::Real);
    static void setMaxBackoff(QuantLib::Real);

    //! Retry wrapper for std::fopen
    static FILE* fopen(const char*, const char*);

    //! Retry wrapper for boost::filesystem::create_directories
    static bool create_directories(const path&);

    //! Retry wrapper for boost::filesystem::remove_all
    static bool remove_all(const path&);
};

} // namespace data
} // namespace ore
