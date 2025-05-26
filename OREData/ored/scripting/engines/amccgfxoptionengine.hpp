/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file amccgfxoptionengine.hpp
    \brief AMC CG fx option engine
    \ingroup engines
*/

#pragma once

#include <ored/scripting/engines/amccgbaseengine.hpp>

#include <ql/instruments/vanillaoption.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>
#include <qle/instruments/vanillaforwardoption.hpp>

namespace ore {
namespace data {

class AmcCgFxOptionEngineBase : public AmcCgBaseEngine {
public:
    AmcCgFxOptionEngineBase(const std::string& domCcy, const std::string& forCcy,
                            const QuantLib::ext::shared_ptr<ModelCG>& modelCg, const std::vector<Date>& simulationDates)
        : AmcCgBaseEngine(modelCg, simulationDates), domCcy_(domCcy), forCcy_(forCcy) {}

    void buildComputationGraph(const bool stickyCloseOutDateRun,
                               const bool reevaluateExerciseInStickyCloseOutDateRun) const override;

    void setupLegs() const;
    void calculateFxOptionBase() const;

protected:
    std::string domCcy_, forCcy_;

    mutable QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff> payoff_;
    mutable Date payDate_;
};

class AmcCgFxOptionEngine : public AmcCgFxOptionEngineBase, public QuantLib::VanillaOption::engine {
public:
    AmcCgFxOptionEngine(const std::string& domCcy, const std::string& forCcy,
                        const QuantLib::ext::shared_ptr<ModelCG>& modelCg, const std::vector<Date>& simulationDates)
        : AmcCgFxOptionEngineBase(domCcy, forCcy, modelCg, simulationDates) {
        registerWith(modelCg_);
    }
    void calculate() const override;
};

class AmcCgFxEuropeanForwardOptionEngine : public AmcCgFxOptionEngineBase,
                                           public QuantExt::VanillaForwardOption::engine {
public:
    AmcCgFxEuropeanForwardOptionEngine(const std::string& domCcy, const std::string& forCcy,
                                       const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                       const std::vector<Date>& simulationDates)
        : AmcCgFxOptionEngineBase(domCcy, forCcy, modelCg, simulationDates) {
        registerWith(modelCg_);
    }
    void calculate() const override;
};

class AmcCgFxEuropeanCSOptionEngine : public AmcCgFxOptionEngineBase,
                                      public QuantExt::CashSettledEuropeanOption::engine {
public:
    AmcCgFxEuropeanCSOptionEngine(const std::string& domCcy, const std::string& forCcy,
                                  const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                  const std::vector<Date>& simulationDates)
        : AmcCgFxOptionEngineBase(domCcy, forCcy, modelCg, simulationDates) {
        registerWith(modelCg_);
    }
    void calculate() const override;
};

} // namespace data
} // namespace ore
