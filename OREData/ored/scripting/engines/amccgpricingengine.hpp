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

namespace ore {
namespace data {

struct TradeExposure {
    /* There are basically two types of trades

       - plain trades
       - individual trades

       The former produce a single component path value and use a standard set of regressors. The regression is run in
       the exposure engine, i.e. outside the trade pricing engine, over all plain trades. This regression uses the union
       of all regressors of plain trades and the regressor set is grouped by the individual trade regressor sets.

       The individual trades produce one or more component path values which are combined to the target conditional
       expectation, which is already a conditional expectation, i.e. no regression is performed outside the trade
       pricing engine. The computation graph from the component path values, technically the first node after that where
       the combination of the components start, to the target conditional expectation is replayed within the exposure
       engine for aad calculations and therefore all source nodes outside this range must be known, i.e. the set of
       regressors plus additional required nodes (including constants).

       The following fields are filled for the two trade types:

                                        plain trades             individual trades
       componentPathValues                   1 entry                     n entries
       targetConditionalExpectation               no                           yes
       startNodeRecombine                         no                           yes
       regressors                                yes                           yes
       additionalRequiredNodes                    no                           yes        */

       std::vector<std::size_t> componentPathValues;
       std::size_t targetConditionalExpectation = QuantExt::ComputationGraph::nan;
       std::size_t startNodeRecombine = QuantExt::ComputationGraph::nan;
       std::set<std::size_t> regressors;
       std::set<std::size_t> additionalRequiredNodes;
};

void scale(QuantExt::ComputationGraph& g, TradeExposure& t, double multiplier);

struct TradeExposureMetaInfo {
    bool hasVega = false;
    std::set<std::string> relevantCurrencies;
    std::set<ModelCG::ModelParameter> relevantModelParameters;
};

class AmcCgPricingEngine {
public:
    virtual ~AmcCgPricingEngine() {}
    virtual void buildComputationGraph(const bool stickyCloseOutDateRun = false,
                                       const bool reevaluateExerciseInStickyCloseOutDateRun = false,
                                       std::vector<TradeExposure>* tradeExposure = nullptr,
                                       TradeExposureMetaInfo* tradeExposureMetaInfo = nullptr) const = 0;
};

} // namespace data
} // namespace ore
