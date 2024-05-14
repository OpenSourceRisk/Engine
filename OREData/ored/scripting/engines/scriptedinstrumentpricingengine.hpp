/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/engines/scriptedinstrumentpricingengine.hpp
    \brief scripted instrument pricing engine
*/

#pragma once

#include <ored/scripting/models/model.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

#include <ored/configuration/conventions.hpp>

namespace ore {
namespace data {

class ScriptedInstrumentPricingEngine : public QuantExt::ScriptedInstrument::engine {
public:
    ScriptedInstrumentPricingEngine(const std::string& npv,
                                    const std::vector<std::pair<std::string, std::string>>& additionalResults,
                                    const QuantLib::ext::shared_ptr<Model>& model, const ASTNodePtr ast,
                                    const QuantLib::ext::shared_ptr<Context>& context, const std::string& script = "",
                                    const bool interactive = false, const bool amcEnabled = false,
                                    const std::set<std::string>& amcStickyCloseOutStates = {},
                                    const bool generateAdditionalResults = false,
                                    const bool includePastCashflows = false)
        : npv_(npv), additionalResults_(additionalResults), model_(model), ast_(ast), context_(context),
          script_(script), interactive_(interactive), amcEnabled_(amcEnabled),
          amcStickyCloseOutStates_(amcStickyCloseOutStates), generateAdditionalResults_(generateAdditionalResults),
          includePastCashflows_(includePastCashflows) {
        registerWith(model_);
    }

    bool lastCalculationWasValid() const { return lastCalculationWasValid_; }

private:
    void calculate() const override;
    Real addMcErrorEstimate(const std::string& label, const ValueType& v) const;

    // calculation state, true iff calculate() was called at least once and last call went without errors
    mutable bool lastCalculationWasValid_ = false;

    const std::string npv_;
    const std::vector<std::pair<std::string, std::string>> additionalResults_;
    const QuantLib::ext::shared_ptr<Model> model_;
    const ASTNodePtr ast_;
    const QuantLib::ext::shared_ptr<Context> context_;
    const std::string script_;
    const bool interactive_;
    const bool amcEnabled_;
    const std::set<std::string> amcStickyCloseOutStates_;
    const bool generateAdditionalResults_;
    const bool includePastCashflows_;
};

} // namespace data
} // namespace ore
