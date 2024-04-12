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

/*! \file ored/model/blackscholesmodelbuilderbase.hpp
    \brief builder for an array of black scholes processes
    \ingroup utilities
*/

#pragma once

#include <ored/model/calibrationpointcache.hpp>

#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>

#include <qle/models/blackscholesmodelwrapper.hpp>

#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {

using namespace QuantExt;
using namespace QuantLib;

class BlackScholesModelBuilderBase : public ModelBuilder {
public:
    BlackScholesModelBuilderBase(const std::vector<Handle<YieldTermStructure>>& curves,
                                 const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                                 const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                                 const Size timeStepsPerYear);
    BlackScholesModelBuilderBase(const Handle<YieldTermStructure>& curve,
                                 const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                 const std::set<Date>& simulationDates, const std::set<Date>& addDates,
                                 const Size timeStepsPerYear);

    Handle<BlackScholesModelWrapper> model() const;

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

protected:
    // generic ctor, you should override setupDateAndTimes() if using this one
    BlackScholesModelBuilderBase(const Handle<YieldTermStructure>& curve,
                                 const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process);

    virtual void setupDatesAndTimes() const;

    virtual std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> getCalibratedProcesses() const = 0;
    virtual std::vector<std::vector<Real>> getCurveTimes() const = 0;
    virtual std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const = 0;


    void performCalculations() const override;
    bool calibrationPointsChanged(const bool updateCache) const;

    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> processes_;
    const std::set<Date> simulationDates_, addDates_;
    const Size timeStepsPerYear_;

    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid discretisationTimeGrid_;         // the (possibly refined) time grid for the simulation

    mutable RelinkableHandle<BlackScholesModelWrapper> model_;

    bool forceCalibration_ = false;
    QuantLib::ext::shared_ptr<MarketObserver> marketObserver_;

    std::vector<Handle<BlackVolTermStructure>> vols_;
    std::vector<Handle<YieldTermStructure>> allCurves_;
    mutable CalibrationPointCache cache_;
};

} // namespace data
} // namespace ore
