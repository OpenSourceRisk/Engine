/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/aggregation/dynamicdeltavarcalculator.hpp
    \brief Dynamic delta/gamma VaR for dynamic IM
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/dimcalculator.hpp>
#include <orea/aggregation/dimhelper.hpp>

namespace ore {
namespace analytics {

//! Dynamic Delta/Gamma VaR for Initial Margin
/*!
    Dynamic IM is estimated using delta/gamma VaR calculation along all paths,
    based on sensitivities stored in the hyper cube
*/
class DynamicDeltaVaRCalculator : public ore::analytics::DynamicInitialMarginCalculator {
public:
    DynamicDeltaVaRCalculator(
        //! Global input parameters,
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        //! Driving portfolio consistent with the cube below
        const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
        //! NPV cube resulting from the Monte Carlo simulation loop
        const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube,
        //! Interpretation of the cube, regular NPV, MPoR grid etc
        const QuantLib::ext::shared_ptr<ore::analytics::CubeInterpretation>& cubeInterpretation,
        //! Additional output of the MC simulation loop with numeraires, index fixings, FX spots etc
        const QuantLib::ext::shared_ptr<ore::analytics::AggregationScenarioData>& scenarioData,
        //! VaR quantile expressed as a percentage
        QuantLib::Real quantile,
        //! VaR holding period in calendar days
        QuantLib::Size horizonCalendarDays,
        //! DIM Helper for dynamic Delta/Gamma VaR calculation
        const QuantLib::ext::shared_ptr<DimHelper>& dimHelper,
        //! Dynamic Delta/Gamma VaR order (1 for delta, 2 for delta-gamma)
        const QuantLib::Size ddvOrder,
        //! Actual t0 IM by netting set used to scale the DIM evolution, no scaling if the argument is omitted
        const std::map<std::string, QuantLib::Real>& currentIM = std::map<std::string, QuantLib::Real>());

    std::map<std::string, QuantLib::Real> unscaledCurrentDIM() override;
    void build() override;

private:
    QuantLib::ext::shared_ptr<DimHelper> dimHelper_;
    QuantLib::Size ddvOrder_;
};

} // namespace analytics
} // namespace ore
