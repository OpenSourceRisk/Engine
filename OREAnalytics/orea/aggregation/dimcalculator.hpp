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

/*! \file orea/aggregation/dimcalculator.hpp
    \brief Dynamic Initial Margin calculator base class
    \ingroup analytics
*/

#pragma once

#include <orea/app/inputparameters.hpp>
#include <orea/aggregation/collatexposurehelper.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>

#include <ql/time/date.hpp>

#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! Dynamic Initial Margin Calculator base class
/*!
  Derived classes implement a constructor with the relevant additional input data
  and a build function that performs the DIM calculations for all netting sets and
  along all paths.
*/
class DynamicInitialMarginCalculator {
public:
    DynamicInitialMarginCalculator(
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
        //! VaR quantile, e.g. 0.99 for 99%
        Real quantile = 0.99,
        //! VaR holding period in calendar days
        Size horizonCalendarDays = 14,
	//! Actual t0 IM by netting set used to scale the DIM evolution, no scaling if the argument is omitted
	const std::map<std::string, Real>& currentIM = std::map<std::string, Real>());

    virtual ~DynamicInitialMarginCalculator() {}

    //! Model implied t0 DIM by netting set, does not need a call to build() before
    virtual map<string, Real> unscaledCurrentDIM() = 0;

    //! t0 IM by netting set, as provided as an arguments
    const map<string, Real>& currentIM() const { return currentIM_; }

    //! Compute dynamic initial margin along all paths and fill result structures
    virtual void build() = 0;

    //! DIM evolution report
    virtual void exportDimEvolution(ore::data::Report& dimEvolutionReport) const;

    //! DIM by nettingSet, date, sample returned as a regular NPV cube
    const QuantLib::ext::shared_ptr<NPVCube>& dimCube() { return dimCube_; }

    //! DIM matrix by date and sample index for the specified netting set
    const vector<vector<Real>>& dynamicIM(const string& nettingSet);

    //! Cash flow matrix by date and sample index for the specified netting set
    const vector<vector<Real>>& cashFlow(const string& nettingSet);

    //! Expected DIM vector by date for the specified netting set
    const vector<Real>& expectedIM(const string& nettingSet);

    //! Get the implied netting set specific scaling factors
    const std::map<std::string, Real>& getInitialMarginScaling() { return nettingSetScaling_; }

protected:
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<NPVCube> cube_, dimCube_;
    QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation_;
    QuantLib::ext::shared_ptr<AggregationScenarioData> scenarioData_;
    Real quantile_;
    Size horizonCalendarDays_;
    map<string, Real> currentIM_;

    bool cubeIsRegular_;
    Size datesLoopSize_;
    std::set<string> nettingSetIds_;
    map<string, Real> nettingSetScaling_;

    // For each netting set: matrix of values by date and sample, aggregated over trades
    map<string, vector<vector<Real>>> nettingSetNPV_;
    map<string, vector<vector<Real>>> nettingSetCloseOutNPV_;
    map<string, vector<vector<Real>>> nettingSetFLOW_;
    map<string, vector<vector<Real>>> nettingSetDeltaNPV_;
    map<string, vector<vector<Real>>> nettingSetDIM_;

    // For each netting set: vector of values by date, aggregated over trades and samples
    map<string, vector<Real>> nettingSetExpectedDIM_;
};

} // namespace analytics
} // namespace ore
