/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/localvolmodelbuilder.hpp
    \brief builder for an array of local vol processes
    \ingroup utilities
*/

#pragma once

#include <orepscriptedtrade/ored/models/blackscholesmodelbuilderbase.hpp>

namespace oreplus {
namespace data {

using namespace ore::data;
using namespace QuantLib;

class LocalVolModelBuilder : public BlackScholesModelBuilderBase {
public:
    enum class Type { Dupire, AndreasenHuge };
    LocalVolModelBuilder(const std::vector<Handle<YieldTermStructure>>& curves,
                         const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                         const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                         const Size timeStepsPerYear, const Type lvType, const std::vector<Real>& calibrationMoneyness,
                         const bool dontCalibrate);
    LocalVolModelBuilder(const Handle<YieldTermStructure>& curve,
                         const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
                         const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                         const Size timeStepsPerYear, const Type lvType, const std::vector<Real>& calibrationMoneyness,
                         const bool dontCalibrate)
        : LocalVolModelBuilder(std::vector<Handle<YieldTermStructure>>{curve},
                               std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>{process}, simulationDates,
                               addDates, timeStepsPerYear, lvType, calibrationMoneyness, dontCalibrate) {}

protected:
    std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> getCalibratedProcesses() const override;
    std::vector<std::vector<Real>> getCurveTimes() const override;
    std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const override;

private:
    const Type lvType_;
    const std::vector<Real> calibrationMoneyness_;
    const bool dontCalibrate_;
};

} // namespace data
} // namespace oreplus
