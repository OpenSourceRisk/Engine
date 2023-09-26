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

#include <ored/scripting/astresetter.hpp>

namespace ore {
namespace data {

namespace {
class ASTResetter : public AcyclicVisitor, public Visitor<ASTNode>, public Visitor<VariableNode> {
public:
    ASTResetter() {}

    void visit(ASTNode& n) override {
        for (auto const& c : n.args)
            if (c != nullptr)
                c->accept(*this);
    }

    void visit(VariableNode& n) override {
        n.isCached = n.isScalar = false;
        n.cachedScalar = nullptr;
        n.cachedVector = nullptr;
        visit(static_cast<ASTNode&>(n));
    }
};
} // namespace

void reset(const ASTNodePtr root) {
    ASTResetter r;
    root->accept(r);
}

} // namespace data
} // namespace ore
