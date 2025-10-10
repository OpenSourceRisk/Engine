/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file orea/aggregation/dimdirectcalculator.hpp
    \brief Dynamic Initial Margin calculator reading dim directly from cube
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/dimcalculator.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! Dynamic Initial Margin Calculator using IM stored in netting set cube
class DirectDynamicInitialMarginCalculator : public DynamicInitialMarginCalculator {
public:
    DirectDynamicInitialMarginCalculator(
        //! Global input parameters
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        //! Driving portfolio consistent with the cube below
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        //! NPV cube resulting from the Monte Carlo simulation loop
        const QuantLib::ext::shared_ptr<NPVCube>& cube,
        //! Interpretation of the cube, regular NPV, MPoR grid etc
        const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation,
        //! Additional output of the MC simulation loop with numeraires, index fixings, FX spots etc
        const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
        //! cube containing IM values per netting set
        const QuantLib::ext::shared_ptr<NPVCube>& imCube,
        //! Actual t0 IM by netting set used to scale the DIM evolution, no scaling if the argument is omitted
	const std::map<std::string, Real>& currentIM = std::map<std::string, Real>());

    const map<string, Real>& unscaledCurrentDIM() const override { return unscaledCurrentDIM_; }
    void build() override;
    void exportDimEvolution(ore::data::Report& dimEvolutionReport) const override;

private:
    QuantLib::ext::shared_ptr<NPVCube> imCube_;
    std::map<std::string, Real> unscaledCurrentDIM_;
};

} // namespace analytics
} // namespace ore
