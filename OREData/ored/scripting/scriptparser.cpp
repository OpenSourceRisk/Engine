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

#include <ored/scripting/grammar.hpp>
#include <ored/scripting/scriptparser.hpp>

#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string/replace.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, const ParserError& error) {
    if (!error.stoppedParsingAt.initialised) {
        out << "parsing succeeded";
    } else {
        out << "parsing stopped at " << to_string(error.stoppedParsingAt) << "\n";
        if (error.expectedWhere.initialised) {
            out << "expected " << error.expectedWhat << " in " << to_string(error.expectedWhere) << ":\n";
            out << error.scriptCurrentLine << "\n";
            out << std::string(std::max<Size>(error.expectedWhere.columnStart, 1) - 1, ' ') << "^--- here\n";
        } else {
            out << "remaining input is\n<<<<<<<<<<\n" << error.remainingInput << "\n>>>>>>>>>>\n";
        }
    }
    return out;
}

ScriptParser::ScriptParser(const std::string& script) {
    ScriptGrammarIterator first(script.begin()), iter = first, last(script.end());
    ScriptGrammar grammar(first);
    success_ = qi::phrase_parse(iter, last, grammar, boost::spirit::qi::space);
    if (!success_ || iter != last) {
        success_ = false;
        parserError_.stoppedParsingAt =
            LocationInfo(boost::spirit::get_line(iter), boost::spirit::get_column(first, iter),
                         boost::spirit::get_line(iter), boost::spirit::get_column(first, iter));
        parserError_.remainingInput = std::string(iter, last);
        if (grammar.hasError) {
            parserError_.expectedWhat = ore::data::to_string(grammar.errorWhat);
            parserError_.expectedWhere = LocationInfo(
                boost::spirit::get_line(grammar.errorPos), boost::spirit::get_column(first, grammar.errorPos),
                boost::spirit::get_line(grammar.errorPos), boost::spirit::get_column(first, grammar.errorPos));
            parserError_.scriptContext = std::string(grammar.errorBegin, grammar.errorEnd);
            parserError_.errorPos = std::distance(grammar.errorBegin, grammar.errorPos);
            auto currentLine = boost::spirit::get_current_line(grammar.errorBegin, grammar.errorPos, grammar.errorEnd);
            parserError_.scriptCurrentLine = std::string(currentLine.begin(), currentLine.end());
        }
    } else {
        QL_REQUIRE(grammar.evalStack.size() == 1,
                   "ScriptParser: unexpected eval stack size (" << grammar.evalStack.size() << "), should be 1");
        ast_ = grammar.evalStack.top();
        QL_REQUIRE(ast_, "ScriptParser: ast is null");
    }
}

std::string printCodeContext(std::string script, const ASTNode* loc, bool compact) {

    if (!loc)
        return "(script reference is not available)\n";

    // assume we have either newline or return, but not return alone (old mac csv format?)
    boost::replace_all(script, "\r", "");

    auto l = loc->locationInfo;
    if (l.lineEnd < l.lineStart || (l.lineEnd == l.lineStart && l.columnEnd <= l.columnStart))
        return "(script reference invalid: " + to_string(l) + ")\n";

    std::string res;
    // if compact is true, we remove leading/trailing symbols used for DLOGGERSTREAM for cleaner error message in log
    if (compact == false)
        res += "<<<<<<<<<<\n";

    Size pos = 0, currentLine = 1;
    for (Size line = l.lineStart; line <= l.lineEnd; ++line) {
        // look for the current line
        while (pos < script.size() && currentLine < line) {
            if (script[pos] == '\n')
                ++currentLine;
            ++pos;
        }
        if (pos == script.size())
            break;
        // look for the line end position in the current line
        Size posLineEnd = pos;
        while (posLineEnd < script.size() && script[posLineEnd] != '\n')
            ++posLineEnd;
        // add current line to the result
        std::string curr = std::string(std::next(script.begin(), pos), std::next(script.begin(), posLineEnd));
        if (compact) {
            boost::algorithm::trim(curr);
            res += curr + ' ';
        } else {
            res += curr + '\n';
        }
        // underline the relevant part of the current line
        Size columnStart = 1, columnEnd = posLineEnd - pos + 1;
        if (line == l.lineStart)
            columnStart = l.columnStart;
        if (line == l.lineEnd)
            columnEnd = l.columnEnd;
        if (columnEnd < columnStart)
            return "(script reference internal error: columnEnd (" + std::to_string(columnEnd) +
                   ") should be >= columnStart (" + std::to_string(columnStart) + "))";
        // adjust column start so that trailing spaces are not underlined
        while (columnStart < columnEnd && pos + columnStart < script.size() &&
               script[pos + std::max<Size>(columnStart, 1) - 1] == ' ')
            ++columnStart;
        if (compact == false)
            res +=
                std::string(std::max<Size>(columnStart, 1) - 1, ' ') + std::string(columnEnd - columnStart, '=') + '\n';
    }

    if (compact == false)
        res += ">>>>>>>>>>\n";
    return res;
}

} // namespace data
} // namespace ore
