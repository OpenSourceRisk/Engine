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

#include <ored/marketdata/clonedloader.hpp>
#include <orea/scenario/cvascenario.hpp>

#pragma once

namespace ore {
namespace analytics {

class CvaScenarioLoader : public ore::data::ClonedLoader {

public:
    // Default constructor
    CvaScenarioLoader(const QuantLib::Date& loaderDate, const QuantLib::ext::shared_ptr<ore::data::Loader>& inLoader);

    //! set the base scenario
    void setBaseScenario(const QuantLib::ext::shared_ptr<ore::analytics::CvaScenario>& baseScenario);

    //! get the base scenario
    const QuantLib::ext::shared_ptr<ore::analytics::CvaScenario>& baseScenario() { return baseScenario_; }

    //! reset to base scenario
    void reset();

    //! apply the given scenario
    void applyScenario(const QuantLib::ext::shared_ptr<ore::analytics::CvaScenario>& scenario);

    //! update underlying marketdatum
    void updateMarketDatum(std::string key, QuantLib::Real value);

private:    
    QuantLib::ext::shared_ptr<ore::analytics::CvaScenario> baseScenario_;
    std::set<std::string> alteredKeys_;
};

} // namespace data
} // namespace ore
