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

#include <ored/scripting/models/blackscholes.hpp>
#include <ored/model/blackscholesmodelbuilder.hpp>
#include <ored/scripting/engines/cliquetoptionmcscriptengine.hpp>
#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

CliquetOptionMcScriptEngine::CliquetOptionMcScriptEngine(const std::string& underlying, const std::string& baseCcy,
                                                         const std::string& underlyingCcy,
                                                         const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& p,
                                                         const std::set<std::string>& tradeTypes, Size samples,
                                                         Size regressionOrder, bool interactive,
                                                         bool scriptedLibraryOverride)
    : underlying_(underlying), baseCcy_(baseCcy), underlyingCcy_(underlyingCcy), p_(p), samples_(samples),
      regressionOrder_(regressionOrder), interactive_(interactive) {

    string scriptStr;

    if (scriptedLibraryOverride) {
        QL_REQUIRE(tradeTypes.size() == 1, "");
        string scriptName = *tradeTypes.begin();

        QL_REQUIRE(ScriptLibraryStorage::instance().get().has(scriptName, ""),
                   "script '" << scriptName << "' not found in library");
        ScriptedTradeScriptData script = ScriptLibraryStorage::instance().get().get(scriptName, "").second;
        scriptStr = script.code();
    } else {
        scriptStr = "NUMBER Payoff, d, premiumPayment;\n"
                    "Payoff = 0;\n"
                    "premiumPayment = 0;\n"
                    "IF PremiumPaymentDate >= TODAY THEN"
                    "    premiumPayment = PAY(LongShort * Notional * Premium, PremiumPaymentDate, PremiumPaymentDate, "
                    "PremiumCurrency);"
                    "END;\n"
                    "FOR d IN (2, SIZE(ValuationDates), 1) DO"
                    "   Payoff = Payoff + min( max( Type * ( (Underlying(ValuationDates[d]) / "
                    "Underlying(ValuationDates[d-1])) - Moneyness ), LocalFloor ), LocalCap );"
                    "END;\n"
                    "Option = premiumPayment + PAY( LongShort * Notional * min( max( Payoff, GlobalFloor ), GlobalCap "
                    "), Expiry, PayDate, PayCcy );\n";
    }

    ScriptParser parser(scriptStr);
    QL_REQUIRE(parser.success(), "could not initialise AST for McScriptEuropeanEngine: " << parser.error());
    ast_ = parser.ast();
    registerWith(p_);
}

void CliquetOptionMcScriptEngine::calculate() const {

    // some checks (copied from QuantLib::AnalyticEuropeanEngine)

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff, "non-striked payoff given");

    // set up the script engine model

    // cas use time steps = 0 for black scholes
    auto builder = QuantLib::ext::make_shared<BlackScholesModelBuilder>(p_->riskFreeRate(), p_, arguments_.valuationDates,
                                                                std::set<Date>(), 0);
    // the black scholes model wrapper won't notify the model of changes in curves and vols, so we register manually
    builder->model()->registerWith(p_);
    Model::McParams mcParams;
    mcParams.regressionOrder = regressionOrder_;
    auto model = QuantLib::ext::make_shared<BlackScholes>(samples_, baseCcy_, p_->riskFreeRate(), underlying_, underlyingCcy_,
                                                  builder->model(), mcParams, arguments_.valuationDates);

    // populate context

    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["TODAY"] = EventVec{samples_, Settings::instance().evaluationDate()};
    context->scalars["Underlying"] = IndexVec{samples_, underlying_};
    std::vector<ValueType> tmp;
    for (auto const& d : arguments_.valuationDates)
        tmp.push_back(EventVec{samples_, d});
    context->arrays["ValuationDates"] = tmp;
    context->scalars["Expiry"] = EventVec{samples_, arguments_.exercise->lastDate()};
    context->scalars["PayCcy"] = CurrencyVec{samples_, baseCcy_};
    context->scalars["PayDate"] = EventVec{samples_, arguments_.paymentDate};
    context->scalars["Moneyness"] = RandomVariable{samples_, arguments_.moneyness};
    context->scalars["Type"] = RandomVariable{samples_, arguments_.type == Option::Type::Call ? 1.0 : -1.0};
    context->scalars["LongShort"] =
        RandomVariable{samples_, arguments_.longShort == QuantLib::Position::Type::Long ? 1.0 : -1.0};
    context->scalars["LocalCap"] =
        RandomVariable{samples_, arguments_.localCap == Null<Real>() ? QL_MAX_REAL : arguments_.localCap};
    context->scalars["LocalFloor"] =
        RandomVariable{samples_, arguments_.localFloor == Null<Real>() ? -QL_MAX_REAL : arguments_.localFloor};
    context->scalars["GlobalCap"] =
        RandomVariable{samples_, arguments_.globalCap == Null<Real>() ? QL_MAX_REAL : arguments_.globalCap};
    context->scalars["GlobalFloor"] =
        RandomVariable{samples_, arguments_.globalFloor == Null<Real>() ? -QL_MAX_REAL : arguments_.globalFloor};
    context->scalars["Notional"] = RandomVariable{samples_, arguments_.notional};
    context->scalars["Premium"] =
        RandomVariable{samples_, arguments_.premium == Null<Real>() ? 0.0 : arguments_.premium};
    context->scalars["PremiumPaymentDate"] = EventVec{samples_, arguments_.premiumPayDate};
    context->scalars["PremiumCurrency"] = CurrencyVec{samples_, arguments_.premiumCurrency};
    context->scalars["Option"] = RandomVariable(samples_, 0.0); // result

    // run the script engine and set the result

    ScriptEngine engine(ast_, context, model);
    engine.run("", interactive_);
    results_.value = expectation(QuantLib::ext::get<RandomVariable>(context->scalars.at("Option"))).at(0);
}

} // namespace data
} // namespace ore
