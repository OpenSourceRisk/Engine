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

/*! \file ored/scripting/scriptengine.hpp
    \brief scriptengine
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/model.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/paylog.hpp>

#include <ored/configuration/conventions.hpp>

namespace ore {
namespace data {

class ScriptEngine {
public:
    ScriptEngine(const ASTNodePtr root, const QuantLib::ext::shared_ptr<Context> context,
                 const QuantLib::ext::shared_ptr<Model> model = nullptr)
        : root_(root), context_(context), model_(model) {}
    void run(const std::string& script = "", bool interactive = false, QuantLib::ext::shared_ptr<PayLog> paylog = nullptr,
             bool includePastCashflows = false);

private:
    const ASTNodePtr root_;
    const QuantLib::ext::shared_ptr<Context> context_;
    const QuantLib::ext::shared_ptr<Model> model_;
};

} // namespace data
} // namespace ore
