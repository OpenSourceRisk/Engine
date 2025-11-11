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

/*! \file models/blackscholesmodelwrapper.hpp
    \brief wrapper around a vector of BS processes
    \ingroup utilities
*/

#pragma once

#include <ql/processes/blackscholesprocess.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

using namespace QuantLib;

/* This class acts as an intermediate class between the model builder and the script model. The motivation to have a
   builder and this wrapper at all is to filter notifications from the vol surfaces and curves so that a recalculations
   only happens when relevant market data has changed */
class AssetModelWrapper : public Observer, public Observable {
public:
    enum class ProcessType { None, BlackScholes, LocalVol, Heston };
    AssetModelWrapper();
    AssetModelWrapper(const ProcessType processType,
                      const std::vector<QuantLib::ext::shared_ptr<StochasticProcess>>& processes,
                      const std::set<Date>& effectiveSimulationDates, const TimeGrid& discretisationTimeGrid);

    const std::vector<QuantLib::ext::shared_ptr<StochasticProcess>>& processes() const;
    const std::set<Date>& effectiveSimulationDates() const;
    const TimeGrid& discretisationTimeGrid() const;

    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>&
    generalizedBlackScholesProcesses() const;
    const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& hestonProcesses() const;

    ProcessType processType() const;

private:
    void update() override;
    ProcessType processType_ = ProcessType::None;
    std::vector<QuantLib::ext::shared_ptr<StochasticProcess>> processes_;
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> generalizedBlackScholesProcesses_;
    std::vector<QuantLib::ext::shared_ptr<HestonProcess>> hestonProcesses_;
    std::set<Date> effectiveSimulationDates_;
    TimeGrid discretisationTimeGrid_;
};

} // namespace QuantExt
