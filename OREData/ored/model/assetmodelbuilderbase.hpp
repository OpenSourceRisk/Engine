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

/*! \file ored/model/assetmodelbuilderbase.hpp
    \brief builder for an array of processes
    \ingroup utilities
*/

#pragma once

#include <ored/model/calibrationpointcache.hpp>

#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>

#include <qle/models/assetmodelwrapper.hpp>

#include <ql/processes/blackscholesprocess.hpp>
#include <ql/any.hpp>

namespace ore {
namespace data {

using namespace QuantExt;
using namespace QuantLib;

class AssetModelBuilderBase : public ModelBuilder {
public:
    AssetModelBuilderBase(
        const std::vector<Handle<YieldTermStructure>>& curves,
        const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
        const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
        const Handle<YieldTermStructure>& baseCurve = {}, const bool observeContinuum = false,
        const std::function<std::set<Real>(const TimeGrid&)>& curveTimes = {},
        const std::function<std::vector<std::set<std::pair<Real, Real>>>(const TimeGrid&)>& volTimesStrikes = {});
    AssetModelBuilderBase(
        const Handle<YieldTermStructure>& curve,
        const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process, const std::set<Date>& simulationDates,
        const std::set<Date>& addDates, const Size timeStepsPerYear, const Handle<YieldTermStructure>& baseCurve = {},
        const bool observeContinuum = false, const std::function<std::set<Real>(const TimeGrid&)>& curveTimes = {},
        const std::function<std::vector<std::set<std::pair<Real, Real>>>(const TimeGrid&)>& volTimesStrikes = {});

    Handle<AssetModelWrapper> model() const;
    const std::set<Date>& simulationDates() const { return simulationDates_; }

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void newCalcWithoutRecalibration() const override;
    //@}

protected:
    // generic ctor, you should override setupDateAndTimes() if using this one
    AssetModelBuilderBase(const Handle<YieldTermStructure>& curve,
                          const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process);

    virtual AssetModelWrapper::ProcessType processType() const = 0;
    virtual std::vector<QuantLib::ext::shared_ptr<StochasticProcess>> getCalibratedProcesses() const = 0;

    virtual void setupDatesAndTimes() const;

    void performCalculations() const override;
    bool calibrationPointsChanged(const bool updateCache) const;
    void buildCacheData(const std::set<Real>& curveTimes,
                        const std::vector<std::set<std::pair<Real, Real>>>& volTimesStrikes,
                        std::vector<std::vector<Real>>& curveData, std::vector<std::vector<Real>>& volData) const;

    std::vector<Handle<YieldTermStructure>> curves_;
    Handle<YieldTermStructure> baseCurve_;
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> processes_;
    std::set<Date> simulationDates_, addDates_;
    Size timeStepsPerYear_;
    bool observeContinuum_;

    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid discretisationTimeGrid_;         // the (possibly refined) time grid for the simulation

    mutable RelinkableHandle<AssetModelWrapper> model_;

    bool forceCalibration_ = false;
    QuantLib::ext::shared_ptr<MarketObserver> marketObserver_;

    std::vector<Handle<BlackVolTermStructure>> vols_;
    std::vector<Handle<YieldTermStructure>> allCurves_;
    mutable CalibrationPointCache cache_, cacheModel_;
    mutable std::function<std::set<Real>(const TimeGrid&)> curveTimesBase_;
    mutable std::set<Real> curveTimes_;
    mutable std::function<std::vector<std::set<std::pair<Real, Real>>>(const TimeGrid&)> volTimesStrikesBase_;
    mutable std::vector<std::set<std::pair<Real, Real>>> volTimesStrikes_;
    mutable Date referenceDate_;

    mutable std::vector<AssetModelCalibrationResults> calibrationResults_;
};

} // namespace data
} // namespace ore
