/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#ifndef orea_i
#define orea_i

%{
#include <boost/shared_ptr.hpp>
#include <boost/assert.hpp>
#include <boost/current_function.hpp>

#include <exception>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <ql/errors.hpp>

#include <orea/orea.hpp>

#ifdef BOOST_MSVC
#include <orea/auto_link.hpp>
#define BOOST_LIB_NAME boost_regex
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_serialization
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_date_time
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_filesystem
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#endif

%}

%init %{
    ore::analytics::initBuilders();
%}

%include <windows.i>

%include orea_app.i
%include orea_cube.i
%include orea_scenariosimmarketparameters.i
%include orea_scenario.i

#endif
