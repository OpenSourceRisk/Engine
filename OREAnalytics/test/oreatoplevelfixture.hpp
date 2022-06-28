/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file test/oreatoplevelfixture.hpp
    \brief Fixture that can be used at top level of OREAnalytics test suites
*/

#pragma once

#include <oret/toplevelfixture.hpp>

#include <orea/engine/observationmode.hpp>

namespace ore {
namespace test {

using ore::analytics::ObservationMode;

//! OREAnalytics Top level fixture
class OreaTopLevelFixture : public TopLevelFixture {
public:
    ObservationMode::Mode savedObservationMode;

    OreaTopLevelFixture() : TopLevelFixture() { savedObservationMode = ObservationMode::instance().mode(); }

    ~OreaTopLevelFixture() { ObservationMode::instance().setMode(savedObservationMode); }
};
} // namespace test
} // namespace ore
