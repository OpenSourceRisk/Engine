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

#include <ored/portfolio/builders/utilities.hpp>
#include <vector>

namespace ore {
namespace data {

using namespace QuantLib;
using std::vector;

FiniteDifferenceParams fdSchemeParams(const EngineBuilder& eb, Time horizon, bool includeDampingStepsInTotalSteps) {

    FiniteDifferenceParams res{ parseFdmSchemeDesc(eb.engineParameter("Scheme")) };
    res.xGrid = parseInteger(eb.engineParameter("XGrid"));
    res.dampingSteps = parseInteger(eb.engineParameter("DampingSteps"));
    res.monotoneVar = parseBool(eb.engineParameter("EnforceMonotoneVariance", {}, false, "true"));
    Size tGridMin = parseInteger(eb.engineParameter("TimeGridMinimumSize", {}, false, "1"));
    res.tGrid = static_cast<Size>(parseInteger(eb.engineParameter("TimeGridPerYear")) * horizon);
    res.tGrid = std::max(tGridMin, res.tGrid);

    // If monotone variance is true, populate the timePoints member.
    if (res.monotoneVar) {
        // Replicate the construction of time grid in FiniteDifferenceModel::rollbackImpl
        // This time grid is required to build a BlackMonotoneVarVolTermStructure which
        // ensures monotonic variance along the time grid
        Size totalSteps = includeDampingStepsInTotalSteps ? res.tGrid + res.dampingSteps : res.tGrid;
        res.timePoints.resize(totalSteps + 1);
        Array timePointsArray(totalSteps, horizon, -horizon / totalSteps);
        res.timePoints[0] = 0.0;
        for (Size i = 0; i < totalSteps; i++)
            res.timePoints[res.timePoints.size() - i - 1] = timePointsArray[i];
        res.timePoints.insert(std::upper_bound(res.timePoints.begin(), res.timePoints.end(), 0.99 / 365), 0.99 / 365);
    }

    return res;
}

} // namespace data
} // namespace ore
