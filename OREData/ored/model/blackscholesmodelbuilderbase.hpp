/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/blackscholesmodelbuilderbase.hpp
    \brief builder for an array of black scholes processes
    \ingroup utilities
*/

#pragma once

#include <orepscriptedtrade/ored/models/blackscholesmodelwrapper.hpp>
#include <orepscriptedtrade/ored/models/calibrationpointcache.hpp>

#include <ored/model/marketobserver.hpp>
#include <ored/model/modelbuilder.hpp>

#include <ql/processes/blackscholesprocess.hpp>

namespace oreplus {
namespace data {

using namespace ore::data;
using namespace QuantLib;

class BlackScholesModelBuilderBase : public ModelBuilder {
public:
    BlackScholesModelBuilderBase(const std::vector<Handle<YieldTermStructure>>& curves,
                                 const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                                 const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                                 const Size timeStepsPerYear);
    BlackScholesModelBuilderBase(const Handle<YieldTermStructure>& curve,
                                 const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                 const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                                 const Size timeStepsPerYear);

    Handle<BlackScholesModelWrapper> model() const;

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

protected:
    virtual std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> getCalibratedProcesses() const = 0;
    virtual std::vector<std::vector<Real>> getCurveTimes() const = 0;
    virtual std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const = 0;

    void performCalculations() const override;
    bool calibrationPointsChanged(const bool updateCache) const;

    void setupDatesAndTimes() const;

    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> processes_;
    const std::set<Date> simulationDates_, addDates_;
    const Size timeStepsPerYear_;

    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid discretisationTimeGrid_;         // the (possibly refined) time grid for the simulation

    mutable RelinkableHandle<BlackScholesModelWrapper> model_;

    bool forceCalibration_ = false;
    boost::shared_ptr<MarketObserver> marketObserver_;

    std::vector<Handle<BlackVolTermStructure>> vols_;
    std::vector<Handle<YieldTermStructure>> allCurves_;
    mutable CalibrationPointCache cache_;
};

} // namespace data
} // namespace oreplus
