/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/portfolio/bondfutureoption.hpp>
#include <ored/portfolio/bondfuture.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bondfutureoption.hpp>
#include <qle/instruments/bondfutureoption.hpp>
#include <regex>

namespace ore {
namespace data {

using namespace QuantLib;
using std::map;
using std::set;
using std::string;

namespace {

bool matchesViaBoolOrRegex(const string& value, const string& futureContract, const string& paramName) {
    // Try bool first. If parsing of string `value` as bool succeeds then return the parsed bool result.
    bool result = false;
    bool isBool = tryParse<bool>(value, result, parseBool);
    if (isBool)
        return result;

    // If parsing as bool failed, try regex.
    try {
        std::regex futureIncPattern{ value };
        result = std::regex_match(futureContract, futureIncPattern);
    } catch (const std::regex_error& e) {
        WLOG("BondFutureOption: Invalid regex for parameter " << paramName << ": " << value <<
            ". Error: " << e.what() << ". Parameter will be ignored.");
    }

    return result;
}

// Helper to determine option type suffix for engine builder.
// May want to use separate volatility surface for calls and puts. This can be determined from the engine 
// parameters and also on the trade level via the envelope (trade level wins if specified). If this is the case, 
// then separate engines are attached to the call and put options on a given underlying contract.
string getOptionTypeSuffix(const BondFutureOptionEngineBuilder& bfoEngineBuilder, const string& futureContract,
    const Envelope& envelope, Option::Type type) {

    // Default is false.
    bool separateCallPutVols = false;

    // Check the engine builder for the string.
    string strSeparateCallPutVols = bfoEngineBuilder.engineParameter("SeparateCallPutVols", {}, false, "");

    // If provided in the trade envelope, this overrides the engine parameter.
    string strEnvSeparateCallPutVols = envelope.additionalField("SeparateCallPutVols", false, "");
    if (!strEnvSeparateCallPutVols.empty())
        strSeparateCallPutVols = strEnvSeparateCallPutVols;

    // If we have a non-empty string, try to parse it. It can be:
    // 1. a global setting of true or false
    // 2. a regex that matches the future contract name.
    separateCallPutVols = matchesViaBoolOrRegex(strSeparateCallPutVols, futureContract, "SeparateCallPutVols");

    // Set the suffix if specified in engine builder or in trade envelope.
    string optTypeSuffix;
    if (separateCallPutVols)
        optTypeSuffix = type == Option::Call ? "CALL" : "PUT";

    return optTypeSuffix;
}

// Check if we are overriding the exercise type on options for this bond future contract.
ext::optional<Exercise::Type> getExerciseTypeOverride(const ext::shared_ptr<EngineFactory>& engineFactory,
    const string& futureContract) {

    // The default if no exercise type override is configured i.e. uninitialised optional.
    ext::optional<Exercise::Type> result;

    // If BondFutureAmericanAsEuropean is not in the global engine parameters, then there is no override.
    const auto& globalEngineParams = engineFactory->engineData()->globalParameters();
    auto itGlobal = globalEngineParams.find("BondFutureAmericanAsEuropean");
    if (itGlobal == globalEngineParams.end() || itGlobal->second.empty())
        return result;

    // We have BondFutureAmericanAsEuropean. It can be:
    // 1. a global setting of true or false
    // 2. a regex that matches the future contract name.
    if (matchesViaBoolOrRegex(itGlobal->second, futureContract, "BondFutureAmericanAsEuropean"))
        result = Exercise::Type::European;

    return result;
}

}

BondFutureOption::BondFutureOption()
    : VanillaOptionTrade("BondFutureOption", AssetClass::BOND) {}

BondFutureOption::BondFutureOption(Envelope& env,
    OptionData optionData,
    std::string futureContractName,
    QuantLib::Real futureContractNotional,
    QuantLib::Real strikePrice)
    : VanillaOptionTrade("BondFutureOption", env, AssetClass::BOND, optionData, futureContractName, "",
        futureContractNotional, TradeStrike(TradeStrike::Type::Price, strikePrice)) {}

void BondFutureOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory)
{
    DLOG("BondFutureOption::build called for trade " << id());

    BondFutureUtils::addIsdaTaxonomy(additionalData_);

    // If pricing engine has been configured to treat bond future options as European, then override the exercise type
    // to European regardless of what is specified on the trade.
    const string& futureContract = asset();
    ext::optional<Exercise::Type> exTypeOverride = getExerciseTypeOverride(engineFactory, futureContract);
    auto [exerciseType, exercise] = exerciseDetails(exTypeOverride);
    string builderTradeType = exerciseType == Exercise::Type::American ? tradeType_ + "American" : tradeType_;

    // Payoff
    auto [type, payoff] = payoffDetails();

    // Create the main instrument.
    ext::shared_ptr<Instrument> option = ext::make_shared<QuantExt::BondFutureOption>(payoff, exercise);

    // Get the pricing engine builder for bond future option (depends on exercise type, from above).
    ext::shared_ptr<EngineBuilder> engineBuilder = engineFactory->builder(builderTradeType);
    QL_REQUIRE(engineBuilder, "BondFutureOption::build: no engine builder found for type " << builderTradeType << ".");
    auto bfoEngineBuilder = ext::dynamic_pointer_cast<BondFutureOptionEngineBuilder>(engineBuilder);
    QL_REQUIRE(bfoEngineBuilder, "BondFutureOption::build: engine builder for type " << builderTradeType <<
        " cannot be cast to a BondFutureOptionEngineBuilder.");

    // Set the bond future option pricing engine. Note: asset() gives the future contract name here.
    string optTypeSuffix = getOptionTypeSuffix(*bfoEngineBuilder, futureContract, envelope(), type);
    option->setPricingEngine(bfoEngineBuilder->engine(futureContract, optTypeSuffix, expiryDate_));

    // Set some Trade specific data.
    setSensitivityTemplate(*bfoEngineBuilder);
    addProductModelEngine(*bfoEngineBuilder);
    // Information gathered during the creation of the bond future index.
    const auto& indexResults = bfoEngineBuilder->indexResults();
    currency_ = indexResults.refData->bondFutureData().currency;
    if (option_.settlement().empty())
        option_.setSettlement("Physical");
    setNotionalAndCurrencies();

    // Store the bond data for the CTD bond.
    bondData_ = indexResults.ctdBuilderResult.bondData;

    // Add to the additional data to make available in additional results report.
    additionalData_["CTDBond"] = indexResults.ctdSecurityId;
    additionalData_["OptionType"] = to_string(type);
    additionalData_["OptionExpiry"] = to_string(io::iso_date(expiryDate_));
    additionalData_["ContractNotional"] = quantity_;

    // Create the wrapper instrument. It is unlikely that we will have a premium with bond futures but we re-use the 
    // logic just in case. The discount curve is set to "" as we are fine with the market discount curve in the premium 
    // currency if there is a premium.
    string configuration = bfoEngineBuilder->configuration(MarketContext::pricing);
    setInstrumentWrapper(option, "", parseCurrency(currency_), configuration, engineFactory);
}

void BondFutureOption::fromXML(XMLNode* node)
{
    VanillaOptionTrade::fromXML(node);
    XMLNode* bfoNode = XMLUtils::getChildNode(node, "BondFutureOptionData");
    QL_REQUIRE(bfoNode, "BondFutureOption::fromXML: expected a BondFutureOptionData node.");
    option_.fromXML(XMLUtils::getChildNode(bfoNode, "OptionData"));
    option_.setPayoffAtExpiry(false);
    assetName_ = XMLUtils::getChildValue(bfoNode, "ContractName", true);
    quantity_ = XMLUtils::getChildValueAsDouble(bfoNode, "ContractNotional", true);
    strike_.fromXML(bfoNode);
}

XMLNode* BondFutureOption::toXML(XMLDocument& doc) const
{
    XMLNode* node = VanillaOptionTrade::toXML(doc);
    XMLNode* bfoNode = doc.allocNode("BondFutureOptionData");
    XMLUtils::appendNode(node, bfoNode);
    XMLUtils::appendNode(bfoNode, option_.toXML(doc));
    XMLUtils::addChild(doc, bfoNode, "ContractName", assetName_);
    XMLUtils::addChild(doc, bfoNode, "ContractNotional", quantity_);
    XMLUtils::appendNode(bfoNode, strike_.toXML(doc));
    return node;
}

map<AssetClass, set<string>> BondFutureOption::underlyingIndices(
    const ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const
{
    return BondFutureUtils::underlyingBondIndices(asset(), referenceDataManager);
}

} // namespace data
} // namespace ore
