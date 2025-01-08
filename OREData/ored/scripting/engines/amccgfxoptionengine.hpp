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
                            const QuantLib::ext::shared_ptr<ModelCG>& modelCg, const std::vector<Date>& simulationDates,
                            const std::vector<Date>& stickyCloseOutDates,
                            const bool recalibrateOnStickyCloseOutDates = false,
                            const bool reevaluateExerciseInStickyRun = false)
        : AmcCgBaseEngine(modelCg, simulationDates, stickyCloseOutDates, recalibrateOnStickyCloseOutDates,
                          reevaluateExerciseInStickyRun),
          domCcy_(domCcy), forCcy_(forCcy) {}

    void buildComputationGraph() const override;

private:
    std::string domCcy_, forCcy_;
};

class AmcCgFxOptionEngine : public AmcCgFxOptionEngineBase, public VanillaOption::engine {
public:
    AmcCgFxOptionEngine(const std::string& domCcy, const std::string& forCcy,
                        const QuantLib::ext::shared_ptr<ModelCG>& modelCg, const std::vector<Date>& simulationDates,
                        const std::vector<Date>& stickyCloseOutDates,
                        const bool recalibrateOnStickyCloseOutDates = false,
                        const bool reevaluateExerciseInStickyRun = false)
        : AmcCgFxOptionEngineBase(domCcy, forCcy, modelCg, simulationDates, stickyCloseOutDates,
                                  recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun) {
        registerWith(modelCg_);
    }
    void calculate() const override;
};

class AmcCgFxEuropeanForwardOptionEngine : public AmcCgFxOptionEngineBase,
                                           public QuantExt::VanillaForwardOption::engine {
public:
    AmcCgFxEuropeanForwardOptionEngine(const std::string& domCcy, const std::string& forCcy,
                                       const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                       const std::vector<Date>& simulationDates,
                                       const std::vector<Date>& stickyCloseOutDates,
                                       const bool recalibrateOnStickyCloseOutDates = false,
                                       const bool reevaluateExerciseInStickyRun = false)
        : AmcCgFxOptionEngineBase(domCcy, forCcy, modelCg, simulationDates, stickyCloseOutDates,
                                  recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun) {
        registerWith(modelCg_);
    }
    void calculate() const override;
};

class AmcCgFxEuropeanCSOptionEngine : public AmcCgFxOptionEngineBase,
                                      public QuantExt::CashSettledEuropeanOption::engine {
public:
    AmcCgFxEuropeanCSOptionEngine(const std::string& domCcy, const std::string& forCcy,
                                  const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                  const std::vector<Date>& simulationDates,
                                  const std::vector<Date>& stickyCloseOutDates,
                                  const bool recalibrateOnStickyCloseOutDates = false,
                                  const bool reevaluateExerciseInStickyRun = false)
        : AmcCgFxOptionEngineBase(domCcy, forCcy, modelCg, simulationDates, stickyCloseOutDates,
                                  recalibrateOnStickyCloseOutDates, reevaluateExerciseInStickyRun) {
        registerWith(modelCg_);
    }
    void calculate() const override;
};

} // namespace data
} // namespace ore
