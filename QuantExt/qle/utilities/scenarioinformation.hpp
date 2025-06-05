/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file scenarioinformation.hpp
    \brief global repository for scenario information
*/

#pragma once

#include <qle/termstructures/scenario.hpp>

#include <ql/patterns/singleton.hpp>

#include <map>

namespace QuantExt {
using namespace QuantLib;

class ScenarioInformationSetter;

//! global repository for scenario information
class ScenarioInformation : public Singleton<ScenarioInformation> {
    friend class Singleton<ScenarioInformation>;

private:
    ScenarioInformation() = default;
    void setParentScenarioAbsolute(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& scenario) {
        parentScenarioAbsolute_ = scenario;
    }
    void setChildScenarioAbsolute(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& scenario) {
        childScenarioAbsolute_ = scenario;
    }
    friend class ScenarioInformationSetter;

public:
    const QuantLib::ext::shared_ptr<QuantExt::Scenario>& getParentScenarioAbsolute() { return parentScenarioAbsolute_; }
    const QuantLib::ext::shared_ptr<QuantExt::Scenario>& getChildScenarioAbsolute() { return childScenarioAbsolute_; }

private:
    QuantLib::ext::shared_ptr<QuantExt::Scenario> childScenarioAbsolute_;
    QuantLib::ext::shared_ptr<QuantExt::Scenario> parentScenarioAbsolute_;
};

//! scenario information setter, ensuring that the info is cleared when the setter goes out of scope
class ScenarioInformationSetter {
public:
    void setParentScenario(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& parentScenario) {
        ScenarioInformation::instance().setParentScenarioAbsolute(parentScenario);
    }
    void setChildScenario(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& childScenario) {
        ScenarioInformation::instance().setChildScenarioAbsolute(childScenario);
    }
    ~ScenarioInformationSetter() {
        ScenarioInformation::instance().setParentScenarioAbsolute(nullptr);
        ScenarioInformation::instance().setChildScenarioAbsolute(nullptr);
    }
};

} // namespace QuantExt
