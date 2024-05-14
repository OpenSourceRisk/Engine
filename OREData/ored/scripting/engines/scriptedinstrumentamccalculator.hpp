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

/*! \file ored/scripting/engines/scriptedinstrumentamccalculator.hpp
    \brief amc calculator for scripted trades
*/

#pragma once

#include <ored/scripting/models/amcmodel.hpp>
#include <ored/scripting/models/model.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

#include <qle/pricingengines/amccalculator.hpp>

#include <ored/configuration/conventions.hpp>

namespace ore {
namespace data {

class ScriptedInstrumentAmcCalculator : public QuantExt::AmcCalculator {
public:
    ScriptedInstrumentAmcCalculator(const std::string& npv, const QuantLib::ext::shared_ptr<Model>& model, const ASTNodePtr ast,
                                    const QuantLib::ext::shared_ptr<Context>& context, const std::string& script = "",
                                    const bool interactive = false,
                                    const std::set<std::string>& stickyCloseOutStates = {})
        : npv_(npv), model_(model), ast_(ast), context_(context), script_(script), interactive_(interactive),
          stickyCloseOutStates_(stickyCloseOutStates) {}

    QuantLib::Currency npvCurrency() override;

    std::vector<QuantExt::RandomVariable> simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                                                       std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                                                       const std::vector<size_t>& relevantPathIndex,
                                                       const std::vector<size_t>& relevantTimeIndex) override;

private:
    const std::string npv_;
    const QuantLib::ext::shared_ptr<Model> model_;
    const ASTNodePtr ast_;
    const QuantLib::ext::shared_ptr<Context> context_;
    const std::string script_;
    const bool interactive_;
    const std::set<std::string> stickyCloseOutStates_;
    //
    std::map<std::string, ValueType> stickyCloseOutRunScalars_;
    std::map<std::string, std::vector<ValueType>> stickyCloseOutRunArrays_;
};

} // namespace data
} // namespace ore
