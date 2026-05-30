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

/*! \file ored/scripting/engines/amccgpricingengine.hpp
    \brief pricing engine suitable to be used in amc-cg framework
*/

#pragma once

#include <ored/scripting/models/modelcg.hpp>

#include <qle/ad/computationgraph.hpp>

#include <set>
#include <string>
#include <variant>

namespace ore {
namespace data {

/* The plain trades produce one componentPathValue per "regressor group", where the regressor group describes
   the full set of regressors needed to calculate conditional expectations plus a possibly required change
   of measure to perform the regression. The regression itself is performed in the exposure engine. */
struct SimpleTradeExposure {
    double multiplier = 1.0;
    struct RegressorGroup {
        std::set<std::size_t> regressorsLocalBaseCcy;
        std::set<std::size_t> regressorsBaseCcy;
        std::size_t pathValue; // in local base ccy
        std::string localBaseCurrency;
    };
    std::vector<RegressorGroup> groups;
};

/* The complex trades produce one or more component path values which are combined to the target conditional
   expectation, which is already a conditional expectation, i.e. no regression is performed outside the trade
   pricing engine.

   The component path values of individual nodes have to be placed in sequence into the computation graph, usually
   by creating dummy nodes with op = none to ensure this property, and the sub-computationgraph from the first
   node after the last component path value up to and including the targetConditionalExpectationDerivatives is
   used to recombine AAD derivative values for dynamic sensitivity calculation. For this, the component path values
   are populated with path derivatives and then the sub-computation graph is evaluated to get the resulting total
   derivative in the node targetConditionalExpectationDerivatives as a conditional expectation. The
   targetConditionalExpectation is then computed after the node targetConditionalExpectationDerivatives. If no
   distinction between targetConditionalExpectation and targetConditionalExpectationDerivatives is required, the
   latter node can be set to the former node.

   For the recombination run, all nodes on which the relevant sub-computationgraph depends will be populated from
   the forward evaluation values.*/
struct ComplexTradeExposure {
    double multiplier = 1.0;
    std::vector<std::size_t> componentPathValues;
    std::string localBaseCurrency;
    std::size_t targetConditionalExpectation = QuantExt::ComputationGraph::nan;
    std::size_t targetConditionalExpectationDerivative = QuantExt::ComputationGraph::nan;
    std::vector<std::size_t> targetConditionalExpDerivativeNpvNodes; // only for regression detail report
};

using TradeExposure = std::variant<std::monostate, SimpleTradeExposure, ComplexTradeExposure>;

struct TradeExposureMetaInfo {
    bool hasVega = false;
    std::set<ModelCG::ModelParameter> relevantModelParameters;
};

class AmcCgPricingEngine {
public:
    virtual ~AmcCgPricingEngine() {}
    // whether the trade will generate a ComplexTradeExposure
    virtual bool isComplexTrade() const = 0;
    // the relevant currency sets of the trade
    virtual std::set<std::set<std::string>> relevantCurrencySets() const = 0;
    // baseCurrencySuggestions should map all relevant currency sets to an admissable base ccy of the model
    // if not given, or the return value is empty, the engine will pick one of the models admissable base ccys
    virtual void
    buildComputationGraph(const bool stickyCloseOutDateRun = false, std::vector<TradeExposure>* tradeExposure = nullptr,
                          TradeExposureMetaInfo* tradeExposureMetaInfo = nullptr,
                          std::function<std::string(std::set<std::string>)> baseCurrencySuggestions = {}) const = 0;
};

} // namespace data
} // namespace ore
