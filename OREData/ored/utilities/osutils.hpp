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

/*! \file ored/utilities/osutils.hpp
    \brief Various OS specific utilities
    \ingroup utilities
*/

#pragma once

#include <string>

namespace ore {
namespace data {
namespace os {

/*! \addtogroup utilities
    @{
*/
//! Returns the OS Name
std::string getOsName();

//! Returns the OS Version
std::string getOsVersion();

//! Returns the CPU name (e.g. "Intel(R) Core(TM) i7-3720QM CPU @ 2.60GHz"
std::string getCpuName();

//! Returns the number of Cores available to the OS.
unsigned int getNumberCores();

//! Returns the total amount of memory available (installed RAM)
std::string getMemoryRAM();

//! Returns the current process memory usage
std::string getMemoryUsage();

//! Returns the current process peak memory usage
std::string getPeakMemoryUsage();

/*! Returns the current process memory usage in bytes
    Parts of code taken from:
    http://nadeausoftware.com/articles/2012/07/c_c_tip_how_get_process_resident_set_size_physical_memory_use
*/
unsigned long long getMemoryUsageBytes();

//! Returns the current process peak memory usage in bytes
unsigned long long getPeakMemoryUsageBytes();

//! Returns the current username
std::string getUsername();

//! Returns the machine name
std::string getHostname();

//! Returns all the above system details in a single string
std::string getSystemDetails();

//! Write the current stacktrace to stderr and LOG() with level = ALERT
void dumpStacktrace();

//! Set an assert handler that logs the stacktrace
void setAssertHandler();

//! @}
}; // namespace os
} // namespace data
} // namespace ore
