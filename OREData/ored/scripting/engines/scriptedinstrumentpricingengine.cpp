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

#include <ored/scripting/engines/scriptedinstrumentamccalculator.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingengine.hpp>
#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/log.hpp>

#include <qle/instruments/cashflowresults.hpp>
#include <qle/math/randomvariable.hpp>

namespace ore {
namespace data {

namespace {

// helper that converts a context value to a ql additional result (i.e. boost::any)

struct anyGetter : public boost::static_visitor<boost::any> {
    explicit anyGetter(const QuantLib::ext::shared_ptr<Model>& model) : model_(model) {}
    boost::any operator()(const RandomVariable& x) const { return model_->extractT0Result(x); }
    boost::any operator()(const EventVec& x) const { return x.value; }
    boost::any operator()(const IndexVec& x) const { return x.value; }
    boost::any operator()(const CurrencyVec& x) const { return x.value; }
    boost::any operator()(const DaycounterVec& x) const { return x.value; }
    boost::any operator()(const Filter& x) const {
        QL_FAIL("can not convert Filter to boost::any, unexpected call to anyGetter");
    }
    QuantLib::ext::shared_ptr<Model> model_;
};

boost::any valueToAny(const QuantLib::ext::shared_ptr<Model>& model, const ValueType& v) {
    return boost::apply_visitor(anyGetter(model), v);
}

} // namespace

Real ScriptedInstrumentPricingEngine::addMcErrorEstimate(const std::string& label, const ValueType& v) const {
    if (model_->type() != Model::Type::MC)
        return Null<Real>();
    if (v.which() != ValueTypeWhich::Number)
        return Null<Real>();
    Real var = variance(QuantLib::ext::get<RandomVariable>(v)).at(0);
    Real errEst = std::sqrt(var / static_cast<double>(model_->size()));
    if(!label.empty())
        results_.additionalResults[label] = errEst;
    return errEst;
}

void ScriptedInstrumentPricingEngine::calculate() const {

    lastCalculationWasValid_ = false;

    // make sure we release the memory allocated by the model after the pricing
    struct MemoryReleaser {
        ~MemoryReleaser() { model->releaseMemory(); }
        QuantLib::ext::shared_ptr<Model> model;
    };
    MemoryReleaser memoryReleaser{model_};

    // set up copy of initial context to run the script engine on

    auto workingContext = QuantLib::ext::make_shared<Context>(*context_);

    // set TODAY in the context

    checkDuplicateName(workingContext, "TODAY");
    Date referenceDate = model_->referenceDate();
    workingContext->scalars["TODAY"] = EventVec{model_->size(), referenceDate};
    workingContext->constants.insert("TODAY");

    // clear NPVMem() regression coefficients

    model_->resetNPVMem();

    // if the model uses a separate training phase for NPV(), run this

    if (model_->trainingSamples() != Null<Size>()) {
        auto trainingContext = QuantLib::ext::make_shared<Context>(*workingContext);
        trainingContext->resetSize(model_->trainingSamples());
        struct TrainingPathToggle {
            TrainingPathToggle(QuantLib::ext::shared_ptr<Model> model) : model(model) { model->toggleTrainingPaths(); }
            ~TrainingPathToggle() { model->toggleTrainingPaths(); }
            QuantLib::ext::shared_ptr<Model> model;
        } toggle(model_);
        ScriptEngine trainingEngine(ast_, trainingContext, model_);
        trainingEngine.run(script_, interactive_);
    }

    // set up script engine and run it

    ScriptEngine engine(ast_, workingContext, model_);

    QuantLib::ext::shared_ptr<PayLog> paylog;
    if (generateAdditionalResults_)
        paylog = QuantLib::ext::make_shared<PayLog>();

    engine.run(script_, interactive_, paylog, includePastCashflows_);

    // extract npv result and set it

    auto npv = workingContext->scalars.find(npv_);
    QL_REQUIRE(npv != workingContext->scalars.end(),
               "did not find npv result variable '" << npv_ << "' as scalar in context");
    QL_REQUIRE(npv->second.which() == ValueTypeWhich::Number,
               "result variable '" << npv_ << "' must be of type NUMBER, got " << npv->second.which());
    results_.value = model_->extractT0Result(QuantLib::ext::get<RandomVariable>(npv->second));
    DLOG("got NPV = " << results_.value << " " << model_->baseCcy());

    // set additional results, if this feature is enabled

    if (generateAdditionalResults_) {
        results_.errorEstimate = addMcErrorEstimate("NPV_MCErrEst", npv->second);
        for (auto const& r : additionalResults_) {
            auto s = workingContext->scalars.find(r.second);
            bool resultSet = false;
            if (s != workingContext->scalars.end()) {
                boost::any t = valueToAny(model_, s->second);
                results_.additionalResults[r.first] = t;
                addMcErrorEstimate(r.first + "_MCErrEst", s->second);
                DLOG("got additional result '" << r.first << "' referencing script variable '" << r.second << "'");
                resultSet = true;
            }
            auto v = workingContext->arrays.find(r.second);
            if (v != workingContext->arrays.end()) {
                QL_REQUIRE(!resultSet, "result variable '"
                                           << r.first << "' referencing script variable '" << r.second
                                           << "' appears both as a scalar and an array, this is unexpected");
                QL_REQUIRE(!v->second.empty(), "result variable '" << v->first << "' is an empty array.");
                std::vector<double> tmpdouble;
                std::vector<std::string> tmpstring;
                std::vector<QuantLib::Date> tmpdate;
                for (auto const& d : v->second) {
                    boost::any t = valueToAny(model_, d);
                    if (t.type() == typeid(double))
                        tmpdouble.push_back(boost::any_cast<double>(t));
                    else if (t.type() == typeid(std::string))
                        tmpstring.push_back(boost::any_cast<std::string>(t));
                    else if (t.type() == typeid(QuantLib::Date))
                        tmpdate.push_back(boost::any_cast<QuantLib::Date>(t));
                    else {
                        QL_FAIL("unexpected result type '" << t.type().name() << "' for result variable '" << r.first
                                                           << "' referencing script variable '" << r.second << "'");
                    }
                }
                QL_REQUIRE((int)!tmpdouble.empty() + (int)!tmpstring.empty() + (int)!tmpdate.empty() == 1,
                           "expected exactly one result type in result array '" << v->first << "'");
                DLOG("got additional result '" << r.first << "' referencing script variable '" << r.second
                                               << "' vector of size "
                                               << tmpdouble.size() + tmpstring.size() + tmpdate.size());
                if (!tmpdouble.empty())
                    results_.additionalResults[r.first] = tmpdouble;
                else if (!tmpstring.empty())
                    results_.additionalResults[r.first] = tmpstring;
                else if (!tmpdate.empty())
                    results_.additionalResults[r.first] = tmpdate;
                else {
                    QL_FAIL("got empty result vector for result variable '"
                            << r.first << "' referencing script variable '" << r.second << "', this is unexpected");
                }
                std::vector<double> errEst;
                for (auto const& d : v->second) {
                    errEst.push_back(addMcErrorEstimate(std::string(), d));
                }
                if (!errEst.empty() && errEst.front() != Null<Real>())
                    results_.additionalResults[r.first + "_MCErrEst"] = errEst;
                resultSet = true;
            }
            QL_REQUIRE(resultSet, "could not set additional result '" << r.first << "' referencing script variable '"
                                                                      << r.second << "'");
        }

        // set contents from paylog as additional results

        paylog->consolidateAndSort();
        std::vector<CashFlowResults> cashFlowResults(paylog->size());
        std::map<Size, Size> cashflowNumber;
        for (Size i = 0; i < paylog->size(); ++i) {
            // cashflow is written as expectation of deflated base ccy amount at T0, converted to flow ccy
            // with the T0 FX Spot and compounded back to the pay date on T0 curves
            Real fx = 1.0;
            Real discount = 1.0;
            if (paylog->dates().at(i) > model_->referenceDate()) {
                fx = model_->fxSpotT0(paylog->currencies().at(i), model_->baseCcy());
                discount = model_->discount(referenceDate, paylog->dates().at(i), paylog->currencies().at(i)).at(0);
            }
            cashFlowResults[i].amount = model_->extractT0Result(paylog->amounts().at(i)) / fx / discount;
            cashFlowResults[i].payDate = paylog->dates().at(i);
            cashFlowResults[i].currency = paylog->currencies().at(i);
            cashFlowResults[i].legNumber = paylog->legNos().at(i);
            cashFlowResults[i].type = paylog->cashflowTypes().at(i);
            cashFlowResults[i].discountFactor = discount;
            DLOG("got cashflow " << QuantLib::io::iso_date(cashFlowResults[i].payDate) << " "
                                 << cashFlowResults[i].currency << cashFlowResults[i].amount << " "
                                 << cashFlowResults[i].currency << "-" << model_->baseCcy() << " " << fx << " discount("
                                 << cashFlowResults[i].currency << ") " << discount);
            if (paylog->dates().at(i) > model_->referenceDate()) {
                addMcErrorEstimate("cashflow_" + std::to_string(paylog->legNos().at(i)) + "_" +
                                       std::to_string(++cashflowNumber[paylog->legNos().at(i)]) + "_MCErrEst",
                                   paylog->amounts().at(i) /
                                       RandomVariable(paylog->amounts().at(i).size(), (fx * discount)));
            }
        }
        results_.additionalResults["cashFlowResults"] = cashFlowResults;

        // set additional results from the model

        results_.additionalResults.insert(model_->additionalResults().begin(), model_->additionalResults().end());

    } // if generate additional results

    // if the engine is amc enabled, add an amc calculator to the additional results

    if (amcEnabled_) {
        DLOG("add amc calculator to results");
        results_.additionalResults["amcCalculator"] =
            QuantLib::ext::static_pointer_cast<AmcCalculator>(QuantLib::ext::make_shared<ScriptedInstrumentAmcCalculator>(
                npv_, model_, ast_, context_, script_, interactive_, amcStickyCloseOutStates_));
    }

    lastCalculationWasValid_ = true;
}

} // namespace data
} // namespace ore
