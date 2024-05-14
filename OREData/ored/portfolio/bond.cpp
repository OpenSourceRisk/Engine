/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2017 Aareal Bank AG

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

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <qle/utilities/inflation.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

QuantExt::BondIndex::PriceQuoteMethod BondData::priceQuoteMethod() const {
    return priceQuoteMethod_.empty() ? QuantExt::BondIndex::PriceQuoteMethod::PercentageOfPar
                                     : parsePriceQuoteMethod(priceQuoteMethod_);
}

Real BondData::priceQuoteBaseValue() const {
    if (priceQuoteBaseValue_.empty())
        return 1.0;
    Real result;
    if (tryParseReal(priceQuoteBaseValue_, result))
        return result;
    QL_FAIL("invalid PriceQuoteBaseValue '" << priceQuoteBaseValue_ << "'");
}

void BondData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BondData");
    QL_REQUIRE(node, "No BondData Node");
    subType_ = XMLUtils::getChildValue(node, "SubType", false);
    issuerId_ = XMLUtils::getChildValue(node, "IssuerId", false);
    creditCurveId_ = XMLUtils::getChildValue(node, "CreditCurveId", false);
    creditGroup_ = XMLUtils::getChildValue(node, "CreditGroup", false);
    securityId_ = XMLUtils::getChildValue(node, "SecurityId", true);
    referenceCurveId_ = XMLUtils::getChildValue(node, "ReferenceCurveId", false);
    incomeCurveId_ = XMLUtils::getChildValue(node, "IncomeCurveId", false);
    volatilityCurveId_ = XMLUtils::getChildValue(node, "VolatilityCurveId", false);
    settlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", false);
    calendar_ = XMLUtils::getChildValue(node, "Calendar", false);
    issueDate_ = XMLUtils::getChildValue(node, "IssueDate", false);
    priceQuoteMethod_ = XMLUtils::getChildValue(node, "PriceQuoteMethod", false);
    priceQuoteBaseValue_ = XMLUtils::getChildValue(node, "PriceQuoteBaseValue", false);
    if (auto n = XMLUtils::getChildNode(node, "BondNotional")) {
        bondNotional_ = parseReal(XMLUtils::getNodeValue(n));
    } else {
        bondNotional_ = 1.0;
    }
    XMLNode* legNode = XMLUtils::getChildNode(node, "LegData");
    isInflationLinked_ = false;
    while (legNode != nullptr) {
        LegData ld;
        ld.fromXML(legNode);
        coupons_.push_back(ld);
        if (ld.concreteLegData()->legType() == "CPI") {
            isInflationLinked_ = true;
        }
        legNode = XMLUtils::getNextSibling(legNode, "LegData");
    }
    hasCreditRisk_ = XMLUtils::getChildValueAsBool(node, "CreditRisk", false, true);
    initialise();
}

XMLNode* BondData::toXML(XMLDocument& doc) const {
    XMLNode* bondNode = doc.allocNode("BondData");
    if (!subType_.empty())
        XMLUtils::addChild(doc, bondNode, "SubType", subType_);
    if (!issuerId_.empty())
        XMLUtils::addChild(doc, bondNode, "IssuerId", issuerId_);
    if (!creditCurveId_.empty())
        XMLUtils::addChild(doc, bondNode, "CreditCurveId", creditCurveId_);
    if (!creditGroup_.empty())
        XMLUtils::addChild(doc, bondNode, "CreditGroup", creditGroup_);
    XMLUtils::addChild(doc, bondNode, "SecurityId", securityId_);
    if (!referenceCurveId_.empty())
        XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", referenceCurveId_);
    if (!incomeCurveId_.empty())
        XMLUtils::addChild(doc, bondNode, "IncomeCurveId", incomeCurveId_);
    if (!volatilityCurveId_.empty())
        XMLUtils::addChild(doc, bondNode, "VolatilityCurveId", volatilityCurveId_);
    if (!settlementDays_.empty())
        XMLUtils::addChild(doc, bondNode, "SettlementDays", settlementDays_);
    if (!calendar_.empty())
        XMLUtils::addChild(doc, bondNode, "Calendar", calendar_);
    if (!issueDate_.empty())
        XMLUtils::addChild(doc, bondNode, "IssueDate", issueDate_);
    if(!priceQuoteMethod_.empty())
	XMLUtils::addChild(doc, bondNode, "PriceQuoteMethod", priceQuoteMethod_);
    if(!priceQuoteBaseValue_.empty())
	XMLUtils::addChild(doc, bondNode, "PriceQuoteBaseValue", priceQuoteBaseValue_);
    XMLUtils::addChild(doc, bondNode, "BondNotional", bondNotional_);
    for (auto& c : coupons_)
        XMLUtils::appendNode(bondNode, c.toXML(doc));
    if (!hasCreditRisk_)
        XMLUtils::addChild(doc, bondNode, "CreditRisk", hasCreditRisk_);
    return bondNode;
}

void BondData::initialise() {

    isPayer_ = false;
    isInflationLinked_ = false;

    if (!zeroBond()) {

        // fill currency, if not directly given (which is only the case for zero bonds)

        for (Size i = 0; i < coupons().size(); ++i) {
            if (i == 0)
                currency_ = coupons()[i].currency();
            else {
                QL_REQUIRE(currency_ == coupons()[i].currency(),
                           "bond leg #" << i << " currency (" << coupons()[i].currency()
                                        << ") not equal to leg #0 currency (" << coupons()[0].currency());
            }
        }

        // fill isPayer, FIXME zero bonds are always long

        for (Size i = 0; i < coupons().size(); ++i) {
            if (i == 0)
                isPayer_ = coupons()[i].isPayer();
            else {
                QL_REQUIRE(isPayer_ == coupons()[i].isPayer(),
                           "bond leg #" << i << " isPayer (" << std::boolalpha << coupons()[i].isPayer()
                                        << ") not equal to leg #0 isPayer (" << coupons()[0].isPayer());
            }
        }

        // fill isInflationLinked
        for (Size i = 0; i < coupons().size(); ++i) {
            if (i == 0)
                isInflationLinked_ = coupons()[i].concreteLegData()->legType() == "CPI";
            else {
                bool isIthCouponInflationLinked = coupons()[i].concreteLegData()->legType() == "CPI";
                QL_REQUIRE(isInflationLinked_ == isIthCouponInflationLinked,
                           "bond leg #" << i << " isInflationLinked (" << std::boolalpha << isIthCouponInflationLinked
                                        << ") not equal to leg #0 isInflationLinked (" << isInflationLinked_);
            }
        }
    }
}

void BondData::populateFromBondReferenceData(const QuantLib::ext::shared_ptr<BondReferenceDatum>& referenceDatum,
					       const std::string& startDate, const std::string& endDate) {
    DLOG("Got BondReferenceDatum for name " << securityId_ << " overwrite empty elements in trade");
    ore::data::populateFromBondReferenceData(subType_, issuerId_, settlementDays_, calendar_, issueDate_, priceQuoteMethod_,
                                             priceQuoteBaseValue_, creditCurveId_, creditGroup_, referenceCurveId_,
                                             incomeCurveId_, volatilityCurveId_, coupons_, securityId_, referenceDatum,
                                             startDate, endDate);
    initialise();
    checkData();
}

void BondData::populateFromBondReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
					     const std::string& startDate, const std::string& endDate) {
    QL_REQUIRE(!securityId_.empty(), "BondData::populateFromBondReferenceData(): no security id given");
    if (!referenceData || !referenceData->hasData(BondReferenceDatum::TYPE, securityId_)) {
        DLOG("could not get BondReferenceDatum for name " << securityId_ << " leave data in trade unchanged");
        initialise();
        checkData();
    } else {
        auto bondRefData = QuantLib::ext::dynamic_pointer_cast<BondReferenceDatum>(
            referenceData->getData(BondReferenceDatum::TYPE, securityId_));
        QL_REQUIRE(bondRefData, "could not cast to BondReferenceDatum, this is unexpected");
        populateFromBondReferenceData(bondRefData, startDate, endDate);
    }
}

void BondData::checkData() const {
    QL_REQUIRE(!securityId_.empty(), "BondData invalid: no security id given");
    std::vector<std::string> missingElements;
    if (settlementDays_.empty())
        missingElements.push_back("SettlementDays");
    if (currency_.empty())
        missingElements.push_back("Currency");
    QL_REQUIRE(missingElements.empty(), "BondData invalid: missing " + boost::algorithm::join(missingElements, ", ") +
                                                " - check if reference data is set up for '"
                                            << securityId_ << "'");
}

std::string BondData::isdaBaseProduct() const {
    static const std::set<std::string> singleName = {"ABS", "Corporate", "Loans", "Muni", "Sovereign"};
    static const std::set<std::string> index = {"ABX", "CMBX", "MBX", "PrimeX", "TRX", "iBoxx"};
    if (singleName.find(subType()) != singleName.end()) {
        return "Single Name";
    } else if (index.find(subType()) != index.end()) {
        return "Index";
    } else {
        QL_FAIL("BondData::isdaBaseProduct() not defined for subType '"
                << subType() << "', expected: "
                << boost::algorithm::join(singleName, ", ") + " (map to 'Single Name') " +
                       boost::algorithm::join(index, ", ") + " (map to 'Index')");
    }
}

void Bond::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Bond::build() called for trade " << id());

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has Bond underlyings
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("Bond");
    QL_REQUIRE(builder, "Bond::build(): internal error, builder is null");

    bondData_ = originalBondData_;
    bondData_.populateFromBondReferenceData(engineFactory->referenceData());

    Date issueDate = parseDate(bondData_.issueDate());
    Calendar calendar = parseCalendar(bondData_.calendar());
    QL_REQUIRE(!bondData_.settlementDays().empty(),
               "no bond settlement days given, if reference data is used, check if securityId '"
                   << bondData_.securityId() << "' is present and of type Bond.");
    Natural settlementDays = parseInteger(bondData_.settlementDays());
    QuantLib::ext::shared_ptr<QuantLib::Bond> bond;

    std::string openEndDateStr = builder->modelParameter("OpenEndDateReplacement", {}, false, "");
    Date openEndDateReplacement = getOpenEndDateReplacement(openEndDateStr, calendar);
    Real mult = bondData_.bondNotional() * (bondData_.isPayer() ? -1.0 : 1.0);
    std::vector<Leg> separateLegs;
    if (bondData_.zeroBond()) { // Zero coupon bond
        bond.reset(new QuantLib::ZeroCouponBond(settlementDays, calendar, bondData_.faceAmount(),
                                                parseDate(bondData_.maturityDate())));
    } else { // Coupon bond
        for (Size i = 0; i < bondData_.coupons().size(); ++i) {
            Leg leg;
            auto configuration = builder->configuration(MarketContext::pricing);
            auto legBuilder = engineFactory->legBuilder(bondData_.coupons()[i].legType());
            leg = legBuilder->buildLeg(bondData_.coupons()[i], engineFactory, requiredFixings_, configuration,
                                       openEndDateReplacement);
            separateLegs.push_back(leg);
        } // for coupons_
        Leg leg = joinLegs(separateLegs);
        bond.reset(new QuantLib::Bond(settlementDays, calendar, issueDate, leg));
    }

    Currency currency = parseCurrency(bondData_.currency());
    QuantLib::ext::shared_ptr<BondEngineBuilder> bondBuilder = QuantLib::ext::dynamic_pointer_cast<BondEngineBuilder>(builder);
    QL_REQUIRE(bondBuilder, "No Builder found for Bond: " << id());
    bond->setPricingEngine(bondBuilder->engine(currency, bondData_.creditCurveId(), bondData_.hasCreditRisk(),
                                               bondData_.securityId(), bondData_.referenceCurveId()));
    setSensitivityTemplate(*bondBuilder);
    instrument_.reset(new VanillaInstrument(bond, mult));

    npvCurrency_ = bondData_.currency();
    maturity_ = bond->cashflows().back()->date();
    notional_ = currentNotional(bond->cashflows());
    notionalCurrency_ = bondData_.currency();

    issuer_ = bondData_.issuerId();

    // Add legs (only 1)
    legs_ = {bond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {bondData_.isPayer()};

    DLOG("Bond::build() finished for trade " << id());
}

void Bond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    originalBondData_.fromXML(XMLUtils::getChildNode(node, "BondData"));
    bondData_ = originalBondData_;
}

XMLNode* Bond::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, originalBondData_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
Bond::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {bondData_.securityId()};
    return result;
}

double BondBuilder::Result::inflationFactor() const {
    if (!isInflationLinked) {
        return 1.0;
    } else {
        QL_REQUIRE(bond, "need to set the bond before calling inflationFactor()");
        double factor = 1.0;
        try {
            factor = QuantExt::inflationLinkedBondQuoteFactor(bond);
        } catch (const std::exception& e) {
            ALOG("Failed to compute the inflation price factor for the bond "
                 << securityId << ", fallback to use factor 1, got " << e.what());
        }
        return factor;
    }
}

BondBuilder::Result BondFactory::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                       const std::string& securityId) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    for (auto const& b : builders_) {
        if (referenceData->hasData(b.first, securityId)) {
            auto tmp = b.second->build(engineFactory, referenceData, securityId);
            tmp.builderLabel = b.first;
            return tmp;
        }
    }

    QL_FAIL("BondFactory: could not build bond '"
            << securityId
            << "': no reference data given or no suitable builder registered. Check if bond is set up in the reference "
               "data and that there is a builder for the reference data type.");
}

void BondFactory::addBuilder(const std::string& referenceDataType, const QuantLib::ext::shared_ptr<BondBuilder>& builder,
                             const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(builders_.insert(std::make_pair(referenceDataType, builder)).second || allowOverwrite,
               "BondFactory::addBuilder(" << referenceDataType << "): builder for key already exists.");
}

BondBuilder::Result VanillaBondBuilder::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                              const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                              const std::string& securityId) const {
    BondData data(securityId, 1.0);
    data.populateFromBondReferenceData(referenceData);
    ore::data::Bond bond(Envelope(), data);
    bond.id() = "VanillaBondBuilder_" + securityId;
    bond.build(engineFactory);

    QL_REQUIRE(bond.instrument(), "VanillaBondBuilder: constructed bond is null, this is unexpected");
    auto qlBond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(bond.instrument()->qlInstrument());

    QL_REQUIRE(bond.instrument() && bond.instrument()->qlInstrument(),
               "VanillaBondBuilder: constructed bond trade does not provide a valid ql instrument, this is unexpected "
               "(either the instrument wrapper or the ql instrument is null)");

    Result res;
    res.bond = qlBond;

    if (data.isInflationLinked()) {
        res.isInflationLinked = true;       
    }
    res.hasCreditRisk = data.hasCreditRisk() && !data.creditCurveId().empty();
    res.currency = data.currency();
    res.creditCurveId = data.creditCurveId();
    res.securityId = data.securityId();
    res.creditGroup = data.creditGroup();
    res.priceQuoteMethod = data.priceQuoteMethod();
    res.priceQuoteBaseValue = data.priceQuoteBaseValue();
    return res;
}

} // namespace data
} // namespace ore
