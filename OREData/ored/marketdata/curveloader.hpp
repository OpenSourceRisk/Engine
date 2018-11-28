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

/*! \file ored/marketdata/curveloader.hpp
    \brief Wrapper function for triggering curve building
    \ingroup marketdata
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <vector>

using ore::data::CurveConfigurations;

namespace ore {
namespace data {
//! Construct term structure wrappers
/*!
  This function
  - scans the list of provided curve specs
  - removes duplicates
  - re-orders them so that they can be loaded sequentially.

  If a dependency cycle is detected this function will throw

  \ingroup marketdata
 */
void order(vector<boost::shared_ptr<CurveSpec>>& curveSpecs, 
    const CurveConfigurations& curveConfigs, 
    std::map<std::string, std::string>& errors);
} // namespace data
} // namespace ore
