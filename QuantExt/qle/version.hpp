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

/*! \file qle/version.hpp
    \brief Version
*/

#ifndef quantext_version_hpp
#define quantext_version_hpp

// Boost Version
// Boost 1.55 has been tested on Linux and 1.57 on Windows.
// Any boost version above 1.46 might work (we use boost filesystem 3 which is the default
// from 1.46 - so some modification will be needed for pre 1.46).
//
// Note that for MSVC 2013 the boost config file needs 1.57 (but this can be modified).
// Note that for MSVC 2015 the boost config file needs 1.58 (but this can be modified).
#include <boost/version.hpp>
#if BOOST_VERSION < 105500
#error using an old version of Boost, please update.
#endif

// We require QuantLib 1.11 or higher
#include <ql/version.hpp>
#if QL_HEX_VERSION < 0x011100f0
#error using an old version of QuantLib, please update.
#endif

//! Version string
#define OPEN_SOURCE_RISK_VERSION "1.8.3.0"

//! Version number
#define OPEN_SOURCE_RISK_VERSION_NUM 1080300

#endif
