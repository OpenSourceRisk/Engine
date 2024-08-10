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

#include <qle/math/gpucodegenerator.hpp>
#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string/join.hpp>

#include <numeric>
#include <set>

namespace QuantExt {

void GpuCodeGenerator::initialize(const std::size_t nInputVars, const std::vector<bool> inputVarIsScalar,
                                  const std::size_t nVariates, const std::size_t modelSize,
                                  const bool doublePrecision) {

    nInputVars_ = nInputVars;
    inputVarIsScalar_ = inputVarIsScalar;
    nVariates_ = nVariates;
    modelSize_ = modelSize;
    doublePrecision_ = doublePrecision;

    fpTypeStr_ = doublePrecision_ ? "double" : "float";
    fpEpsStr_ = doublePrecision_ ? "0x1.0p-52" : "0x1.0p-23f";
    fpSuffix_ = doublePrecision_ ? std::string() : "f";

    nLocalVars_ = 0;
    ops_.clear();
    freedVariables_.clear();
    currentKernelNo_ = 0;
    kernelBreakLines_.clear();
    conditionalExpectationVars_.clear();
    conditionalExpectationVars_.push_back({});
    outputVars_.clear();
    outputVarAssignments_.clear();
    sourceCode_.clear();
    kernelNames_.clear();
    localVarReplacements_.clear();
    localVarMap_.clear();

    inputVarOffset_.clear();
    std::size_t offset = 0;
    for (std::size_t i = 0; i < nInputVars_; ++i) {
        inputVarOffset_.push_back(offset);
        offset += inputVarIsScalar_[i] ? 1 : modelSize_;
    }

    initialized_ = true;
}

std::size_t GpuCodeGenerator::inputBufferSize() const {
    if (inputVarOffset_.empty())
        return 0;
    return inputVarOffset_.back() + (inputVarIsScalar_.back() ? 1 : modelSize_);
}

std::size_t GpuCodeGenerator::generateResultId() {
    std::size_t resultId;
    if (!freedVariables_.empty()) {
        resultId = freedVariables_.back();
        freedVariables_.pop_back();
    } else {
        resultId = nInputVars_ + nVariates_ + (nLocalVars_++);
    }
    return resultId;
}

std::pair<GpuCodeGenerator::VarType, std::size_t> GpuCodeGenerator::getVar(const std::size_t id) const {
    if (id < nInputVars_)
        return std::make_pair(VarType::input, id);
    else if (id < nInputVars_ + nVariates_)
        return std::make_pair(VarType::rn, id - nInputVars_);
    else
        return std::make_pair(VarType::local, id - nInputVars_ - nVariates_);
}

std::string GpuCodeGenerator::getVarStr(const std::pair<VarType, const std::size_t>& var) const {
    if (var.first == VarType::input)
        return "input[" + std::to_string(inputVarOffset_[var.second]) + "UL" +
               (inputVarIsScalar_[var.second] ? "" : "+i") + "]";
    else if (var.first == VarType::rn)
        return "rn[" + std::to_string(var.second * modelSize_) + "UL+i]";
    else if (var.first == VarType::local)
        return "values[" + std::to_string(var.second * modelSize_) + "UL+i]";
    else {
        QL_FAIL("GpuCodeGenerator::getId(): var type not handled, internal error.");
    }
}

std::size_t GpuCodeGenerator::getId(const std::pair<VarType, const std::size_t>& var) const {
    if (var.first == VarType::input)
        return var.second;
    else if (var.first == VarType::rn)
        return nInputVars_ + var.second;
    else if (var.first == VarType::local)
        return nInputVars_ + nVariates_ + var.second;
    else {
        QL_FAIL("GpuCodeGenerator::getId(): var type not handled, internal error.");
    }
}

std::size_t GpuCodeGenerator::applyOperation(const std::size_t randomVariableOpCode,
                                             const std::vector<std::size_t>& args) {
    std::size_t resultId = generateResultId();
    std::vector<std::pair<VarType, std::size_t>> rhs;
    std::transform(args.begin(), args.end(), std::back_inserter(rhs),
                   [this](const std::size_t id) { return getVar(id); });

    if (randomVariableOpCode == RandomVariableOpCode::ConditionalExpectation) {
        for (auto& r : rhs) {
            if (r.first == VarType::local)
                continue;
            /* generate assignment v_i = rhs-arg to ensure that all conditional expectation
               vars are local variables */
            applyOperation(RandomVariableOpCode::None, {getId(r)});
            r = ops_.back().lhs;
        }
    }

    Operation op;
    op.lhs = getVar(resultId);
    op.rhs = rhs;
    op.randomVariableOpCode = randomVariableOpCode;
    ops_.push_back(op);
    return resultId;
}

void GpuCodeGenerator::freeVariable(const std::size_t id) {
    // we do not free input variables or variates,  we only free variables that were added during the calc
    if (id < nInputVars_ + nVariates_)
        return;
    freedVariables_.push_back(id);
}

void GpuCodeGenerator::declareOutputVariable(const std::size_t id) { outputVars_.push_back(getVar(id)); }

void GpuCodeGenerator::generateBoilerplateCode() {
    // clang-format off
    sourceCode_ += std::string(  
            "bool ore_closeEnough(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y);\n"
            "bool ore_closeEnough(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y) {\n"
            "    const " + fpTypeStr_ + " tol = 42.0" + fpSuffix_ + " * " + fpEpsStr_ + ";\n"
            "    " + fpTypeStr_ + " diff = fabs(x - y);\n"
            "    if (x == 0.0" + fpSuffix_ + " || y == 0.0" + fpSuffix_ + ")\n"
            "        return diff < tol * tol;\n"
            "    return diff <= tol * fabs(x) || diff <= tol * fabs(y);\n"
            "}\n" +
            fpTypeStr_ + " ore_indicatorEq(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y);\n" +
            fpTypeStr_ + " ore_indicatorEq(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y) "
                                                "{ return ore_closeEnough(x, y) ? 1.0" + fpSuffix_ + " : 0.0" + fpSuffix_ +"; }\n" +
            fpTypeStr_ + " ore_indicatorGt(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y);\n" +
            fpTypeStr_ + " ore_indicatorGt(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y) " +
                                                "{ return x > y && !ore_closeEnough(x, y); }\n" +
            fpTypeStr_ + " ore_indicatorGeq(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y);\n" +
            fpTypeStr_ + " ore_indicatorGeq(const " + fpTypeStr_ + " x, const " + fpTypeStr_ + " y) { return x > y || ore_closeEnough(x, y); }\n" +
            fpTypeStr_ + " ore_normalCdf(const " + fpTypeStr_ + " x);\n" +
            fpTypeStr_ + " ore_normalCdf(const " + fpTypeStr_ + " x) {\n return 0.0" + fpSuffix_ + ";}\n" + 
            fpTypeStr_ + " ore_normalPdf(const " + fpTypeStr_ + " x);\n" +
            fpTypeStr_ + " ore_normalPdf(const " + fpTypeStr_ + " x) {\n" +
            "    " + fpTypeStr_ + " exponent = -(x*x)/2.0" + fpSuffix_ + ";\n" +
            "    return exponent <= -690.0" + fpSuffix_ + " ? 0.0 : exp(exponent) * 0.3989422804014327" + fpSuffix_ + ";\n"
            "}\n");
    // clang-format on
}

void GpuCodeGenerator::determineKernelBreakLines() {

    constexpr std::size_t max_kernel_lines = 4096;

    /* process conditional expectations and
       - generate resulting break lines
       - populate the conditionalExpectationVars_ container */

    std::set<std::pair<VarType, std::size_t>> currentCondExpVars;

    for (std::size_t i = 0; i < ops_.size(); ++i) {

        /* a new part is started when
           - we exceed the max number of lines per kernel or
           - as soon as the rhs of an op depends on a conditional expectation result var in the current kernel */

        if ((i + 1) % max_kernel_lines == 0 ||
            std::find_if(ops_[i].rhs.begin(), ops_[i].rhs.end(),
                         [&currentCondExpVars](const std::pair<VarType, std::size_t>& v) {
                             return currentCondExpVars.find(v) != currentCondExpVars.end();
                         }) != ops_[i].rhs.end()) {
            kernelBreakLines_.push_back(i);
            conditionalExpectationVars_.push_back({});
            currentCondExpVars.clear();
        }

        /* if op is a cond exp, update conditionalExpectationVars_ container and current cond exp results */

        if (ops_[i].randomVariableOpCode == RandomVariableOpCode::ConditionalExpectation) {
            conditionalExpectationVars_.back().push_back({});
            conditionalExpectationVars_.back().back().push_back(ops_[i].lhs);
            for (auto const& r : ops_[i].rhs)
                conditionalExpectationVars_.back().back().push_back(r);
            currentCondExpVars.insert(ops_[i].lhs);
        }
    }
}

void GpuCodeGenerator::determineLocalVarReplacements() {

    // determine the vars that we want to replace with local vars for each part

    constexpr std::size_t max_local_vars = 16;

    std::size_t kernelNo = 0;
    std::vector<std::size_t> freq(nLocalVars_, 0);

    for (std::size_t i = 0; i < ops_.size(); ++i) {

        if (kernelBreakLines_[kernelNo] == i) {
            std::vector<std::size_t> index(nLocalVars_);
            std::iota(index.begin(), index.end(), 0);
            std::sort(index.begin(), index.end(), [&freq](std::size_t a, std::size_t b) { return freq[a] > freq[b]; });
            localVarReplacements_.push_back({});
            for (std::size_t j = 0; j < std::min(max_local_vars, nLocalVars_); ++j) {
                localVarReplacements_.back().insert(index[j]);
            }
            std::fill(freq.begin(), freq.end(), 0);
            ++kernelNo;
        }

        if (ops_[i].lhs.first == VarType::local) {
            freq[ops_[i].lhs.second]++;
        }

        for (auto const& v : ops_[i].rhs) {
            if (v.first == VarType::local) {
                freq[v.second]++;
            }
        }
    }

    // determine the first usage on lhs and rhs for each replacement variable

    kernelNo = 0;

    for (std::size_t i = 0; i < ops_.size(); ++i) {

        if (kernelBreakLines_[kernelNo] == i) {
            ++kernelNo;
        }

        if (ops_[i].lhs.first == VarType::local) {
            if (std::set<LocalVarReplacement>::iterator f = localVarReplacements_[kernelNo].find(ops_[i].lhs.second);
                f != localVarReplacements_[kernelNo].end() && f->firstLhsUse() == std::nullopt) {
                f->setFirstLhsUse(i);
            }
        }

        for (auto const& r : ops_[i].rhs) {
            if (r.first == VarType::local) {
                if (std::set<LocalVarReplacement>::iterator f = localVarReplacements_[kernelNo].find(r.second);
                    f != localVarReplacements_[kernelNo].end() && f->firstRhsUse() == std::nullopt) {
                    f->setFirstRhsUse(i);
                }
            }
        }
    }

    /* mark replacement variables that need to be written to values buffer because
       - they are not used on lhs before rhs in a later part
       - they are an output and not on lhs in a later part */

    std::set<std::size_t> criticalLocalVars;

    for (std::size_t part = kernelBreakLines_.size() - 1; part > 0; --part) {

        if (part > 0) {
            std::set<std::size_t> varsOnLhs;
            for (std::size_t i = kernelBreakLines_[part - 1]; i < kernelBreakLines_[part]; ++i) {
                for (auto const& r : ops_[i].rhs) {
                    if (r.first == VarType::local && varsOnLhs.find(r.second) == varsOnLhs.end()) {
                        criticalLocalVars.insert(r.second);
                    }
                }
                if (part == kernelBreakLines_.size() - 1) {
                    for (auto const& o : outputVars_) {
                        if (varsOnLhs.find(o.second) == varsOnLhs.end()) {
                            criticalLocalVars.insert(o.second);
                        }
                    }
                }
                if (ops_[i].lhs.first == VarType::local)
                    varsOnLhs.insert(ops_[i].lhs.second);
            }
        }

        auto c = criticalLocalVars.begin();
        auto v = localVarReplacements_[part - 1].begin();

        while (c != criticalLocalVars.end() && v != localVarReplacements_[part - 1].end()) {
            if (*c == v->id()) {
                v->setToBeCached(true);
                ++c;
                ++v;
            } else if (*c < v->id()) {
                ++c;
            } else {
                ++v;
            }
        }
    }

    /* mark replacement variables that are also conditional expectation vars in the same part
       as to be written to values buffer */

    for (std::size_t part = 0; part < kernelBreakLines_.size(); ++part) {
        std::set<std::size_t> condExpVarsInPart;
        for (auto const& s : conditionalExpectationVars_[part]) {
            for (auto const& v : s) {
                condExpVarsInPart.insert(v.second);
            }
        }
        auto c = condExpVarsInPart.begin();
        auto v = localVarReplacements_[part].begin();

        while (c != condExpVarsInPart.end() && v != localVarReplacements_[part].end()) {
            if (*c == v->id()) {
                v->setToBeCached(true);
                ++c;
                ++v;
            } else if (*c < v->id()) {
                ++c;
            } else {
                ++v;
            }
        }
    }

    /* mark replacement variables in last part that are also output as to be written to values buffer */

    auto o = outputVars_.begin();
    auto v = localVarReplacements_[kernelBreakLines_.size() - 1].begin();

    while (o != outputVars_.end() && v != localVarReplacements_[kernelBreakLines_.size() - 1].end()) {
        if (o->second == v->id()) {
            v->setToBeCached(true);
            ++o;
            ++v;
        } else if (o->second < v->id()) {
            ++o;
        } else {
            ++v;
        }
    }

    /* if a variable is replaced in all parts and is not marked as to be cached, we can remove
       it from the set of local values buffer */

    std::set<LocalVarReplacement> tmp(localVarReplacements_.front());

    for (std::size_t part = 1; part < kernelBreakLines_.size(); ++part) {
        std::set<LocalVarReplacement> tmp2;
        std::set_intersection(tmp.begin(), tmp.end(), localVarReplacements_[part].begin(),
                              localVarReplacements_[part].end(), std::inserter(tmp2, tmp2.end()));
        tmp.swap(tmp2);
    }

    std::set<LocalVarReplacement> superfluousLocalVars;
    std::copy_if(tmp.begin(), tmp.end(), std::inserter(superfluousLocalVars, superfluousLocalVars.end()),
                 [](const LocalVarReplacement r) { return !r.toBeCached(); });

    /* build local var id map */

    auto s = superfluousLocalVars.begin();
    std::size_t counter = 0;
    for (std::size_t i = 0; i < nLocalVars_; ++i) {
        if (s != superfluousLocalVars.end() && s->id() == i) {
            ++s;
            continue;
        }
        localVarMap_[i] = counter++;
    }
}

void GpuCodeGenerator::generateKernelStartCode() {

    kernelNames_.push_back("ore_kernel_" + std::to_string(currentKernelNo_));

    std::vector<std::string> inputArgs;
    if (nInputVars_ > 0)
        inputArgs.push_back("__global " + fpTypeStr_ + "* input");
    if (nVariates_ > 0)
        inputArgs.push_back("__global " + fpTypeStr_ + "* rn");
    if (nLocalVars_ > 0)
        inputArgs.push_back("__global " + fpTypeStr_ + "* values");

    sourceCode_ += "__kernel void " + kernelNames_.back() + "(" + boost::join(inputArgs, ",") +
                   ") {\n"
                   "unsigned long i = get_global_id(0);\n"
                   "if(i < " +
                   std::to_string(modelSize_) + "UL) {\n";
}

void GpuCodeGenerator::generateKernelEndCode() { sourceCode_ += "}}\n"; }

void GpuCodeGenerator::generateOperationCode(const Operation& op) {

    std::string resultStr = getVarStr(op.lhs);
    std::vector<std::string> argStr;
    std::transform(op.rhs.begin(), op.rhs.end(), std::back_inserter(argStr),
                   [this](const std::pair<VarType, std::size_t>& v) { return getVarStr(v); });

    std::string code;

    switch (op.randomVariableOpCode) {
    case RandomVariableOpCode::None: {
        code = resultStr + "=" + argStr[0] + ";\n";
        break;
    }
    case RandomVariableOpCode::Add: {
        code = resultStr + "=" + boost::join(argStr, "+") + ";\n";
        break;
    }
    case RandomVariableOpCode::Subtract: {
        code = resultStr + "=" + argStr[0] + "-" + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::Negative: {
        code = resultStr + "=-" + argStr[0] + ";\n";
        break;
    }
    case RandomVariableOpCode::Mult: {
        code = resultStr + "=" + argStr[0] + "*" + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::Div: {
        code = resultStr + "=" + argStr[0] + "/" + argStr[1] + ";\n";
        break;
    }
    case RandomVariableOpCode::ConditionalExpectation: {
        // no code needed, calc is done by a special kernel or on the host
        break;
    }
    case RandomVariableOpCode::IndicatorEq: {
        code = resultStr + "=ore_indicatorEq(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::IndicatorGt: {
        code = resultStr + "=ore_indicatorGt(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::IndicatorGeq: {
        code = resultStr + "=ore_indicatorGeq(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::Min: {
        code = resultStr + "=fmin(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::Max: {
        code = resultStr + "=fmax(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    case RandomVariableOpCode::Abs: {
        code = resultStr + "=fabs(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Exp: {
        code = resultStr + "=exp(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Sqrt: {
        code = resultStr + "=sqrt(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Log: {
        code = resultStr + "=log(" + argStr[0] + ");\n";
        break;
    }
    case RandomVariableOpCode::Pow: {
        code = resultStr + "=pow(" + argStr[0] + "," + argStr[1] + ");\n";
        break;
    }
    // TODO add this in the kernel boilerplate code before activating it here
    // case RandomVariableOpCode::NormalCdf: {
    //     code = resultStr + "=ore_normalCdf(" + argStr[0] + ");\n";
    //     break;
    // }
    case RandomVariableOpCode::NormalPdf: {
        code = resultStr + "=ore_normalPdf(" + argStr[0] + ");\n";
        break;
    }
    default: {
        QL_FAIL("GpuCodeGenerator::generateOpCode(): no implementation for op code "
                << op.randomVariableOpCode << " (" << getRandomVariableOpLabels()[op.randomVariableOpCode]
                << ") provided.");
    }
    } // switch random var op code

    sourceCode_ += code;
}

void GpuCodeGenerator::generateOutputVarAssignments() {
    for (auto& o : outputVars_) {
        if (o.first == VarType::local)
            continue;
        /* generate assignment v_i = o and replace output var o to ensure all output vars
           are local variables */
        applyOperation(RandomVariableOpCode::None, {getId(o)});
        o = ops_.back().lhs;
    }
}

void GpuCodeGenerator::finalize() {

    // init state of this function

    currentKernelNo_ = 0;

    // preparations

    generateBoilerplateCode();
    determineKernelBreakLines();
    generateOutputVarAssignments();

    // add last line as break line, this is what the loop below expects

    kernelBreakLines_.push_back(ops_.size());

    // optimization: local var replacements

    determineLocalVarReplacements();

    // loop over ops and generate kernel code

    generateKernelStartCode();

    for (std::size_t i = 0; i < ops_.size() + 1; ++i) {
        if (i == kernelBreakLines_[currentKernelNo_]) {
            if (i == ops_.size())
                sourceCode_ += outputVarAssignments_;
            generateKernelEndCode();
            ++currentKernelNo_;
            if (i < ops_.size())
                generateKernelStartCode();
        }
        if (i < ops_.size())
            generateOperationCode(ops_[i]);
    }

    finalized_ = true;
}

bool operator<(const GpuCodeGenerator::LocalVarReplacement a, const GpuCodeGenerator::LocalVarReplacement b) {
    return a.id() < b.id();
}

} // namespace QuantExt
