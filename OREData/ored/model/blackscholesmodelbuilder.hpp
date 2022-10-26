/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/blackscholesmodelbuilder.hpp
    \brief builder for an array of black scholes processes
    \ingroup utilities
*/

#pragma once

#include <orepscriptedtrade/ored/models/blackscholesmodelbuilderbase.hpp>

namespace oreplus {
namespace data {

using namespace ore::data;
using namespace QuantLib;

class BlackScholesModelBuilder : public BlackScholesModelBuilderBase {
public:
    BlackScholesModelBuilder(const std::vector<Handle<YieldTermStructure>>& curves,
                             const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                             const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                             const Size timeStepsPerYear = 0, const std::string& calibration = "ATM",
                             const std::vector<std::vector<Real>>& calibrationStrikes = {});
    BlackScholesModelBuilder(const Handle<YieldTermStructure>& curve,
                             const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
                             const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                             const Size timeStepsPerYear = 0, const std::string& calibration = "ATM",
                             const std::vector<Real>& calibrationStrikes = {});

protected:
    std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> getCalibratedProcesses() const override;
    std::vector<std::vector<Real>> getCurveTimes() const override;
    std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const override;

private:
    const std::string calibration_;
    const std::vector<std::vector<Real>> calibrationStrikes_;
};

} // namespace data
} // namespace oreplus
