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

/*! \file ored/scripting/computationgraphbuilder.hpp
    \brief computation graph builder
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/modelcg.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/paylog.hpp>

#include <qle/math/randomvariable_ops.hpp>

namespace ore {
namespace data {

class ComputationGraphBuilder {
public:
    struct PayLogEntry {
        std::size_t value;
        std::size_t filter;
        QuantLib::Date obs;
        QuantLib::Date pay;
        std::string ccy;
        QuantLib::Size legNo;
        std::string cashflowType;
        QuantLib::Size slot;
    };

    ComputationGraphBuilder(ComputationGraph& g, const std::vector<std::string>& opLabels, const ASTNodePtr root,
                            const QuantLib::ext::shared_ptr<Context> context, const QuantLib::ext::shared_ptr<ModelCG> model = nullptr)
        : g_(g), opLabels_(opLabels), root_(root), context_(context), model_(model) {}
    void run(const bool generatePayLog, const bool includePastCashflows = false, const std::string& script = "",
             bool interactive = false);
    const std::set<std::size_t>& keepNodes() const { return keepNodes_; }
    const std::vector<PayLogEntry>& payLogEntries() const { return payLogEntries_; }

private:
    ComputationGraph& g_;

    const std::vector<std::string> opLabels_;
    const ASTNodePtr root_;
    const QuantLib::ext::shared_ptr<Context> context_;
    const QuantLib::ext::shared_ptr<ModelCG> model_;

    std::set<std::size_t> keepNodes_;
    std::vector<PayLogEntry> payLogEntries_;
};

} // namespace data
} // namespace ore
