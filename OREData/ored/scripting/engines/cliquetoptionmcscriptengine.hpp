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

#pragma once

#include <qle/instruments/cliquetoption.hpp>
#include <ored/scripting/models/model.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/scripting/ast.hpp>

#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {

class CliquetOptionMcScriptEngine : public QuantExt::CliquetOption::engine {
public:
    CliquetOptionMcScriptEngine(const std::string& underlying, const std::string& baseCcy,
                                const std::string& underlyingCcy,
                                const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& p,
                                const std::set<std::string>& tradeTypes, const Size samples, const Size regressionOrder,
                                bool interactive, bool scriptedLibraryOverride);
    void calculate() const override;

private:
    const std::string underlying_, baseCcy_, underlyingCcy_;
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> p_;
    const Size samples_, regressionOrder_;
    bool interactive_;
    ASTNodePtr ast_;
};

} // namespace data
} // namespace ore
