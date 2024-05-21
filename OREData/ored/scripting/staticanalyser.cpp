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

#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/staticanalyser.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/log.hpp>

#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/errors.hpp>

#include <algorithm>

#define TRACE(message, n)                                                                                              \
    { DLOGGERSTREAM(message << " at " << to_string((n).locationInfo) << '\n'); }

namespace ore {
namespace data {

namespace {
class ASTIndexExtractor : public AcyclicVisitor,
                          public Visitor<ASTNode>,
                          public Visitor<FunctionPayNode>,
                          public Visitor<FunctionLogPayNode>,
                          public Visitor<FunctionNpvNode>,
                          public Visitor<FunctionNpvMemNode>,
                          public Visitor<FunctionDiscountNode>,
                          public Visitor<FunctionFwdCompNode>,
                          public Visitor<FunctionFwdAvgNode>,
                          public Visitor<FunctionAboveProbNode>,
                          public Visitor<FunctionBelowProbNode>,
                          public Visitor<VarEvaluationNode> {
public:
    ASTIndexExtractor(std::map<std::string, std::set<QuantLib::Date>>& indexEvalDates,
                      std::map<std::string, std::set<QuantLib::Date>>& indexFwdDates,
                      std::map<std::string, std::set<QuantLib::Date>>& payObsDates,
                      std::map<std::string, std::set<QuantLib::Date>>& payPayDates,
                      std::map<std::string, std::set<QuantLib::Date>>& discountObsDates,
                      std::map<std::string, std::set<QuantLib::Date>>& discountPayDates,
                      std::map<std::string, std::set<QuantLib::Date>>& fwdCompAvgFixingDates,
                      std::map<std::string, std::set<QuantLib::Date>>& fwdCompAvgEvalDates,
                      std::map<std::string, std::set<QuantLib::Date>>& fwdCompAvgStartEndDates,
                      std::map<std::string, std::set<QuantLib::Date>>& probFixingDates,
                      std::set<QuantLib::Date>& regressionDates, Context& context, ASTNode*& lastVisitedNode)
        : indexEvalDates_(indexEvalDates), indexFwdDates_(indexFwdDates), payObsDates_(payObsDates),
          payPayDates_(payPayDates), discountObsDates_(discountObsDates), discountPayDates_(discountPayDates),
          fwdCompAvgFixingDates_(fwdCompAvgFixingDates), fwdCompAvgEvalDates_(fwdCompAvgEvalDates),
          fwdCompAvgStartEndDates_(fwdCompAvgStartEndDates), probFixingDates_(probFixingDates),
          regressionDates_(regressionDates), context_(context), lastVisitedNode_(lastVisitedNode) {}

    void checkpoint(ASTNode& n) { lastVisitedNode_ = &n; }

    void visit(ASTNode& n) override {
        checkpoint(n);
        for (auto p : n.args)
            if (p)
                p->accept(*this);
    }

    std::string getVariableName(const ASTNodePtr p) {
        checkpoint(*p);
        auto var = QuantLib::ext::dynamic_pointer_cast<VariableNode>(p);
        if (var) {
            TRACE("getVariableName(" << var->name << ")", *p);
            return var->name;
        }
        QL_FAIL("not a variable identifier");
    }

    // returns Null<Real>() if not a constant number node
    Real getConstantNumber(const ASTNodePtr p) {
        checkpoint(*p);
        auto cn = QuantLib::ext::dynamic_pointer_cast<ConstantNumberNode>(p);
        if (cn) {
            TRACE("getConstantNumber(" << cn->value << ")", *p);
            return cn->value;
        }
        return Null<Real>();
    }

    std::vector<ValueType> getVariableValues(const std::string& name) {
        std::vector<ValueType> result;
        if (name.empty())
            return result;
        bool found = false;
        // TODAY is allowed as an argument for a VarEvaluationNode, but actually not defined
        // in the context necessarily when running the static analysis, and we don't need to add
        // it to any results of the static analysis, so we just return an empty vector.
        if (name == "TODAY") {
            found = true;
        }
        auto scalar = context_.scalars.find(name);
        if (scalar != context_.scalars.end()) {
            result = std::vector<ValueType>(1, scalar->second);
            found = true;
        }
        auto array = context_.arrays.find(name);
        if (array != context_.arrays.end()) {
            result = array->second;
            found = true;
        }
        QL_REQUIRE(found, "variable '" << name << "' is not defined.");
        // there might be null dates in the result, which we remove: Either they are not used
        // in the script execution step or will throw an error there, but we can't deduce any
        // useful information during the static analysis, so we just ignore them
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const ValueType& v) {
                                        return v.which() == ValueTypeWhich::Event &&
                                               QuantLib::ext::get<EventVec>(v).value == Date();
                                    }),
                     result.end());
        return result;
    }

    void visit(VarEvaluationNode& n) override {
        checkpoint(n);
        std::string indexVariable = getVariableName(n.args[0]);
        std::string datesVariable = getVariableName(n.args[1]);
        std::string fwdDatesVariable = n.args[2] ? getVariableName(n.args[2]) : "";
        checkpoint(n);
        TRACE("varEvaluation(" << indexVariable << ", " << datesVariable << ", " << fwdDatesVariable << ")", n);
        std::vector<ValueType> indexValues = getVariableValues(indexVariable);
        std::vector<ValueType> datesValues = getVariableValues(datesVariable);
        std::vector<ValueType> fwdDatesValues =
            fwdDatesVariable.empty() ? std::vector<ValueType>() : getVariableValues(fwdDatesVariable);
        for (auto const& v : indexValues)
            TRACE("got index " << v, n);
        for (auto const& v : datesValues)
            TRACE("got date " << v, n);
        for (auto const& v : fwdDatesValues)
            TRACE("got fwd date " << v, n);
        for (auto const& i : indexValues) {
            for (auto const& d : datesValues) {
                QL_REQUIRE(i.which() == ValueTypeWhich::Index, "index expected on lhs");
                QL_REQUIRE(d.which() == ValueTypeWhich::Event, "event expected as obs date");
                indexEvalDates_[QuantLib::ext::get<IndexVec>(i).value].insert(QuantLib::ext::get<EventVec>(d).value);
            }
            for (auto const& d : fwdDatesValues) {
                QL_REQUIRE(i.which() == ValueTypeWhich::Index, "index expected on lhs");
                QL_REQUIRE(d.which() == ValueTypeWhich::Event, "event expected as fwd date");
                indexFwdDates_[QuantLib::ext::get<IndexVec>(i).value].insert(QuantLib::ext::get<EventVec>(d).value);
            }
        }
        visit(static_cast<ASTNode&>(n));
    }

    void processPayOrDiscountNode(std::map<std::string, std::set<QuantLib::Date>>& obsDates,
                                  std::map<std::string, std::set<QuantLib::Date>>& payDates, ASTNode& n,
                                  const Size indexObs, const Size indexPay, const Size indexCurr) {
        checkpoint(n);
        std::string obsDateVariable = getVariableName(n.args[indexObs]);
        std::vector<ValueType> obsDateValues = getVariableValues(obsDateVariable);
        std::string payDateVariable = getVariableName(n.args[indexPay]);
        std::vector<ValueType> payDateValues = getVariableValues(payDateVariable);
        std::string currencyVariable = getVariableName(n.args[indexCurr]);
        std::vector<ValueType> currencyValues = getVariableValues(currencyVariable);
        checkpoint(n);
        TRACE("pay(" << obsDateVariable << "," << payDateVariable << "," << currencyVariable << ")", n);
        for (auto const& v : obsDateValues)
            TRACE("got obs date " << v, n);
        for (auto const& v : payDateValues)
            TRACE("got pay date " << v, n);
        for (auto const& v : currencyValues)
            TRACE("got currency " << v, n);
        for (auto const& v : obsDateValues) {
            QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected as arg #" << (indexObs + 1));
            for (auto const& c : currencyValues) {
                QL_REQUIRE(c.which() == ValueTypeWhich::Currency, "currency expected as arg #" << (indexCurr + 1));
                obsDates[QuantLib::ext::get<CurrencyVec>(c).value].insert(QuantLib::ext::get<EventVec>(v).value);
            }
        }
        for (auto const& v : payDateValues) {
            QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected as arg #" << (indexPay + 1));
            for (auto const& c : currencyValues) {
                QL_REQUIRE(c.which() == ValueTypeWhich::Currency, "currency expected as arg #" << (indexCurr + 1));
                payDates[QuantLib::ext::get<CurrencyVec>(c).value].insert(QuantLib::ext::get<EventVec>(v).value);
            }
        }
        visit(static_cast<ASTNode&>(n));
    }

    void visit(FunctionPayNode& n) override { processPayOrDiscountNode(payObsDates_, payPayDates_, n, 1, 2, 3); }
    void visit(FunctionLogPayNode& n) override { processPayOrDiscountNode(payObsDates_, payPayDates_, n, 1, 2, 3); }

    void visit(FunctionDiscountNode& n) override {
        processPayOrDiscountNode(discountObsDates_, discountPayDates_, n, 0, 1, 2);
    }

    void processFwdCompAvg(ASTNode& n) {
        checkpoint(n);
        std::string indexVariable = getVariableName(n.args[0]);
        std::vector<ValueType> indexValues = getVariableValues(indexVariable);
        std::string obsDateVariable = getVariableName(n.args[1]);
        std::vector<ValueType> obsDateValues = getVariableValues(obsDateVariable);
        std::string startDateVariable = getVariableName(n.args[2]);
        std::vector<ValueType> startDateValues = getVariableValues(startDateVariable);
        std::string endDateVariable = getVariableName(n.args[3]);
        std::vector<ValueType> endDateValues = getVariableValues(endDateVariable);
        std::vector<ValueType> lookbackValues{RandomVariable(1, 0.0)};
        std::vector<ValueType> fixingDaysValues{RandomVariable(1, 0.0)};
        if (n.args[6]) {
            auto value = getConstantNumber(n.args[6]);
            if (value != Null<Real>())
                lookbackValues = std::vector<ValueType>{RandomVariable(1, value)};
            else {
                std::string lookbackVariable = getVariableName(n.args[6]);
                lookbackValues = getVariableValues(lookbackVariable);
            }
        }
        if (n.args[8]) {
            auto value = getConstantNumber(n.args[8]);
            if (value != Null<Real>()) {
                fixingDaysValues = std::vector<ValueType>{RandomVariable(1, value)};
            } else {
                std::string fixingDaysVariable = getVariableName(n.args[8]);
                fixingDaysValues = getVariableValues(fixingDaysVariable);
            }
        }
        checkpoint(n);
        TRACE("fwd[comp|avg](" << indexVariable << "," << obsDateVariable << "," << startDateVariable << ","
                               << endDateVariable << ")",
              n);
        for (auto const& i : indexValues)
            TRACE("got index " << i, n);
        for (auto const& v : obsDateValues)
            TRACE("got obs date " << v, n);
        for (auto const& v : startDateValues)
            TRACE("got start date " << v, n);
        for (auto const& v : endDateValues)
            TRACE("got end date " << v, n);
        for (auto const& i : indexValues) {
            QL_REQUIRE(i.which() == ValueTypeWhich::Index, "index expected as arg #1");
            std::string indexName = QuantLib::ext::get<IndexVec>(i).value;
            IndexInfo ind(indexName);
            auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(ind.irIbor());
            // ignore indices that are not on, those are not allowed in the end, but might still occur
            // here, wenn an array contains both libor and on indices
            if (!on) {
                DLOG("skipping index " << i << "since it is not an overnight index.");
                continue;
            }
            for (auto const& v : obsDateValues) {
                QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected as arg #2 (obsdate)");
                fwdCompAvgEvalDates_[indexName].insert(QuantLib::ext::get<EventVec>(v).value);
            }
            Date minStart = Date::maxDate();
            Date maxEnd = Date::minDate();
            for (auto const& v : startDateValues) {
                QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected as arg #3 (startdate)");
                minStart = std::min(minStart, QuantLib::ext::get<EventVec>(v).value);
            }
            for (auto const& w : endDateValues) {
                QL_REQUIRE(w.which() == ValueTypeWhich::Event, "date expected as arg #4 (enddate)");
                maxEnd = std::max(maxEnd, QuantLib::ext::get<EventVec>(w).value);
            }
            if (minStart >= maxEnd)
                continue;
            RandomVariable lookback, fixingDays;
            for (auto const& l : lookbackValues) {
                QL_REQUIRE(l.which() == ValueTypeWhich::Number, "number expected as arg #7 (lookback)");
                lookback = QuantLib::ext::get<RandomVariable>(l);
                QL_REQUIRE(lookback.deterministic(), "expected arg #7 (lookback) to be deterministic");
                for (auto const& f : fixingDaysValues) {
                    QL_REQUIRE(f.which() == ValueTypeWhich::Number, "number expected as arg #9 (fixingDays)");
                    fixingDays = QuantLib::ext::get<RandomVariable>(f);
                    QL_REQUIRE(fixingDays.deterministic(), "expected arg #9 (fixingDays) to be deterministic");
                    // construct template coupon and extract fixing and value dates
                    QuantExt::OvernightIndexedCoupon cpn(
                        maxEnd, 1.0, minStart, maxEnd, on, 1.0, 0.0, Date(), Date(), DayCounter(), false, false,
                        static_cast<Integer>(lookback.at(0)) * Days, 0, static_cast<Natural>(fixingDays.at(0)));
                    fwdCompAvgFixingDates_[indexName].insert(cpn.fixingDates().begin(), cpn.fixingDates().end());
                    DLOG("adding " << cpn.fixingDates().size() << " fixing dates for index " << indexName);
                    if (!cpn.valueDates().empty()) {
                        fwdCompAvgStartEndDates_[indexName].insert(cpn.valueDates().front());
                        fwdCompAvgStartEndDates_[indexName].insert(cpn.valueDates().back());
                    }
                }
            }
        }
        visit(static_cast<ASTNode&>(n));
    }

    void visit(FunctionFwdCompNode& n) override { processFwdCompAvg(n); }

    void visit(FunctionFwdAvgNode& n) override { processFwdCompAvg(n); }

    void processProbNode(ASTNode& n) {
        checkpoint(n);
        std::string indexVariable = getVariableName(n.args[0]);
        std::vector<ValueType> indexValues = getVariableValues(indexVariable);
        std::string obsDate1Variable = getVariableName(n.args[1]);
        std::vector<ValueType> obsDate1Values = getVariableValues(obsDate1Variable);
        std::string obsDate2Variable = getVariableName(n.args[2]);
        std::vector<ValueType> obsDate2Values = getVariableValues(obsDate2Variable);
        checkpoint(n);
        TRACE("prob(" << indexVariable << "," << obsDate1Variable << "," << obsDate2Variable, n);
        for (auto const& i : indexValues)
            TRACE("got index " << i, n);
        for (auto const& v : obsDate1Values)
            TRACE("got obs date 1 " << v, n);
        for (auto const& v : obsDate2Values)
            TRACE("got obs date 2 " << v, n);
        for (auto const& i : indexValues) {
            QL_REQUIRE(i.which() == ValueTypeWhich::Index, "index expected as arg #1");
            std::string indexName = QuantLib::ext::get<IndexVec>(i).value;
            Date minDate = Date::maxDate(), maxDate = Date::minDate();
            for (auto const& v : obsDate1Values) {
                QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected as arg #2");
                Date d = QuantLib::ext::get<EventVec>(v).value;
                indexEvalDates_[indexName].insert(d);
                if (d < minDate)
                    minDate = d;
                if (d > maxDate)
                    maxDate = d;
            }
            for (auto const& v : obsDate2Values) {
                QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected as arg #3");
                Date d = QuantLib::ext::get<EventVec>(v).value;
                indexEvalDates_[indexName].insert(d);
                if (d < minDate)
                    minDate = d;
                if (d > maxDate)
                    maxDate = d;
            }
            // determine the fixing calendar (assume that for commodity this is the same for different futures)
            Calendar fixingCalendar;
            IndexInfo ind(indexName);
            if (ind.isComm()) {
                Date today = Settings::instance().evaluationDate(); // any date will do
                fixingCalendar = ind.comm(today)->fixingCalendar();
            } else {
                fixingCalendar = ind.index()->fixingCalendar();
            }
            DLOG("adding prob fixing dates from " << QuantLib::io::iso_date(minDate) << " to "
                                                  << QuantLib::io::iso_date(maxDate) << " for " << indexName);
            for (Date d = minDate; d <= maxDate; ++d) {
                if (fixingCalendar.isBusinessDay(d)) {
                    probFixingDates_[indexName].insert(d);
                }
            }
        }
    }

    void visit(FunctionAboveProbNode& n) override {
        processProbNode(n);
        visit(static_cast<ASTNode&>(n));
    }

    void visit(FunctionBelowProbNode& n) override {
        processProbNode(n);
        visit(static_cast<ASTNode&>(n));
    }

    void processNpvNode(ASTNode& n) {
        checkpoint(n);
        std::string obsDateVariable = getVariableName(n.args[1]);
        std::vector<ValueType> obsDateValues = getVariableValues(obsDateVariable);
        checkpoint(n);
        TRACE("npv(" << obsDateVariable << ")", n);
        for (auto const& v : obsDateValues) {
            QL_REQUIRE(v.which() == ValueTypeWhich::Event, "date expected and 2nd argument");
            regressionDates_.insert(QuantLib::ext::get<EventVec>(v).value);
        }
        visit(static_cast<ASTNode&>(n));
    }

    void visit(FunctionNpvNode& n) override { processNpvNode(n); }

    void visit(FunctionNpvMemNode& n) override { processNpvNode(n); }

private:
    std::map<std::string, std::set<QuantLib::Date>>&indexEvalDates_, &indexFwdDates_, &payObsDates_, &payPayDates_,
        &discountObsDates_, &discountPayDates_, &fwdCompAvgFixingDates_, &fwdCompAvgEvalDates_,
        &fwdCompAvgStartEndDates_, &probFixingDates_;
    std::set<QuantLib::Date>& regressionDates_;
    Context& context_;
    ASTNode*& lastVisitedNode_;
};
} // namespace

void StaticAnalyser::run(const std::string& script) {
    indexEvalDates_.clear();
    indexFwdDates_.clear();
    payObsDates_.clear();
    payPayDates_.clear();
    discountObsDates_.clear();
    discountPayDates_.clear();
    fwdCompAvgFixingDates_.clear();
    fwdCompAvgEvalDates_.clear();
    fwdCompAvgStartEndDates_.clear();
    probFixingDates_.clear();
    regressionDates_.clear();

    ASTNode* loc;
    ASTIndexExtractor runner(indexEvalDates_, indexFwdDates_, payObsDates_, payPayDates_, discountObsDates_,
                             discountPayDates_, fwdCompAvgFixingDates_, fwdCompAvgEvalDates_, fwdCompAvgStartEndDates_,
                             probFixingDates_, regressionDates_, *context_, loc);

    try {
        root_->accept(runner);
    } catch (const std::exception& e) {
        std::ostringstream errorMessage;
        errorMessage << "Error during static script analysis: " << e.what() << " at "
                     << (loc ? to_string(loc->locationInfo) : "(last visited ast node not known)")
                     << " - see log for more details.";

        LOGGERSTREAM(printCodeContext(script, loc));
        ALOG(errorMessage.str());

        QL_FAIL(errorMessage.str());
    }

    DLOG("Static analyser finished without errors.");
}

} // namespace data
} // namespace ore
