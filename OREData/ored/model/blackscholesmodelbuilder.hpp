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

/*! \file ored/model/blackscholesmodelbuilder.hpp
    \brief builder for an array of black scholes processes
    \ingroup utilities
*/

#pragma once

#include <ored/model/blackscholesmodelbuilderbase.hpp>

namespace ore {
namespace data {

using namespace ore::data;
using namespace QuantLib;

class BlackScholesModelBuilder : public BlackScholesModelBuilderBase {
public:
    BlackScholesModelBuilder(const std::vector<Handle<YieldTermStructure>>& curves,
                             const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                             const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                             const Size timeStepsPerYear = 0, const std::string& calibration = "ATM",
                             const std::vector<std::vector<Real>>& calibrationStrikes = {});
    BlackScholesModelBuilder(const Handle<YieldTermStructure>& curve,
                             const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                             const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                             const Size timeStepsPerYear = 0, const std::string& calibration = "ATM",
                             const std::vector<Real>& calibrationStrikes = {});

    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> getCalibratedProcesses() const override;

protected:
    std::vector<std::vector<Real>> getCurveTimes() const override;
    std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const override;

private:
    const std::string calibration_;
    const std::vector<std::vector<Real>> calibrationStrikes_;
};

} // namespace data
} // namespace ore
