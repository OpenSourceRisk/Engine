/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/**
 * \file portfolio/builders/utilities.hpp
 * \brief Utility functions providing shared logicfor engine builders.
 * \ingroup builders
 */
#pragma once
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

struct FiniteDifferenceParams {
    QuantLib::FdmSchemeDesc scheme = QuantLib::FdmSchemeDesc::Douglas();
    QuantLib::Size tGrid = QuantLib::Null<QuantLib::Size>();
    QuantLib::Size xGrid = QuantLib::Null<QuantLib::Size>();
    QuantLib::Size dampingSteps = QuantLib::Null<QuantLib::Size>();
    bool monotoneVar = false;
    std::vector<QuantLib::Time> timePoints;

    explicit FiniteDifferenceParams(QuantLib::FdmSchemeDesc s = QuantLib::FdmSchemeDesc::Douglas())
        : scheme(std::move(s)) {}
};

FiniteDifferenceParams fdSchemeParams(const EngineBuilder& engineBuilder, QuantLib::Time horizon,
    bool includeDampingStepsInTotalSteps = true);

} // namespace data
} // namespace ore
