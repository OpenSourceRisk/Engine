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

#include <ored/scripting/engines/scriptedinstrumentamccalculator.hpp>
#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/utilities.hpp>

#include <qle/math/randomvariable.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

QuantLib::Currency ScriptedInstrumentAmcCalculator::npvCurrency() { return parseCurrency(model_->baseCcy()); }

std::vector<QuantExt::RandomVariable> ScriptedInstrumentAmcCalculator::simulatePath(
    const std::vector<QuantLib::Real>& pathTimes, std::vector<std::vector<QuantExt::RandomVariable>>& paths,
    const std::vector<size_t>& relevantPathIndex, const std::vector<size_t>& relevantTimeIndex) {

    QL_REQUIRE(relevantPathIndex.size() == relevantTimeIndex.size(),
               "ScriptedInstrumentAmcCalculator::simulatePath: Mismatch between relevantPathIndex size and "
               "relevantTimeIndex size, internal error");

    bool stickyCloseOutRun = false;
    for (size_t i = 0; i < relevantPathIndex.size(); ++i) {
        if (relevantPathIndex[i] != relevantTimeIndex[i]) {
            stickyCloseOutRun = true;
            break;
        }
    }
    // inject the global paths into our local model, notice that this will change the size of the model

    auto amcModel = QuantLib::ext::dynamic_pointer_cast<AmcModel>(model_);
    QL_REQUIRE(amcModel, "expected an AmcModel");
    amcModel->injectPaths(&pathTimes, &paths, &relevantPathIndex, &relevantTimeIndex);

    // the rest is similar to what is done in the ScriptedInstrumentPricingEngine:

    // set up copy of initial context to run the script engine on

    auto workingContext = QuantLib::ext::make_shared<Context>(*context_);

    // amend context to new model size
    amendContextVariablesSizes(workingContext, model_->size());

    // make sure we reset the injected path data after the calculation
    struct InjectedPathReleaser {
        ~InjectedPathReleaser() {
            QuantLib::ext::dynamic_pointer_cast<AmcModel>(model)->injectPaths(nullptr, nullptr, nullptr, nullptr);
        }
        QuantLib::ext::shared_ptr<Model> model;
    };
    InjectedPathReleaser injectedPathReleaser{model_};

    // set TODAY in the context

    checkDuplicateName(workingContext, "TODAY");
    Date referenceDate = model_->referenceDate();
    workingContext->scalars["TODAY"] = EventVec{model_->size(), referenceDate};
    workingContext->constants.insert("TODAY");

    // set context variables that should be static for sticky close-out runs

    if (stickyCloseOutRun) {
        for (auto const& s : stickyCloseOutRunScalars_) {
            workingContext->scalars[s.first] = s.second;
            workingContext->constants.insert(s.first);
            workingContext->ignoreAssignments.insert(s.first);
            DLOG("add scalar " << s.first << " to context from previous run, since we have a sticky close-out run now");
        }
        for (auto const& s : stickyCloseOutRunArrays_) {
            workingContext->arrays[s.first] = s.second;
            workingContext->constants.insert(s.first);
            workingContext->ignoreAssignments.insert(s.first);
            DLOG("add array " << s.first << " to context from previous run, since we have a sticky close-out run now");
        }
    }

    // set up script engine and run it

    ScriptEngine engine(ast_, workingContext, model_);
    engine.run(script_, interactive_, nullptr);

    // extract AMC Exposure result and return them

    Size resultSize = relevantTimeIndex.size();
    std::vector<QuantExt::RandomVariable> result(resultSize + 1);

    // the T0 npv is the first component of the result

    auto npv = workingContext->scalars.find(npv_);
    QL_REQUIRE(npv != workingContext->scalars.end(),
               "did not find npv result variable '" << npv_ << "' as scalar in context");
    QL_REQUIRE(npv->second.which() == ValueTypeWhich::Number,
               "result variable '" << npv_ << "' must be of type NUMBER, got " << npv->second.which());
    result[0] = expectation(QuantLib::ext::get<RandomVariable>(npv->second));

    // the other components are given as the additional result _AMC_NPV

    auto s = workingContext->arrays.find("_AMC_NPV");
    QL_REQUIRE(s != workingContext->arrays.end(), "did not find amc exposure result _AMC_NPV");
    QL_REQUIRE(s->second.size() == resultSize,
               "result _AMC_NPV has size "
                   << s->second.size() << " which is inconsistent with number of (positive, and relevant) path times "
                   << resultSize);

    for (Size i = 0; i < resultSize; ++i) {
        QL_REQUIRE(s->second[i].which() == ValueTypeWhich::Number,
                   "component #" << i << " in _AMC_NPV has wrong type, expected Number");
        result[i + 1] = QuantLib::ext::get<RandomVariable>(s->second[i]);
    }

    // extract variables that should be static in subsequent sticky close-out runs

    for (auto const& n : stickyCloseOutStates_) {
        auto s = workingContext->scalars.find(n);
        if (s != workingContext->scalars.end()) {
            stickyCloseOutRunScalars_[n] = s->second;
            //DLOG("store variable " << n << " for subsequent sticky close-out run");
        }
        auto v = workingContext->arrays.find(n);
        if (v != workingContext->arrays.end()) {
            stickyCloseOutRunArrays_[n] = v->second;
            //DLOG("store array " << n << " for subsequent sticky close-out run");
        }
    }

    return result;
}

} // namespace data
} // namespace ore
