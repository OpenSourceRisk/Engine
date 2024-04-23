/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file simplescenariofactory.hpp
    \brief factory classes for simple scenarios
    \ingroup scenario
*/

#pragma once

#include <boost/make_shared.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/simplescenario.hpp>

namespace ore {
namespace analytics {

//! Factory class for building simple scenario objects
/*! \ingroup scenario
 */
class SimpleScenarioFactory : public ScenarioFactory {
public:
    explicit SimpleScenarioFactory(const bool useCommonSharedDataBlock = false)
        : useCommonSharedDataBlock_(useCommonSharedDataBlock) {}
    const QuantLib::ext::shared_ptr<Scenario> buildScenario(Date asof, const std::string& label = "",
                                                            Real numeraire = 0.0) const override {
        auto tmp = QuantLib::ext::make_shared<SimpleScenario>(asof, label, numeraire,
                                                              useCommonSharedDataBlock_ ? sharedData_ : nullptr);
        if (useCommonSharedDataBlock_ && sharedData_ == nullptr)
            sharedData_ = tmp->sharedData();
        return tmp;
    }

private:
    bool useCommonSharedDataBlock_ = false;
    mutable QuantLib::ext::shared_ptr<SimpleScenario::SharedData> sharedData_;
};

} // namespace analytics
} // namespace ore
