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

/*! \file ored/scripting/scriptparser.hpp
    \brief script parser
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/ast.hpp>

#include <ostream>

namespace ore {
namespace data {

struct ParserError {
    // always provided in case of an error
    std::string remainingInput;
    LocationInfo stoppedParsingAt;
    // only provided for a subset of possible errors
    std::string expectedWhat;
    LocationInfo expectedWhere;
    std::string scriptCurrentLine;
    std::string scriptContext;
    Size errorPos;
};

std::ostream& operator<<(std::ostream& out, const ParserError& error);

class ScriptParser {
public:
    ScriptParser(const std::string& script);
    // did the parsing succeed?
    bool success() const { return success_; }
    // null if not successful
    ASTNodePtr ast() const { return ast_; }
    // error info, if not successful
    const ParserError& error() const { return parserError_; }

private:
    bool success_;
    ASTNodePtr ast_;
    ParserError parserError_;
};

std::string printCodeContext(std::string script, const ASTNode* loc, bool compact = false);

} // namespace data
} // namespace ore
