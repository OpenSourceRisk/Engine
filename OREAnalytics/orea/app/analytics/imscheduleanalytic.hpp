/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/imscheduleanalytic.hpp
    \brief ORE IM Schedule Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>
#include <orea/simm/imschedulecalculator.hpp>

namespace ore {
namespace analytics {


class IMScheduleAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "IM_SCHEDULE";

    IMScheduleAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override {}
};

class IMScheduleAnalytic : public Analytic {
public:
    IMScheduleAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                       const Crif& crif = Crif(),
                       const bool hasNettingSetDetails = false)
        : Analytic(std::make_unique<IMScheduleAnalyticImpl>(inputs), {"IM_SCHEDULE"}, inputs,
                   false, false, false, false),
          crif_(crif), hasNettingSetDetails_(hasNettingSetDetails) {}

    const QuantLib::ext::shared_ptr<IMScheduleCalculator>& imSchedule() const { return imSchedule_; }
    void setImSchedule(const QuantLib::ext::shared_ptr<IMScheduleCalculator>& imSchedule) { imSchedule_ = imSchedule; }

    //! Load CRIF from external source, override to generate CRIF from the input portfolio
    virtual void loadCrifRecords(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);

    const Crif& crif() const { return crif_; }
    bool hasNettingSetDetails() const { return hasNettingSetDetails_; }
    const std::map<SimmConfiguration::SimmSide, std::set<ore::data::NettingSetDetails>>& hasSEC() const {
        return hasSEC_;
    }
    const std::map<SimmConfiguration::SimmSide, std::set<ore::data::NettingSetDetails>>& hasCFTC() const {
        return hasCFTC_;
    }
 private:
    Crif crif_;
    bool hasNettingSetDetails_ = false;
    std::map<SimmConfiguration::SimmSide, std::set<ore::data::NettingSetDetails>> hasSEC_;
    std::map<SimmConfiguration::SimmSide, std::set<ore::data::NettingSetDetails>> hasCFTC_;
    QuantLib::ext::shared_ptr<IMScheduleCalculator> imSchedule_;
};

} // namespace analytics
} // namespace ore
