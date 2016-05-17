/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2011 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#pragma once

#include <ql/time/date.hpp>
#include <version.hpp>

/* Timebomb macro, to be put at begining of every main()
 */


//! no timebomb
//#define QRE_CHECK_TIMEBOMB 


/* Timebomb date hardcoded to 1 Oct 2016
 * Use __DATE__ macro to enable "Release date + 90 days" type eval periods.
 */

#define ORE_CHECK_TIMEBOMB \
  { \
    QuantLib::Date timebombDate = QuantLib::Date(1, QuantLib::October, 2016); \
    QuantLib::Date today = QuantLib::Date::todaysDate(); \
    if (today >= timebombDate) { \
        printVersion(); \
        std::cout << std::endl; \
        std::cout << "** Evaluation Period Expired **" << std::endl; \
        std::cout << "Contact info@quaternion.com for licence renewal." << std::endl; \
        exit (-1); \
    } \
  } 

