/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/bondrepo.hpp>
#include <ored/portfolio/builders/bondrepo.hpp>
#include <qle/instruments/bondrepo.hpp>

#include <ored/portfolio/bondutils.hpp>

#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/log.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void BondRepo::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondRepo::build() called for trade " << id());

    // get bond repo engine builder

    auto builder = QuantLib::ext::dynamic_pointer_cast<BondRepoEngineBuilderBase>(engineFactory->builder("BondRepo"));
    QL_REQUIRE(builder, "BondRepo::build(): engine builder is null");

    // build security leg (as a bond)

    securityLegData_ = originalSecurityLegData_;
    securityLegData_.populateFromBondReferenceData(engineFactory->referenceData());
    securityLeg_ = QuantLib::ext::make_shared<ore::data::Bond>(Envelope(), securityLegData_);
    securityLeg_->id() = id() + "_SecurityLeg";
    securityLeg_->build(engineFactory);
    QL_REQUIRE(!securityLeg_->legs().empty(), "BondRepo::build(): security leg has no cashflows");

    // build cash leg

    auto configuration = builder->configuration(MarketContext::pricing);
    auto legBuilder = engineFactory->legBuilder(cashLegData_.legType());
    cashLeg_ = legBuilder->buildLeg(cashLegData_, engineFactory, requiredFixings_, configuration);

    // add notional payment

    QL_REQUIRE(!cashLeg_.empty(), "BondRepo::build(): cash leg empty");
    auto lastCpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(cashLeg_.back());
    QL_REQUIRE(lastCpn, "BondRepo::build(): expected coupon on cash leg");
    cashLeg_.push_back(QuantLib::ext::make_shared<QuantLib::SimpleCashFlow>(lastCpn->nominal(), lastCpn->date()));

    // add required fixings from bond

    requiredFixings_.addData(securityLeg_->requiredFixings());

    // set trade members

    npvCurrency_ = cashLegData_.currency();
    notionalCurrency_ = cashLegData_.currency();
    maturity_ = CashFlows::maturityDate(cashLeg_);
    notional_ = currentNotional(cashLeg_);

    // start with the cashleg's legs
    legs_ = {cashLeg_};
    legCurrencies_ = {cashLegData_.currency()};
    legPayers_ = {cashLegData_.isPayer()};

    // add security legs to trade legs (should be 1 leg only, but to be safe we copy all legs in the trade)
    legs_.insert(legs_.end(), securityLeg_->legs().begin(), securityLeg_->legs().end());
    legCurrencies_.insert(legCurrencies_.end(), securityLeg_->legCurrencies().begin(),
                          securityLeg_->legCurrencies().end());
    std::vector<bool> securityLegPayers(securityLeg_->legs().size(), !cashLegData_.isPayer());
    legPayers_.insert(legPayers_.end(), securityLegPayers.begin(), securityLegPayers.end());

    QL_REQUIRE(cashLegData_.currency() == securityLeg_->bondData().currency(),
               "BondRepo: cash leg currency (" << cashLegData_.currency() << ") must match security leg currency ("
                                               << securityLeg_->bondData().currency() << ")");

    auto qlBondInstr = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(securityLeg_->instrument()->qlInstrument());
    QL_REQUIRE(qlBondInstr, "BondRepo: could not cast to QuantLib::Bond instrument, this is unexpected");
    auto qlInstr = QuantLib::ext::make_shared<QuantExt::BondRepo>(cashLeg_, cashLegData_.isPayer(), qlBondInstr,
                                                          std::abs(securityLeg_->instrument()->multiplier()));

    qlInstr->setPricingEngine(builder->engine(securityLegData_.incomeCurveId()));
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlInstr);
    setSensitivityTemplate(*builder);

    // set additionalData
    additionalData_["underlyingSecurityId"] = securityLegData_.securityId();
    additionalData_["legType[1]"] = string("Cash");
    additionalData_["currentNotional[1]"] = notional_;
    additionalData_["originalNotional[1]"] = originalNotional(cashLeg_);
    additionalData_["notionalCurrency[1]"] = notionalCurrency_;
    additionalData_["legType[2]"] = string("Security");
    additionalData_["originalNotional[2]"] = securityLegData_.bondNotional();
    additionalData_["currentNotional[2]"] = currentNotional(qlBondInstr->cashflows()) * securityLegData_.bondNotional();
    additionalData_["notionalCurrency[2]"] = securityLegData_.currency();
}

void BondRepo::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* dataNode = XMLUtils::getChildNode(node, "BondRepoData");
    QL_REQUIRE(dataNode, "BondRepoData node not found");
    XMLNode* bondNode = XMLUtils::getChildNode(dataNode, "BondData");
    QL_REQUIRE(bondNode, "BondData node not found");
    originalSecurityLegData_.fromXML(bondNode);
    securityLegData_ = originalSecurityLegData_;

    XMLNode* repoNode = XMLUtils::getChildNode(dataNode, "RepoData");
    QL_REQUIRE(repoNode, "RepoData node not found");

    XMLNode* legNode = XMLUtils::getChildNode(repoNode, "LegData");
    QL_REQUIRE(legNode, "LegData node not found");
    cashLegData_.fromXML(legNode);
}

XMLNode* BondRepo::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode("BondRepoData");
    XMLUtils::appendNode(node, dataNode);

    XMLUtils::appendNode(dataNode, originalSecurityLegData_.toXML(doc));

    XMLNode* repoNode = doc.allocNode("RepoData");
    XMLUtils::appendNode(dataNode, repoNode);
    XMLUtils::appendNode(repoNode, cashLegData_.toXML(doc));

    return node;
}

std::map<AssetClass, std::set<std::string>>
BondRepo::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {securityLegData_.securityId()};
    return result;
}

} // namespace data
} // namespace ore
