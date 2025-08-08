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

namespace ore {
namespace data {

void BondFuture::build(const ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondFuture::build() called for trade " << id());

    // ISDA taxonomy https://www.isda.org/a/20EDE/q4-2011-credit-standardisation-legend.pdf
    // TODO: clarify ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    bool isLong = parsePositionType(longShort_) == QuantLib::Position::Type::Long;

    QL_REQUIRE(engineFactory->referenceData()->hasData("BondFuture", contractName_),
               "BondFutureUtils::identifyCtdBond(): no bond future reference data found for " << contractName_);

    auto refData = QuantLib::ext::dynamic_pointer_cast<BondFutureReferenceDatum>(
        engineFactory->referenceData()->getData("BondFuture", contractName_));

    auto builder = QuantLib::ext::dynamic_pointer_cast<BondFutureEngineBuilder>(engineFactory->builder("BondFuture"));

    bool pricing =
        builder->globalParameters().count("Calibrate") == 0 || parseBool(builder->globalParameters().at("Calibrate"));

    auto [ctd, conversionFactor] = BondFutureUtils::identifyCtdBond(engineFactory, contractName_, pricing);
    auto [expiry, settlement] = BondFutureUtils::deduceDates(refData);

    auto b = BondFactory::instance().build(engineFactory, engineFactory->referenceData(),
                                           StructuredSecurityId(ctd, contractName_));
    auto index = QuantLib::ext::make_shared<QuantExt::BondFuturesIndex>(
        contractName_, expiry, b.bond, parseBool(refData->bondFutureData().dirtyQuotation));
    auto instr = QuantLib::ext::make_shared<QuantExt::BondFuture>(index, contractNotional_, isLong, settlement,
                                                                  refData->bondFutureData().settlement == "Physical");

    bondData_ = b.bondData;

    instr->setPricingEngine(builder->engine(id(), refData->bondFutureData().currency, conversionFactor));

    requiredFixings_.addFixingDate(index->futureExpiryDate(), IndexNameTranslator::instance().oreName(index->name()),
                                   settlement);

    setSensitivityTemplate(*builder);
    addProductModelEngine(*builder);
    instrument_.reset(new VanillaInstrument(instr, 1.0));

    maturity_ = settlement;
    maturityType_ = "Contract settled";
    npvCurrency_ = refData->bondFutureData().currency;
    notional_ = contractNotional_;
    legs_ = vector<Leg>(1, b.bond->cashflows());
    legCurrencies_ = vector<string>(1, refData->bondFutureData().currency);
    legPayers_ = vector<bool>(1, isLong);
}

void BondFuture::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "BondFuture::fromXML(): no BondFutureData Node");
    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    contractNotional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "ContractNotional", true);
    longShort_ = XMLUtils::getChildValue(bondFutureNode, "LongShort", true);
}

XMLNode* BondFuture::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BondFutureData");
    XMLUtils::addChild(doc, node, "ContractName", contractName_);
    XMLUtils::addChild(doc, node, "ContractNotional", contractNotional_);
    XMLUtils::addChild(doc, node, "LongShort", longShort_);
    return node;
}

} // namespace data
} // namespace ore
