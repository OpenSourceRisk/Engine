/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/engines/scriptedinstrumentpricingenginecg.hpp
    \brief scripted instrument pricing engine using a cg model
*/

#pragma once

#include <qle/ad/external_randomvariable_ops.hpp>

#include <ored/scripting/ast.hpp>
#include <ored/scripting/computationgraphbuilder.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/models/model.hpp>
#include <ored/scripting/models/modelcg.hpp>
#include <ored/scripting/paylog.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

#include <ored/configuration/conventions.hpp>

namespace ore {
namespace data {

class ScriptedInstrumentPricingEngineCG : public QuantExt::ScriptedInstrument::engine {
public:
    ScriptedInstrumentPricingEngineCG(const std::string& npv,
                                      const std::vector<std::pair<std::string, std::string>>& additionalResults,
                                      const QuantLib::ext::shared_ptr<ModelCG>& model, const ASTNodePtr ast,
                                      const QuantLib::ext::shared_ptr<Context>& context,
                                      const Model::McParams& mcParams, const std::string& script = "",
                                      const bool interactive = false, const bool generateAdditionalResults = false,
                                      const bool includePastCashflows = false, const bool useCachedSensis = false,
                                      const bool useExternalComputeFramework = false,
                                      const bool useDoublePrecisionForExternalCalculation = false);
    ~ScriptedInstrumentPricingEngineCG();

    bool lastCalculationWasValid() const { return lastCalculationWasValid_; }
    const std::string& npvName() const { return npv_; }

    void buildComputationGraph() const;

private:
    void calculate() const override;

    // calculation state, true iff calculate() was called at least once and last call went without errors

    mutable bool lastCalculationWasValid_ = false;

    // computation graph version built in model

    mutable std::size_t cgVersion_ = 0;

    // external compute framework calculation id and results

    mutable std::size_t externalCalculationId_ = 0;
    mutable std::vector<std::vector<double>> externalOutput_;
    mutable std::vector<double*> externalOutputPtr_;

    // variables populated during cg building

    mutable std::vector<ComputationGraphBuilder::PayLogEntry> payLogEntries_;
    mutable std::set<std::size_t> keepNodes_;
    mutable QuantLib::ext::shared_ptr<Context> workingContext_;

    // computation graph associated ops

    mutable std::vector<RandomVariableOpNodeRequirements> opNodeRequirements_;

    // if no external compute framework used
    mutable std::vector<RandomVariableOp> ops_;
    mutable std::vector<RandomVariableGrad> grads_;

    // for external compute framework
    mutable std::vector<ExternalRandomVariableOp> opsExternal_;
    mutable std::vector<ExternalRandomVariableGrad> gradsExternal_;

    // store base scenario model parameters to compute sensi-based NPVs

    mutable bool haveBaseValues_ = false;
    mutable double baseNpv_;
    mutable std::vector<std::pair<std::size_t, double>> baseModelParams_;
    mutable std::vector<double> sensis_;
    mutable std::map<std::string, boost::any> instrumentAdditionalResults_;

    // inputs

    const std::string npv_;
    const std::vector<std::pair<std::string, std::string>> additionalResults_;
    const QuantLib::ext::shared_ptr<ModelCG> model_;
    const ASTNodePtr ast_;
    const QuantLib::ext::shared_ptr<Context> context_;
    const Model::McParams mcParams_;
    const std::string script_;
    const bool interactive_;
    const bool generateAdditionalResults_;
    const bool includePastCashflows_;
    const bool useCachedSensis_;
    const bool useExternalComputeFramework_;
    const bool useDoublePrecisionForExternalCalculation_;
};

} // namespace data
} // namespace ore
