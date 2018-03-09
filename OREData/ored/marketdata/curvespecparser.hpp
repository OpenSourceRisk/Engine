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

/*! \file ored/marketdata/curvespecparser.hpp
    \brief CurveSpec parser
    \ingroup curves
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <ored/marketdata/curvespec.hpp>

namespace ore {
namespace data {

//! function to convert a string into a curve spec
/*! \ingroup curves
 */
boost::shared_ptr<CurveSpec> parseCurveSpec(const std::string&);
} // namespace data
} // namespace ore
