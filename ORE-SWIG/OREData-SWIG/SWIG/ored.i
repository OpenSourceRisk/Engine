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

#ifndef ored_i
#define ored_i

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

#ifdef BOOST_MSVC
#include <ored/auto_link.hpp>
#endif

#include <ored/ored.hpp>

%}


%include ored_calendarAdjustmentConfig.i
%include ored_curvespec.i
%include ored_curveconfigurations.i
%include ored_crossassetmodeldata.i
%include ored_conventions.i
%include ored_loader.i
%include ored_log.i
%include ored_market.i
%include ored_marketdatum.i
%include ored_parsers.i
%include ored_portfolio.i
%include ored_reports.i
%include ored_referencedatamanager.i
%include ored_todaysmarketparameters.i
%include ored_tradegenerator.i
%include ored_utilities.i
%include ored_volcurves.i
%include ored_xmlutils.i

#endif
