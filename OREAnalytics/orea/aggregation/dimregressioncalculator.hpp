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

/*! \file orea/aggregation/dimregressioncalculator.hpp
    \brief Dynamic Initial Margin calculator by regression
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

//! Dynamic Initial Margin Calculator using polynomial regression
/*!
  Dynamic IM is estimated using polynomial and local regression methods applied to the NPV moves over simulation time
  steps across all paths.
*/
class RegressionDynamicInitialMarginCalculator : public DynamicInitialMarginCalculator {
public:
    RegressionDynamicInitialMarginCalculator(
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
        //! VaR quantile expressed as a percentage
        Real quantile,
        //! VaR holding period in calendar days
        Size horizonCalendarDays,
        //! Polynom order of the regression
        Size regressionOrder,
        //! Regressors to be used
        vector<string> regressors,
        //! Number of local regression evaluation windows across all paths, up to number of samples
        Size localRegressionEvaluations = 0,
        //! Local regression band width in standard deviations of the regression variable
        Real localRegressionBandWidth = 0,
	//! Actual t0 IM by netting set used to scale the DIM evolution, no scaling if the argument is omitted
	const std::map<std::string, Real>& currentIM = std::map<std::string, Real>());

    map<string, Real> unscaledCurrentDIM() override;
    void build() override;
    void exportDimEvolution(ore::data::Report& dimEvolutionReport) const override;

    void exportDimRegression(const std::string& nettingSet, const std::vector<Size>& timeSteps,
                             const std::vector<QuantLib::ext::shared_ptr<ore::data::Report>>& dimRegReports);

    const vector<vector<Real>>& localRegressionResults(const string& nettingSet);
    const vector<Real>& zeroOrderResults(const string& nettingSet);
    const vector<Real>& simpleResultsUpper(const string& nettingSet);
    const vector<Real>& simpleResultsLower(const string& nettingSet);

private:
    //! Compile the array of DIM regressors for the specified netting set, date and sample index
    Array regressorArray(string nettingSet, Size dateIndex, Size sampleIndex);

    Size regressionOrder_;
    vector<string> regressors_;
    Size localRegressionEvaluations_;
    Real localRegressionBandWidth_;

    // For each netting set: Array of regressor values by date and sample
    map<string, vector<vector<Array>>> regressorArray_;
    // For each netting set: local regression DIM estimate by date and sample
    map<string, vector<vector<Real>>> nettingSetLocalDIM_;
    // For each netting set: vector of values by date, aggregated over trades and samples
    map<string, vector<Real>> nettingSetZeroOrderDIM_;
    map<string, vector<Real>> nettingSetSimpleDIMh_;
    map<string, vector<Real>> nettingSetSimpleDIMp_;
};

inline bool lessThan(const Array& a, const Array& b) {
    QL_REQUIRE(a.size() > 0, "array a is empty");
    QL_REQUIRE(b.size() > 0, "array a is empty");
    return a[0] < b[0];
}

} // namespace analytics
} // namespace ore
