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

/*! \file portfolio/creditlinkedswap.hpp
    \brief credit linked swap data model
    \ingroup portfolio
*/

#include <ored/portfolio/creditlinkedswap.hpp>

#include <ored/portfolio/builders/creditlinkedswap.hpp>

namespace ore {
namespace data {

void CreditLinkedSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* n = XMLUtils::getChildNode(node, "CreditLinkedSwapData");
    creditCurveId_ = XMLUtils::getChildValue(n, "CreditCurveId");
    settlesAccrual_ = XMLUtils::getChildValueAsBool(n, "SettlesAccrual", false, true);
    fixedRecoveryRate_ = XMLUtils::getChildValueAsDouble(n, "FixedRecoveryRate", false, Null<Real>());
    defaultPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault;
    if (auto c = XMLUtils::getChildNode(n, "DefaultPaymentTime")) {
        if (XMLUtils::getNodeValue(c) == "atDefault")
            defaultPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault;
        else if (XMLUtils::getNodeValue(c) == "atPeriodEnd")
            defaultPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
        else if (XMLUtils::getNodeValue(c) == "atMaturity")
            defaultPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atMaturity;
        else {
            QL_FAIL("default payment time '" << XMLUtils::getNodeValue(c)
                                             << "' not known, expected atDefault, atPeriodEnd, atMaturity");
        }
    }
    if (auto tmp = XMLUtils::getChildNode(n, "IndependentPayments")) {
        for (auto const& d : XMLUtils::getChildrenNodes(tmp, "LegData")) {
            independentPayments_.push_back(LegData());
            independentPayments_.back().fromXML(d);
        }
    }
    if (auto tmp = XMLUtils::getChildNode(n, "ContingentPayments")) {
        for (auto const& d : XMLUtils::getChildrenNodes(tmp, "LegData")) {
            contingentPayments_.push_back(LegData());
            contingentPayments_.back().fromXML(d);
        }
    }
    if (auto tmp = XMLUtils::getChildNode(n, "DefaultPayments")) {
        for (auto const& d : XMLUtils::getChildrenNodes(tmp, "LegData")) {
            defaultPayments_.push_back(LegData());
            defaultPayments_.back().fromXML(d);
        }
    }
    if (auto tmp = XMLUtils::getChildNode(n, "RecoveryPayments")) {
        for (auto const& d : XMLUtils::getChildrenNodes(tmp, "LegData")) {
            recoveryPayments_.push_back(LegData());
            recoveryPayments_.back().fromXML(d);
        }
    }
}

XMLNode* CreditLinkedSwap::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* n = Trade::toXML(doc);
    XMLNode* d = doc.allocNode("CreditLinkedSwapData");
    XMLUtils::appendNode(n, d);
    XMLUtils::addChild(doc, d, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, d, "SettlesAccrual", settlesAccrual_);
    XMLUtils::addChild(doc, d, "FixedRecoveryRate", fixedRecoveryRate_);
    if (defaultPaymentTime_ == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault) {
        XMLUtils::addChild(doc, d, "DefaultPaymentTime", "atDefault");
    } else if (defaultPaymentTime_ == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd) {
        XMLUtils::addChild(doc, d, "DefaultPaymentTime", "atPeriodEnd");
    } else if (defaultPaymentTime_ == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atMaturity) {
        XMLUtils::addChild(doc, d, "DefaultPaymentTime", "atMaturity");
    } else {
        QL_FAIL("toXML(): unexpected DefaultPaymentTime");
    }
    XMLNode* d1 = doc.allocNode("IndependentPayments");
    XMLUtils::appendNode(d, d1);
    for (auto& l : independentPayments_) {
        XMLUtils::appendNode(d1, l.toXML(doc));
    }
    XMLNode* d2 = doc.allocNode("ContingentPayments");
    XMLUtils::appendNode(d, d2);
    for (auto& l : contingentPayments_) {
        XMLUtils::appendNode(d2, l.toXML(doc));
    }
    XMLNode* d3 = doc.allocNode("DefaultPayments");
    XMLUtils::appendNode(d, d3);
    for (auto& l : defaultPayments_) {
        XMLUtils::appendNode(d3, l.toXML(doc));
    }
    XMLNode* d4 = doc.allocNode("RecoveryPayments");
    XMLUtils::appendNode(d, d4);
    for (auto& l : recoveryPayments_) {
        XMLUtils::appendNode(d4, l.toXML(doc));
    }
    return n;
}

void CreditLinkedSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("Building credit linked swap " << id());

    // ISDA taxonomy

    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("");  
    additionalData_["isdaTransaction"] = string("");  

    // checks, set npv currency (= single currency allowed in all legs)

    npvCurrency_.clear();
    for (auto const& l : independentPayments_) {
        QL_REQUIRE(npvCurrency_ == "" || l.currency() == npvCurrency_,
                   "CreditLinkedSwap: all leg currencies must be the same, found " << npvCurrency_ << " and "
                                                                                   << l.currency());
        npvCurrency_ = l.currency();
    }

    // get engine builder

    auto builder = QuantLib::ext::dynamic_pointer_cast<CreditLinkedSwapEngineBuilder>(engineFactory->builder(tradeType()));
    QL_REQUIRE(builder, "CreditLinkedSwap: wrong builder, expected CreditLinkedSwapEngineBuilder");
    auto configuration = builder->configuration(MarketContext::pricing);

    // build underlying legs

    std::vector<QuantExt::CreditLinkedSwap::LegType> legTypes;
    for (auto const& l : independentPayments_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        legs_.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        legPayers_.push_back(l.isPayer());
        legTypes.push_back(QuantExt::CreditLinkedSwap::LegType::IndependentPayments);
    }
    for (auto const& l : contingentPayments_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        legs_.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        legPayers_.push_back(l.isPayer());
        legTypes.push_back(QuantExt::CreditLinkedSwap::LegType::ContingentPayments);
    }
    for (auto const& l : defaultPayments_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        legs_.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        legPayers_.push_back(l.isPayer());
        legTypes.push_back(QuantExt::CreditLinkedSwap::LegType::DefaultPayments);
    }
    for (auto const& l : recoveryPayments_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        legs_.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        legPayers_.push_back(l.isPayer());
        legTypes.push_back(QuantExt::CreditLinkedSwap::LegType::RecoveryPayments);
    }

    // build ql instrument

    auto qlInstr =
        QuantLib::ext::make_shared<QuantExt::CreditLinkedSwap>(legs_, legPayers_, legTypes, settlesAccrual_, fixedRecoveryRate_,
                                                       defaultPaymentTime_, parseCurrency(npvCurrency_));

    // wrap instrument

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlInstr);

    // set trade members

    notionalCurrency_ = npvCurrency_;
    legCurrencies_ = std::vector<std::string>(legs_.size(), npvCurrency_);
    maturity_ = qlInstr->maturity();

    // set pricing engine

    qlInstr->setPricingEngine(builder->engine(npvCurrency_, creditCurveId_));
    setSensitivityTemplate(*builder);

    // log

    DLOG("Finished building credit linked swap " << id());
    DLOG("Currency                : " << npvCurrency_);
    DLOG("IndependentPayments legs: " << independentPayments_.size());
    DLOG("ContingentPayments  legs: " << contingentPayments_.size());
    DLOG("DefaultPayments     legs: " << defaultPayments_.size());
    DLOG("RecoveryPayments    legs: " << recoveryPayments_.size());
}

QuantLib::Real CreditLinkedSwap::notional() const {
    Real notional = 0.0;
    for (auto const& l : legs_)
        notional = std::max(notional, currentNotional(l));
    return notional;
}

} // namespace data
} // namespace ore
