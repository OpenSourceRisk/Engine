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

/*! \file orea/app/analytics/simmanalytic.hpp
    \brief ORE SIMM Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>
#include <orea/simm/crif.hpp>

namespace ore {
namespace analytics {

class SimmAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SIMM";

    SimmAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};

class SimmAnalytic : public Analytic {
public:
    SimmAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs, const Crif& crif = Crif(),
                 const bool hasNettingSetDetails = false,
                 const bool determineWinningRegulations = true)
        : Analytic(std::make_unique<SimmAnalyticImpl>(inputs), {"SIMM"}, inputs, false, false, false, false),
          crif_(crif),
          hasNettingSetDetails_(hasNettingSetDetails),
          determineWinningRegulations_(determineWinningRegulations) {
        setWriteIntermediateReports(inputs->writeSimmIntermediateReports());
    }

    const Crif& crif() const { return crif_; }
    bool hasNettingSetDetails() { return hasNettingSetDetails_; }
    bool determineWinningRegulations() { return determineWinningRegulations_; }
    
    //! Load CRIF from external source, override to generate CRIF
    virtual void loadCrifRecords(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);

private:
    Crif crif_;
    bool hasNettingSetDetails_;
    bool determineWinningRegulations_;
};

} // namespace analytics
} // namespace ore
