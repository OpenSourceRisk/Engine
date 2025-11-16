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

/*! \file ored/model/hestonmodelbuilder.hpp
    \brief builder for an array of heston processes
    \ingroup utilities
*/

#pragma once

#include <ored/model/assetmodelbuilderbase.hpp>
#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

class HestonModelBuilder final : public AssetModelBuilderBase {
public:
    HestonModelBuilder(const std::vector<Handle<YieldTermStructure>>& curves,
                       const std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                       const std::set<Date>& simulationDates = {}, const std::set<Date>& addDates = {},
                       const Size timeStepsPerYear = 1,
                       const std::vector<Period>& calibrationExpiries = {3 * Months, 6 * Months, 1 * Years, 2 * Years,
                                                                         3 * Years, 5 * Years},
                       const std::vector<Real>& calibrationMoneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
                       const std::vector<Period>& calibrationVarianceTerms = {1 * Months, 3 * Months, 6 * Months,
                                                                              1 * Years, 2 * Years, 3 * Years},
                       // theta, kappa, sigma, rho, v0 (same order as in the Heston model, not the Heston process)
                       const std::vector<Real>& initialValues = {0.04, 1.0, 0.5, -0.5, 0.04},
                       const std::vector<bool>& fixedValues = {false, false, false, false, false},
                       Real relaxedFellerConstraint = 1.0, Size calibrationRestarts = 50, Real tolerance = 0.001,
                       const std::string& referenceCalibrationGrid = "", const bool dontCalibrate = false,
                       const Handle<YieldTermStructure>& baseCurve = {});
    HestonModelBuilder(const Handle<YieldTermStructure>& curve,
                       const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                       const std::set<Date>& simulationDates = {}, const std::set<Date>& addDates = {},
                       const Size timeStepsPerYear = 1,
                       const std::vector<Period>& calibrationExpiries = {3 * Months, 6 * Months, 1 * Years, 2 * Years,
                                                                         3 * Years, 5 * Years},
                       const std::vector<Real>& calibrationMoneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
                       const std::vector<Period>& calibrationVarianceTerms = {1 * Months, 3 * Months, 6 * Months,
                                                                              1 * Years, 2 * Years, 3 * Years},
                       // theta, kappa, sigma, rho, v0 (same order as in the Heston model, not the Heston process)
                       const std::vector<Real>& initialValues = {0.04, 1.0, 0.5, -0.5, 0.04},
                       const std::vector<bool>& fixedValues = {false, false, false, false, false},
                       Real relaxedFellerConstraint = 1.0, Size calibrationRestarts = 50, Real tolerance = 0.001,
                       const std::string& referenceCalibrationGrid = "", const bool dontCalibrate = false,
                       const Handle<YieldTermStructure>& baseCurve = {})
        : HestonModelBuilder(std::vector<Handle<YieldTermStructure>>{curve},
                             std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>>{process}, simulationDates,
                             addDates, timeStepsPerYear, calibrationExpiries, calibrationMoneyness,
                             calibrationVarianceTerms, initialValues, fixedValues, relaxedFellerConstraint,
                             calibrationRestarts, tolerance, referenceCalibrationGrid, dontCalibrate, baseCurve) {}

    std::vector<ext::shared_ptr<StochasticProcess>> getCalibratedProcesses() const override;

    AssetModelWrapper::ProcessType processType() const override;

protected:
    std::vector<std::vector<Real>> getCurveTimes() const override;
    std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const override;

private:
    class VarianceCalculator : public GeneralisedReplicatingVarianceSwapEngine {
    public:
        VarianceCalculator(const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process)
            : GeneralisedReplicatingVarianceSwapEngine(QuantLib::ext::shared_ptr<QuantLib::Index>(), process,
                                                       process->riskFreeRate()) {}

        Real futureVariance(const Date& maturity) const { return calculateFutureVariance(maturity); }
    };

    std::vector<Period> calibrationExpiries_;
    std::vector<Real> calibrationMoneyness_;
    std::vector<Period> calibrationVarianceTerms_;
    std::vector<Real> initialValues_;
    std::vector<bool> fixedValues_;
    Real relaxedFellerConstraint_;
    Size calibrationRestarts_;
    Real tolerance_;
    std::string referenceCalibrationGrid_;
    bool dontCalibrate_;
};

} // namespace data
} // namespace ore
