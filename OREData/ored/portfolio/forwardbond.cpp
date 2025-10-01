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

#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/instruments/forwardbond.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual360.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void ForwardBond::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("ForwardBond::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Forward");
    additionalData_["isdaSubProduct"] = string("Debt");
    additionalData_["isdaTransaction"] = string("");

    additionalData_["currency"] = currency_;

    // propagate some parameters to underlying bond builder on a copy of engine factory

    auto engineFactoryOverride = QuantLib::ext::make_shared<EngineFactory>(*engineFactory);
    QuantLib::ext::shared_ptr<EngineBuilder> builder_fwd = engineFactoryOverride->builder("ForwardBond");
    auto isBond = [](const std::string& s) { return s.find("Bond") != std::string::npos; };
    std::vector<EngineFactory::ParameterOverride> overrides;
    overrides.push_back(EngineFactory::ParameterOverride{
        "ForwardBond",
        isBond,
        {{"TreatSecuritySpreadAsCreditSpread",
          builder_fwd->modelParameter("TreatSecuritySpreadAsCreditSpread", {}, false, "false")}}});
    overrides.push_back(EngineFactory::ParameterOverride{
        "ForwardBond",
        isBond,
        {{"SpreadOnIncomeCurve", builder_fwd->engineParameter("SpreadOnIncomeCurve", {}, false, "false")}}});
    overrides.push_back(EngineFactory::ParameterOverride{
        "ForwardBond", isBond, {{"TimestepPeriod", builder_fwd->engineParameter("TimestepPeriod", {}, false, "3M")}}});
    engineFactoryOverride->setEngineParameterOverrides(overrides);

    const QuantLib::ext::shared_ptr<Market> market = engineFactoryOverride->market();
    bondData_ = originalBondData_;

    auto bondType = getBondReferenceDatumType(bondData_.securityId(), engineFactoryOverride->referenceData());

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond;

    Real bondNotional = bondData_.bondNotional();

    try {

        if (bondType.empty() || bondType == BondReferenceDatum::TYPE) {

            // vanilla bond underlying

            bondData_.populateFromBondReferenceData(engineFactoryOverride->referenceData());
            auto underlying = QuantLib::ext::make_shared<ore::data::Bond>(Envelope(), bondData_);
            underlying->build(engineFactoryOverride);
            bond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(underlying->instrument()->qlInstrument());

        } else {

            // non-vanilla bond underlying (callable bond etc.)

            auto r = BondFactory::instance().build(engineFactoryOverride, engineFactoryOverride->referenceData(),
                                                   bondData_.securityId());
            bond = r.bond;
            bondData_ = r.bondData;
        }

    } catch (...) {
        // try to fill some fields for trade matching purposes although the trade build itself failed already
        npvCurrency_ = notionalCurrency_ = currency_ = bondData_.currency();
        for (auto const& d : bondData_.coupons()) {
            try {
                auto s = makeSchedule(d.schedule());
                maturity_ = std::max(maturity_, s.back());
            } catch (...) {
            }
            if (!d.notionals().empty())
                notional_ = d.notionals().front();
        }
        notional_ *= bondNotional;
        throw;
    }

    npvCurrency_ = currency_ = bondData_.coupons().front().currency();
    notionalCurrency_ = currency_;

    additionalData_["underlyingSecurityId"] = bondData_.securityId();

    QL_REQUIRE(!bondData_.referenceCurveId().empty(), "reference curve id required");
    QL_REQUIRE(!bondData_.settlementDays().empty(), "settlement days required");

    Date fwdMaturityDate = parseDate(fwdMaturityDate_);
    Date fwdSettlementDate = fwdSettlementDate_.empty() ? fwdMaturityDate : parseDate(fwdSettlementDate_);
    bool isPhysicallySettled;
    if (settlement_ == "Physical" || settlement_.empty())
        isPhysicallySettled = true;
    else if (settlement_ == "Cash")
        isPhysicallySettled = false;
    else {
        QL_FAIL("ForwardBond: invalid settlement '" << settlement_ << "', expected Cash or Physical");
    }
    Real amount = amount_.empty() ? Null<Real>() : parseReal(amount_);
    Real lockRate = lockRate_.empty() ? Null<Real>() : parseReal(lockRate_);
    Real dv01 = dv01_.empty() ? Null<Real>() : parseReal(dv01_);
    DayCounter lockRateDayCounter = lockRateDayCounter_.empty() ? Actual360() : parseDayCounter(lockRateDayCounter_);
    bool settlementDirty = settlementDirty_.empty() ? true : parseBool(settlementDirty_);
    Real compensationPayment = parseReal(compensationPayment_);
    Date compensationPaymentDate = parseDate(compensationPaymentDate_);
    bool longInForward = parseBool(longInForward_);
    bool knockOut = knockOut_.empty() ? !lockRate_.empty() : parseBool(knockOut_);

    QL_REQUIRE((amount == Null<Real>() && lockRate != Null<Real>()) ||
                   (amount != Null<Real>() && lockRate == Null<Real>()),
               "ForwardBond: exactly one of Amount of LockRate must be given");
    QL_REQUIRE(dv01 >= 0.0, "negative DV01 given");
    QL_REQUIRE(compensationPaymentDate <= fwdMaturityDate, "Premium cannot be paid after forward contract maturity");

    if (lockRate != Null<Real>())
        isPhysicallySettled = false;

    bool firstLegIsPayer = bondData_.coupons()[0].isPayer();
    QL_REQUIRE(firstLegIsPayer == false, "ForwardBond: The underlying bond must be entered with a receiver leg. Use "
                                         "LongInBond to specify pay direction of forward payoff");
    QL_REQUIRE(compensationPayment > 0.0 || close_enough(compensationPayment, 0.0),
               "ForwardBond: Negative compensation payments ("
                   << compensationPayment
                   << ") are not allowed. Notice that we will ensure that a positive compensation amount will be paid "
                      "by the party being long in the forward contract.");

    // cashflows will be generated as additional results in the pricing engine
    legs_ = {};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {firstLegIsPayer};
    Currency currency = parseCurrency(currency_);
    maturity_ = bond->cashflows().back()->date();
    maturityType_ = "Bond Cashflow Date";
    notional_ = currentNotional(bond->cashflows()) * bondNotional;

    auto fwdBondBuilder = QuantLib::ext::dynamic_pointer_cast<FwdBondEngineBuilder>(builder_fwd);
    QL_REQUIRE(fwdBondBuilder, "ForwardBond::build(): could not cast builder to FwdBondEngineBuilder: " << id());

    // first ctor is for vanilla fwd bonds, second for tlocks with a lock rate specifying the payoff
    QuantLib::ext::shared_ptr<QuantLib::Instrument> fwdBond =
        amount != Null<Real>()
            ? QuantLib::ext::make_shared<QuantExt::ForwardBond>(
                  bond, amount, fwdMaturityDate, fwdSettlementDate, isPhysicallySettled, knockOut, settlementDirty,
                  compensationPayment, compensationPaymentDate, longInForward, bondNotional)
            : QuantLib::ext::make_shared<QuantExt::ForwardBond>(
                  bond, lockRate, lockRateDayCounter, longInForward, fwdMaturityDate, fwdSettlementDate,
                  isPhysicallySettled, knockOut, settlementDirty, compensationPayment, compensationPaymentDate,
                  longInForward, bondNotional, dv01);

    // contractId as input for the spread on the contract discount is empty
    fwdBond->setPricingEngine(fwdBondBuilder->engine(
        id(), currency, bondData_.securityId(), envelope().additionalField("discount_curve", false, std::string()),
        bondData_.creditCurveId(), bondData_.securityId(), bondData_.referenceCurveId(), bondData_.incomeCurveId(),
        settlementDirty, Real()));

    setSensitivityTemplate(*fwdBondBuilder);
    addProductModelEngine(*fwdBondBuilder);
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0));

    additionalData_["currentNotional"] = currentNotional(bond->cashflows()) * bondNotional;
    additionalData_["originalNotional"] = originalNotional(bond->cashflows()) * bondNotional;

    engineFactory->modelBuilders().insert(engineFactoryOverride->modelBuilders().begin(),
                                          engineFactoryOverride->modelBuilders().end());
}

void ForwardBond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fwdBondNode = XMLUtils::getChildNode(node, "ForwardBondData");
    QL_REQUIRE(fwdBondNode, "No ForwardBondData Node");
    originalBondData_.fromXML(XMLUtils::getChildNode(fwdBondNode, "BondData"));
    bondData_ = originalBondData_;

    XMLNode* fwdSettlementNode = XMLUtils::getChildNode(fwdBondNode, "SettlementData");
    QL_REQUIRE(fwdSettlementNode, "No fwdSettlementNode Node");

    fwdMaturityDate_ = XMLUtils::getChildValue(fwdSettlementNode, "ForwardMaturityDate", true);
    fwdSettlementDate_ = XMLUtils::getChildValue(fwdSettlementNode, "ForwardSettlementDate", false);
    settlement_ = XMLUtils::getChildValue(fwdSettlementNode, "Settlement", false);
    amount_ = XMLUtils::getChildValue(fwdSettlementNode, "Amount", false);
    lockRate_ = XMLUtils::getChildValue(fwdSettlementNode, "LockRate", false);
    lockRateDayCounter_ = XMLUtils::getChildValue(fwdSettlementNode, "LockRateDayCounter", false);
    settlementDirty_ = XMLUtils::getChildValue(fwdSettlementNode, "SettlementDirty", false);
    dv01_ = XMLUtils::getChildValue(fwdSettlementNode, "dv01", false);

    XMLNode* fwdPremiumNode = XMLUtils::getChildNode(fwdBondNode, "PremiumData");
    if (fwdPremiumNode) {
        compensationPayment_ = XMLUtils::getChildValue(fwdPremiumNode, "Amount", true);
        compensationPaymentDate_ = XMLUtils::getChildValue(fwdPremiumNode, "Date", true);
    } else {
        compensationPayment_ = "0.0";
        compensationPaymentDate_ = fwdMaturityDate_;
    }

    longInForward_ = XMLUtils::getChildValue(fwdBondNode, "LongInForward", true);
    knockOut_ = XMLUtils::getChildValue(fwdBondNode, "KnockOut", false);
}

XMLNode* ForwardBond::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fwdBondNode = doc.allocNode("ForwardBondData");
    XMLUtils::appendNode(node, fwdBondNode);
    XMLUtils::appendNode(fwdBondNode, originalBondData_.toXML(doc));

    XMLNode* fwdSettlementNode = doc.allocNode("SettlementData");
    XMLUtils::appendNode(fwdBondNode, fwdSettlementNode);
    XMLUtils::addChild(doc, fwdSettlementNode, "ForwardMaturityDate", fwdMaturityDate_);
    if (!fwdSettlementDate_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "ForwardSettlementDate", fwdSettlementDate_);
    if (!settlement_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "Settlement", settlement_);
    if (!amount_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "Amount", amount_);
    if (!lockRate_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "LockRate", lockRate_);
    if (!dv01_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "dv01", dv01_);
    if (!lockRateDayCounter_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "LockRateDayCounter", lockRateDayCounter_);
    if (!settlementDirty_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "SettlementDirty", settlementDirty_);

    XMLNode* fwdPremiumNode = doc.allocNode("PremiumData");
    XMLUtils::appendNode(fwdBondNode, fwdPremiumNode);
    XMLUtils::addChild(doc, fwdPremiumNode, "Amount", compensationPayment_);
    XMLUtils::addChild(doc, fwdPremiumNode, "Date", compensationPaymentDate_);

    XMLUtils::addChild(doc, fwdBondNode, "LongInForward", longInForward_);
    if (!knockOut_.empty())
        XMLUtils::addChild(doc, fwdBondNode, "KnockOut", knockOut_);

    return node;
}

std::map<AssetClass, std::set<std::string>>
ForwardBond::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {bondData_.securityId()};
    return result;
}

} // namespace data
} // namespace ore
