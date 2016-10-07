/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

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

namespace openriskengine {
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

//! Returns the current process memory usage in bytes
unsigned long long getMemoryUsageBytes();

//! Returns the current username
std::string getUsername();

//! Returns the machine name
std::string getHostname();

//! Returns all the above system details in a single string
std::string getSystemDetails();

//! @}
};
}
}
