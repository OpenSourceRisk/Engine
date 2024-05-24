/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

#include <ored/portfolio/ascot.hpp>
#include <ored/portfolio/builders/ascot.hpp>

#include <ored/utilities/parsers.hpp>

#include <ql/cashflows/cashflows.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

std::map<AssetClass, std::set<std::string>>
Ascot::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return bond_.underlyingIndices(referenceDataManager);
}

void Ascot::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    DLOG("Ascot::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("Other");  
    additionalData_["isdaTransaction"] = string("");  

    // build underlying convertible bond
    bond_.reset();
    // we need to do set the id manually because it otherwise remains blank
    bond_.id() = id() + "_Bond";
    bond_.build(engineFactory);
    requiredFixings_.addData(bond_.requiredFixings());
    QuantLib::ext::shared_ptr<QuantExt::ConvertibleBond2> cb =
        QuantLib::ext::dynamic_pointer_cast<QuantExt::ConvertibleBond2>(bond_.instrument()->qlInstrument());

    // check option fields
    Exercise::Type exerciseType = parseExerciseType(optionData_.style());
    QL_REQUIRE(exerciseType == Exercise::American, "expected American exercise type");
    QL_REQUIRE(optionData_.exerciseDates().size() == 1,
               "Ascot::build(): exactly one option date required, found " << optionData_.exerciseDates().size());
    Date exerciseDate = parseDate(optionData_.exerciseDates().back());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<AmericanExercise>(exerciseDate);
    Option::Type type = parseOptionType(optionData_.callPut());

    // build funding leg
    // Payer should be false,
    // i.e. the swap is entered from the viewpoint of the asset swap buyer
    QL_REQUIRE(fundingLegData_.isPayer() == false, "expected isPayer == false for funding leg");
    Leg fundingLeg;

    auto builder = QuantLib::ext::dynamic_pointer_cast<AscotEngineBuilder>(engineFactory->builder("Ascot"));
    auto configuration = builder->configuration(MarketContext::pricing);
    auto legBuilder = engineFactory->legBuilder(fundingLegData_.legType());
    fundingLeg = legBuilder->buildLeg(fundingLegData_, engineFactory, requiredFixings_, configuration);

    QL_REQUIRE(builder, "Ascot::build(): could not cast to AscotBuilder, this is unexpected");

    auto qlAscot =
        QuantLib::ext::make_shared<QuantExt::Ascot>(type, exercise, bond_.data().bondData().bondNotional(), cb, fundingLeg);
    qlAscot->setPricingEngine(builder->engine(id(), bond_.data().bondData().currency()));
    setSensitivityTemplate(*builder);

    Real multiplier = (parsePositionType(optionData_.longShort()) == Position::Long ? 1.0 : -1.0);
    instrument_ =
        QuantLib::ext::shared_ptr<ore::data::InstrumentWrapper>(new ore::data::VanillaInstrument(qlAscot, multiplier));

    npvCurrency_ = notionalCurrency_ = bond_.notionalCurrency();
    legs_ = {cb->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {parsePositionType(optionData_.longShort()) == Position::Long};

    notional_ = bond_.data().bondData().bondNotional();
    maturity_ = bond_.maturity();
}

void Ascot::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, "AscotData");
    QL_REQUIRE(dataNode, "AscotData node not found");

    ConvertibleBondData bondData;
    bondData.fromXML(XMLUtils::getChildNode(dataNode, "ConvertibleBondData"));
    bond_ = ConvertibleBond(envelope(), bondData);

    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));

    XMLNode* tmpNode = XMLUtils::getChildNode(dataNode, "ReferenceSwapData");
    QL_REQUIRE(tmpNode, "ReferenceSwapData node not found");
    XMLNode* legNode = XMLUtils::getChildNode(tmpNode, "LegData");
    QL_REQUIRE(legNode, "LegData node not found");
    fundingLegData_.fromXML(legNode);
}

XMLNode* Ascot::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode("AscotData");
    XMLUtils::appendNode(node, dataNode);

    ConvertibleBondData d = bond_.data();
    XMLUtils::appendNode(dataNode, d.toXML(doc));
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));

    XMLNode* fundingDataNode = doc.allocNode("ReferenceSwapData");
    XMLUtils::appendNode(dataNode, fundingDataNode);
    XMLUtils::appendNode(fundingDataNode, fundingLegData_.toXML(doc));

    return node;
}

} // namespace data
} // namespace ore
