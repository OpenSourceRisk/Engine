/*
 Copyright (C) 2025 Quaternion Risk Management Ltd

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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondfuture.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bondfuture.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/indexnametranslator.hpp>

#include <qle/instruments/forwardbond.hpp>
#include <qle/instruments/bondfuture.hpp>

#include <ql/currencies/america.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/calendars/china.hpp>
#include <ql/time/calendars/unitedstates.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace data {

void BondFuture::build(const ext::shared_ptr<EngineFactory>& engineFactory)
{
    DLOG("BondFuture::build() called for trade " << id());

    BondFutureUtils::addIsdaTaxonomy(additionalData_);
    bool isLong = parsePositionType(longShort_) == QuantLib::Position::Type::Long;

    // Get the pricing engine builder for bond future.
    ext::shared_ptr<EngineBuilder> engineBuilder = engineFactory->builder("BondFuture");
    QL_REQUIRE(engineBuilder, "BondFuture::build: no engine builder found for type BondFuture.");
    auto bfEngineBuilder = ext::dynamic_pointer_cast<BondFutureEngineBuilder>(engineBuilder);
    QL_REQUIRE(bfEngineBuilder, "BondFuture::build: engine builder for type BondFuture cannot "
        "be cast to a BondFutureEngineBuilder.");

    // Get the bond future pricing engine. Only after call to engine(...), are indexResults() below available.
    auto bfEngine = bfEngineBuilder->engine(contractName_);

    // Information gathered during the creation of the bond future index for contractName_.
    const auto& indexResults = bfEngineBuilder->indexResults();
    refData_ = indexResults.refData;
    bondData_ = indexResults.ctdBuilderResult.bondData;

    // Create the bond future instrument.
    const auto& bondFutureData = refData_->bondFutureData();
    bool physicalSettle = bondFutureData.settlement == "Physical";
    auto instr = QuantLib::ext::make_shared<QuantExt::BondFuture>(
        indexResults.index, contractNotional_, isLong, indexResults.futureSettle, physicalSettle);

    // Set its pricing engine.
    instr->setPricingEngine(bfEngine);

    Date today = Settings::instance().evaluationDate();
    string oreIndexName = IndexNameTranslator::instance().oreName(indexResults.index->name());
    requiredFixings_.addFixingDate(today, oreIndexName, indexResults.futureSettle);

    setSensitivityTemplate(*bfEngineBuilder);
    addProductModelEngine(*bfEngineBuilder);
    instrument_ = ext::make_shared<VanillaInstrument>(instr, 1.0);

    maturity_ = indexResults.futureSettle;
    maturityType_ = "Contract settled";
    npvCurrency_ = bondFutureData.currency;
    notional_ = contractNotional_;
    legs_ = vector<Leg>(1, indexResults.ctdBuilderResult.bond->cashflows());
    legCurrencies_ = vector<string>(1, bondFutureData.currency);
    legPayers_ = vector<bool>(1, isLong);
}

void BondFuture::fromXML(XMLNode* node)
{
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "BondFuture::fromXML(): no BondFutureData Node");
    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    contractNotional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "ContractNotional", true);
    longShort_ = XMLUtils::getChildValue(bondFutureNode, "LongShort", true);
    if (auto n = XMLUtils::getChildNode(bondFutureNode, "ApplyConversionFactor"))
        applyConversionFactor_ = parseBool(XMLUtils::getNodeValue(n));
    if (auto n = XMLUtils::getChildNode(bondFutureNode, "UseFuturePrice"))
        useFuturePrice_ = parseBool(XMLUtils::getNodeValue(n));
}

XMLNode* BondFuture::toXML(XMLDocument& doc) const
{
    XMLNode* node = Trade::toXML(doc);
    XMLNode* bondFutureNode = doc.allocNode("BondFutureData");
    XMLUtils::addChild(doc, bondFutureNode, "ContractName", contractName_);
    XMLUtils::addChild(doc, bondFutureNode, "ContractNotional", contractNotional_);
    XMLUtils::addChild(doc, bondFutureNode, "LongShort", longShort_);
    if (applyConversionFactor_)
        XMLUtils::addChild(doc, bondFutureNode, "ApplyConversionFactor", *applyConversionFactor_);
    if (useFuturePrice_)
        XMLUtils::addChild(doc, bondFutureNode, "UseFuturePrice", *useFuturePrice_);
    XMLUtils::appendNode(node, bondFutureNode);
    return node;
}

bool BondFuture::applyConversionFactor() const
{
    return applyConversionFactor_.value_or(true);
}

bool BondFuture::useFuturePrice() const
{
    return useFuturePrice_.value_or(false);
}

map<AssetClass, set<string>> BondFuture::underlyingIndices(
    const ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const
{
    return BondFutureUtils::underlyingBondIndices(contractName_, referenceDataManager);
}

} // namespace data
} // namespace ore
