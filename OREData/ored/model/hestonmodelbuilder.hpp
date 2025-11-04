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

#include <ored/model/blackscholesmodelbuilderbase.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

class HestonModelBuilder : public BlackScholesModelBuilderBase {
public:
    HesotnModelBuilder(const std::vector<Handle<YieldTermStructure>>& curves,
                         const std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                         const std::set<Date>& simulationDates = {}, const std::set<Date>& addDates = {},
                         const Size timeStepsPerYear = 1, const Type lvType = Type::Dupire,
                         const std::vector<Real>& calibrationMoneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
                         const std::string& referenceCalibrationGrid = "", const bool dontCalibrate = false,
                         const Handle<YieldTermStructure>& baseCurve = {});
    HestonModelBuilder(const Handle<YieldTermStructure>& curve,
                         const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                         const std::set<Date>& simulationDates = {}, const std::set<Date>& addDates = {},
                         const Size timeStepsPerYear = 1, const Type lvType = Type::Dupire,
                         const std::vector<Real>& calibrationMoneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
                         const std::string& referenceCalibrationGrid = "", const bool dontCalibrate = false,
                         const Handle<YieldTermStructure>& baseCurve = {})
        : HestonModelBuilder(std::vector<Handle<YieldTermStructure>>{curve},
                               std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>>{process}, simulationDates,
                               addDates, timeStepsPerYear, lvType, calibrationMoneyness, referenceCalibrationGrid,
                               dontCalibrate, baseCurve) {}

    std::vector<ext::shared_ptr<StochasticProcess>> getCalibratedProcesses() const override;

protected:
    std::vector<std::vector<Real>> getCurveTimes() const override;
    std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const override;

private:
    std::vector<Real> calibrationMoneyness_;
    std::string referenceCalibrationGrid_;
    bool dontCalibrate_;
};

} // namespace data
} // namespace ore
