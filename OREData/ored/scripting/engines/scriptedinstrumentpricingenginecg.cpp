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

#include <ored/scripting/engines/scriptedinstrumentamccalculator.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingenginecg.hpp>
#include <ored/scripting/utilities.hpp>

#include <qle/ad/backwardderivatives.hpp>
#include <qle/ad/forwardevaluation.hpp>
#include <qle/ad/ssaform.hpp>
#include <qle/methods/multipathvariategenerator.hpp>

#include <ored/utilities/log.hpp>

#include <qle/instruments/cashflowresults.hpp>
#include <qle/math/computeenvironment.hpp>
#include <qle/math/randomvariable.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>

namespace ore {
namespace data {

namespace {

// helper that converts a context value to a ql additional result (i.e. boost::any)

struct anyGetterCg : public boost::static_visitor<boost::any> {
    boost::any operator()(const RandomVariable& x) const { QL_FAIL("unexpected call to anyGetter (RandomVariable)"); }
    boost::any operator()(const EventVec& x) const { return x.value; }
    boost::any operator()(const IndexVec& x) const { return x.value; }
    boost::any operator()(const CurrencyVec& x) const { return x.value; }
    boost::any operator()(const DaycounterVec& x) const { return x.value; }
    boost::any operator()(const Filter& x) const { QL_FAIL("unexpected call to anyGetter (Filter)"); }
};

boost::any valueToAnyCg(const ValueType& v) { return boost::apply_visitor(anyGetterCg(), v); }

double externalAverage(const std::vector<double>& v) {
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::mean>> acc;
    for (auto const& x : v) {
        acc(x);
    }
    return boost::accumulators::mean(acc);
}

} // namespace

ScriptedInstrumentPricingEngineCG::~ScriptedInstrumentPricingEngineCG() {
    if (externalCalculationId_)
        ComputeEnvironment::instance().context().disposeCalculation(externalCalculationId_);
}

ScriptedInstrumentPricingEngineCG::ScriptedInstrumentPricingEngineCG(
    const std::string& npv, const std::vector<std::pair<std::string, std::string>>& additionalResults,
    const QuantLib::ext::shared_ptr<ModelCG>& model, const ASTNodePtr ast,
    const QuantLib::ext::shared_ptr<Context>& context, const Model::McParams& mcParams, const std::string& script,
    const bool interactive, const bool generateAdditionalResults, const bool includePastCashflows,
    const bool useCachedSensis, const bool useExternalComputeFramework,
    const bool useDoublePrecisionForExternalCalculation)
    : npv_(npv), additionalResults_(additionalResults), model_(model), ast_(ast), context_(context),
      mcParams_(mcParams), script_(script), interactive_(interactive),
      generateAdditionalResults_(generateAdditionalResults), includePastCashflows_(includePastCashflows),
      useCachedSensis_(useCachedSensis), useExternalComputeFramework_(useExternalComputeFramework),
      useDoublePrecisionForExternalCalculation_(useDoublePrecisionForExternalCalculation) {

    // register with model

    registerWith(model_);

    // get ops + labels, grads and op node requirements

    opNodeRequirements_ = getRandomVariableOpNodeRequirements();
    if (useExternalComputeFramework_) {
        opsExternal_ = getExternalRandomVariableOps();
        gradsExternal_ = getExternalRandomVariableGradients();
    } else {
        ops_ = getRandomVariableOps(model_->size(), mcParams_.regressionOrder, mcParams_.polynomType, 0.0,
                                    mcParams_.regressionVarianceCutoff);
        grads_ = getRandomVariableGradients(model_->size(), mcParams_.regressionOrder, mcParams_.polynomType, 0.2,
                                            mcParams_.regressionVarianceCutoff);
    }
}

void ScriptedInstrumentPricingEngineCG::buildComputationGraph() const {

    if (cgVersion_ != model_->cgVersion()) {

        auto g = model_->computationGraph();

        // clear NPVMem() regression coefficients

        model_->resetNPVMem();

        // set up copy of initial context to run the cg builder against

        workingContext_ = QuantLib::ext::make_shared<Context>(*context_);

        // set TODAY in the context

        checkDuplicateName(workingContext_, "TODAY");
        Date referenceDate = model_->referenceDate();
        workingContext_->scalars["TODAY"] = EventVec{model_->size(), referenceDate};
        workingContext_->constants.insert("TODAY");

        // set variables from initial context

        for (auto const& v : workingContext_->scalars) {
            if (v.second.which() == ValueTypeWhich::Number) {
                auto r = QuantLib::ext::get<RandomVariable>(v.second);
                QL_REQUIRE(r.deterministic(), "ScriptedInstrumentPricingEngineCG::calculate(): expected variable '"
                                                  << v.first << "' from initial context to be deterministic, got "
                                                  << r);
                g->setVariable(v.first + "_0", cg_const(*g, r.at(0)));
            }
        }

        for (auto const& a : workingContext_->arrays) {
            for (Size i = 0; i < a.second.size(); ++i) {
                if (a.second[i].which() == ValueTypeWhich::Number) {
                    auto r = QuantLib::ext::get<RandomVariable>(a.second[i]);
                    QL_REQUIRE(r.deterministic(), "ScriptedInstrumentPricingEngineCG::calculate(): expected variable '"
                                                      << a.first << "[" << i
                                                      << "]' from initial context to be deterministic, got " << r);
                    g->setVariable(a.first + "_" + std::to_string(i), cg_const(*g, r.at(0)));
                }
            }
        }

        // build graph

        ComputationGraphBuilder cgBuilder(*g, getRandomVariableOpLabels(), ast_, workingContext_, model_);
        cgBuilder.run(generateAdditionalResults_, includePastCashflows_, script_, interactive_);
        cgVersion_ = model_->cgVersion();
        DLOG("Built computation graph version " << cgVersion_ << " size is " << g->size());
        TLOGGERSTREAM(ssaForm(*g, getRandomVariableOpLabels()));
        keepNodes_ = cgBuilder.keepNodes();
        payLogEntries_ = cgBuilder.payLogEntries();

        // clear stored base model params

        haveBaseValues_ = false;
    }
}

void ScriptedInstrumentPricingEngineCG::calculate() const {

    // TODOs
    QL_REQUIRE(!useExternalComputeFramework_ || !generateAdditionalResults_,
               "ScriptedInstrumentPricingEngineCG: when using external compute framework, generation of additional "
               "results is not supported yet.");
    QL_REQUIRE(!useExternalComputeFramework_ || !useCachedSensis_,
               "ScriptedInstrumentPricingEngineCG: when using external compute framework, usage of cached sensis is "
               "not supported yet");
    QL_REQUIRE(model_->trainingSamples() == Null<Size>(), "ScriptedInstrumentPricingEngineCG: separate training phase "
                                                          "not supported, trainingSamples can not be specified.");

    lastCalculationWasValid_ = false;

    buildComputationGraph();

    if (!haveBaseValues_ || !useCachedSensis_) {

        // calculate NPV and Sensis ("base scenario"), store base npv + sensis + base model params

        auto g = model_->computationGraph();

        bool newExternalCalc = false;
        if (useExternalComputeFramework_) {
            QL_REQUIRE(ComputeEnvironment::instance().hasContext(),
                       "ScriptedInstrumentPricingEngineCG::calculate(): no compute enviroment context selected.");
            ComputeContext::Settings settings;
            settings.debug = false;
            settings.useDoublePrecision = useDoublePrecisionForExternalCalculation_;
            settings.rngSequenceType = mcParams_.sequenceType;
            settings.rngSeed = mcParams_.seed;
            settings.regressionOrder = mcParams_.regressionOrder;
            std::tie(externalCalculationId_, newExternalCalc) =
                ComputeEnvironment::instance().context().initiateCalculation(model_->size(), externalCalculationId_,
                                                                             cgVersion_, settings);
            DLOG("initiated external calculation id " << externalCalculationId_ << ", version " << cgVersion_);
        }

        // populate values

        std::vector<RandomVariable> values(g->size(), RandomVariable(model_->size()));

        std::vector<ExternalRandomVariable> valuesExternal;
        if (useExternalComputeFramework_)
            valuesExternal.resize(g->size());

        // set constants

        for (auto const& c : g->constants()) {
            if (useExternalComputeFramework_) {
                valuesExternal[c.second] = ExternalRandomVariable(c.first);
            } else {
                values[c.second] = RandomVariable(model_->size(), c.first);
            }
        }

        // set model parameters

        baseModelParams_ = model_->modelParameters();
        for (auto const& p : baseModelParams_) {
            TLOG("setting model parameter at node " << p.first << " to value " << std::setprecision(16) << p.second);
            if (useExternalComputeFramework_) {
                valuesExternal[p.first] = ExternalRandomVariable(p.second);
            } else {
                values[p.first] = RandomVariable(model_->size(), p.second);
            }
        }
        DLOG("set " << baseModelParams_.size() << " model parameters");

        // set random variates

        auto const& rv = model_->randomVariates();
        if (!rv.empty()) {
            if (useExternalComputeFramework_) {
                if (newExternalCalc) {
                    auto gen =
                        ComputeEnvironment::instance().context().createInputVariates(rv.size(), rv.front().size());
                    for (Size k = 0; k < rv.size(); ++k) {
                        for (Size j = 0; j < rv.front().size(); ++j)
                            valuesExternal[rv[k][j]] = ExternalRandomVariable(gen[k][j]);
                    }
                }
            } else {
                if (mcParams_.sequenceType == QuantExt::SequenceType::MersenneTwister &&
                    mcParams_.externalDeviceCompatibilityMode) {
                    // use same order for rng generation as it is (usually) done on external devices
                    // this is mainly done to be able to reconcile results produced on external devices
                    auto rng = std::make_unique<MersenneTwisterUniformRng>(mcParams_.seed);
                    QuantLib::InverseCumulativeNormal icn;
                    for (Size j = 0; j < rv.front().size(); ++j) {
                        for (Size i = 0; i < rv.size(); ++i) {
                            for (Size path = 0; path < model_->size(); ++path) {
                                values[rv[i][j]].set(path, icn(rng->nextReal()));
                            }
                        }
                    }
                } else {
                    // use the 'usual' path generation that we also use elsewhere
                    auto gen = makeMultiPathVariateGenerator(mcParams_.sequenceType, rv.size(), rv.front().size(),
                                                             mcParams_.seed, mcParams_.sobolOrdering,
                                                             mcParams_.sobolDirectionIntegers);
                    for (Size path = 0; path < model_->size(); ++path) {
                        auto p = gen->next();
                        for (Size j = 0; j < rv.front().size(); ++j) {
                            for (Size k = 0; k < rv.size(); ++k) {
                                values[rv[k][j]].set(path, p.value[j][k]);
                            }
                        }
                    }
                }
            }
            DLOG("generated random variates for dim = " << rv.size() << ", steps = " << rv.front().size());
        }

        // set flags for nodes we want to keep (model params, npv and additional results)

        std::vector<bool> keepNodes(g->size(), false);

        keepNodes[cg_var(*g, npv_ + "_0")] = true;

        for (auto const& p : baseModelParams_)
            keepNodes[p.first] = true;

        if (generateAdditionalResults_) {
            for (auto const& r : additionalResults_) {
                auto s = workingContext_->scalars.find(r.second);
                if (s != workingContext_->scalars.end() && s->second.which() == ValueTypeWhich::Number) {
                    keepNodes[cg_var(*g, r.second + "_0")] = true;
                }
                auto v = workingContext_->arrays.find(r.second);
                if (v != workingContext_->arrays.end()) {
                    for (Size i = 0; i < v->second.size(); ++i) {
                        keepNodes[cg_var(*g, r.second + "_" + std::to_string(i))] = true;
                    }
                }
            }
            for (auto const& n : keepNodes_)
                keepNodes[n] = true;
        }

        // run the forward evaluation

        std::size_t baseNpvNode = cg_var(*g, npv_ + "_0");

        if (useExternalComputeFramework_) {
            if (newExternalCalc) {
                forwardEvaluation(*g, valuesExternal, opsExternal_, ExternalRandomVariable::deleter, useCachedSensis_,
                                  opNodeRequirements_, keepNodes);
                valuesExternal[baseNpvNode].declareAsOutput();
                externalOutput_ = std::vector<std::vector<double>>(1, std::vector<double>(model_->size()));
                externalOutputPtr_ = std::vector<double*>(1, &externalOutput_.front()[0]);
                DLOG("ran forward evaluation");
            }
        } else {
            forwardEvaluation(*g, values, ops_, RandomVariable::deleter, useCachedSensis_, opNodeRequirements_,
                              keepNodes);
            DLOG("ran forward evaluation");
        }

        TLOGGERSTREAM(ssaForm(*g, getRandomVariableOpLabels(), values));

        // extract npv result and set it

        if (useExternalComputeFramework_) {
            ComputeEnvironment::instance().context().finalizeCalculation(externalOutputPtr_);
            baseNpv_ = results_.value = externalAverage(externalOutput_[0]);
        } else {
            baseNpv_ = results_.value = model_->extractT0Result(values[baseNpvNode]);
        }

        DLOG("got NPV = " << results_.value << " " << model_->baseCcy());

        // extract additional results

        if (generateAdditionalResults_) {

            instrumentAdditionalResults_.clear();

            for (auto const& r : additionalResults_) {

                auto s = workingContext_->scalars.find(r.second);
                bool resultSet = false;
                if (s != workingContext_->scalars.end()) {
                    if (s->second.which() == ValueTypeWhich::Number) {
                        instrumentAdditionalResults_[r.first] =
                            model_->extractT0Result(values[cg_var(*g, r.second + "_0")]);
                    } else {
                        boost::any t = valueToAnyCg(s->second);
                        instrumentAdditionalResults_[r.first] = t;
                    }
                    DLOG("got additional result '" << r.first << "' referencing script variable '" << r.second << "'");
                    resultSet = true;
                }
                auto v = workingContext_->arrays.find(r.second);
                if (v != workingContext_->arrays.end()) {
                    QL_REQUIRE(!resultSet, "result variable '"
                                               << r.first << "' referencing script variable '" << r.second
                                               << "' appears both as a scalar and an array, this is unexpected");
                    QL_REQUIRE(!v->second.empty(), "result variable '" << v->first << "' is an empty array.");
                    std::vector<double> tmpdouble;
                    std::vector<std::string> tmpstring;
                    std::vector<QuantLib::Date> tmpdate;
                    for (Size i = 0; i < v->second.size(); ++i) {
                        if (v->second[i].which() == ValueTypeWhich::Number) {
                            tmpdouble.push_back(
                                model_->extractT0Result(values[cg_var(*g, r.second + "_" + std::to_string(i))]));
                        } else {
                            boost::any t = valueToAnyCg(v->second[i]);
                            if (t.type() == typeid(std::string))
                                tmpstring.push_back(boost::any_cast<std::string>(t));
                            else if (t.type() == typeid(QuantLib::Date))
                                tmpdate.push_back(boost::any_cast<QuantLib::Date>(t));
                            else {
                                QL_FAIL("unexpected result type '" << t.type().name() << "' for result variable '"
                                                                   << r.first << "' referencing script variable '"
                                                                   << r.second << "'");
                            }
                        }
                    }
                    QL_REQUIRE((int)!tmpdouble.empty() + (int)!tmpstring.empty() + (int)!tmpdate.empty() == 1,
                               "expected exactly one result type in result array '" << v->first << "'");
                    DLOG("got additional result '" << r.first << "' referencing script variable '" << r.second
                                                   << "' vector of size "
                                                   << tmpdouble.size() + tmpstring.size() + tmpdate.size());
                    if (!tmpdouble.empty())
                        instrumentAdditionalResults_[r.first] = tmpdouble;
                    else if (!tmpstring.empty())
                        instrumentAdditionalResults_[r.first] = tmpstring;
                    else if (!tmpdate.empty())
                        instrumentAdditionalResults_[r.first] = tmpdate;
                    else {
                        QL_FAIL("got empty result vector for result variable '"
                                << r.first << "' referencing script variable '" << r.second << "', this is unexpected");
                    }
                    resultSet = true;
                }
                QL_REQUIRE(resultSet, "could not set additional result '"
                                          << r.first << "' referencing script variable '" << r.second << "'");
            }

            // set contents from paylog as additional results

            auto paylog = QuantLib::ext::make_shared<PayLog>();
            for (auto const& p : payLogEntries_) {
                paylog->write(values[p.value],
                              !close_enough(values[p.filter], RandomVariable(values[p.filter].size(), 0.0)), p.obs,
                              p.pay, p.ccy, p.legNo, p.cashflowType, p.slot);
            }

            paylog->consolidateAndSort();
            std::vector<CashFlowResults> cashFlowResults(paylog->size());
            for (Size i = 0; i < paylog->size(); ++i) {
                // cashflow is written as expectation of deflated base ccy amount at T0, converted to flow ccy
                // with the T0 FX Spot and compounded back to the pay date on T0 curves
                Real fx = 1.0;
                Real discount = 1.0;
                if (paylog->dates().at(i) > model_->referenceDate()) {
                    fx = model_->getDirectFxSpotT0(paylog->currencies().at(i), model_->baseCcy());
                    discount = model_->getDirectDiscountT0(paylog->dates().at(i), paylog->currencies().at(i));
                }
                cashFlowResults[i].amount = model_->extractT0Result(paylog->amounts().at(i)) / fx / discount;
                cashFlowResults[i].payDate = paylog->dates().at(i);
                cashFlowResults[i].currency = paylog->currencies().at(i);
                cashFlowResults[i].legNumber = paylog->legNos().at(i);
                cashFlowResults[i].type = paylog->cashflowTypes().at(i);
                DLOG("got cashflow " << QuantLib::io::iso_date(cashFlowResults[i].payDate) << " "
                                     << cashFlowResults[i].currency << cashFlowResults[i].amount << " "
                                     << cashFlowResults[i].currency << "-" << model_->baseCcy() << " " << fx
                                     << "discount(" << cashFlowResults[i].currency << ") " << discount);
            }
            instrumentAdditionalResults_["cashFlowResults"] = cashFlowResults;

            // set additional results from the model

            instrumentAdditionalResults_.insert(model_->additionalResults().begin(), model_->additionalResults().end());

        } // if generate additional results

        if (useCachedSensis_) {

            // extract sensis and store them

            std::vector<RandomVariable> derivatives(g->size(), RandomVariable(model_->size(), 0.0));
            derivatives[cg_var(*g, npv_ + "_0")] = RandomVariable(model_->size(), 1.0);
            backwardDerivatives(*g, values, derivatives, grads_, RandomVariable::deleter, keepNodes);

            sensis_.resize(baseModelParams_.size());
            for (Size i = 0; i < baseModelParams_.size(); ++i) {
                sensis_[i] = model_->extractT0Result(derivatives[baseModelParams_[i].first]);
            }
            DLOG("got backward sensitivities");

            // set flag indicating that we can use cached sensis in subsequent calculations

            haveBaseValues_ = true;
        }

    } else {

        // useCachedSensis => calculate npv from stored base npv, sensis, model params

        auto modelParams = model_->modelParameters();

        double npv = baseNpv_;
        DLOG("computing npv using baseNpv " << baseNpv_ << " and sensis.");

        for (Size i = 0; i < baseModelParams_.size(); ++i) {
            QL_REQUIRE(modelParams[i].first == baseModelParams_[i].first, "internal error: modelParams["
                                                                              << i << "] node " << modelParams[i].first
                                                                              << " does not match baseModelParams node "
                                                                              << baseModelParams_[i].first);
            Real tmp = sensis_[i] * (modelParams[i].second - baseModelParams_[i].second);
            npv += tmp;
            DLOG("node " << modelParams[i].first << ": [" << modelParams[i].second << " (current) - "
                         << baseModelParams_[i].second << " (base) ] * " << sensis_[i] << " (delta) => " << tmp);
        }

        results_.value = npv;
    }

    if (generateAdditionalResults_) {
        results_.additionalResults = instrumentAdditionalResults_;
    }

    lastCalculationWasValid_ = true;
}

} // namespace data
} // namespace ore
