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

#include <ored/portfolio/builders/creditdefaultswapoption.hpp>
#include <ored/portfolio/creditdefaultswapoption.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

CreditDefaultSwapOption::AuctionSettlementInformation::AuctionSettlementInformation()
    : auctionFinalPrice_(Null<Real>()) {}

CreditDefaultSwapOption::AuctionSettlementInformation::AuctionSettlementInformation(
    const Date& auctionSettlementDate, Real& auctionFinalPrice)
    : auctionSettlementDate_(auctionSettlementDate), auctionFinalPrice_(auctionFinalPrice) {}

const Date& CreditDefaultSwapOption::AuctionSettlementInformation::auctionSettlementDate() const {
    return auctionSettlementDate_;
}

Real CreditDefaultSwapOption::AuctionSettlementInformation::auctionFinalPrice() const {
    return auctionFinalPrice_;
}

void CreditDefaultSwapOption::AuctionSettlementInformation::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AuctionSettlementInformation");
    auctionSettlementDate_ = parseDate(XMLUtils::getChildValue(node, "AuctionSettlementDate", true));
    auctionFinalPrice_ = XMLUtils::getChildValueAsDouble(node, "AuctionFinalPrice", true);
}

XMLNode* CreditDefaultSwapOption::AuctionSettlementInformation::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("AuctionSettlementInformation");
    XMLUtils::addChild(doc, node, "AuctionSettlementDate", to_string(auctionSettlementDate_));
    XMLUtils::addChild(doc, node, "AuctionFinalPrice", auctionFinalPrice_);
    return node;
}

CreditDefaultSwapOption::CreditDefaultSwapOption()
    : Trade("CreditDefaultSwapOption"), strike_(Null<Real>()), knockOut_(true) {}

CreditDefaultSwapOption::CreditDefaultSwapOption(const Envelope& env,
    const OptionData& option,
    const CreditDefaultSwapData& swap,
    Real strike,
    const string& strikeType,
    bool knockOut,
    const string& term,
    const boost::optional<AuctionSettlementInformation>& asi)
    : Trade("CreditDefaultSwapOption", env), option_(option), swap_(swap), strike_(strike),
      strikeType_(strikeType), knockOut_(knockOut), term_(term), asi_(asi) {}

void CreditDefaultSwapOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CreditDefaultSwapOption::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Swaptions");
    // set isdaSubProduct to entityType in credit reference data
    additionalData_["isdaSubProduct"] = string("");
    string entity =
        swap_.referenceInformation() ? swap_.referenceInformation()->referenceEntityId() : swap_.creditCurveId();
    QuantLib::ext::shared_ptr<ReferenceDataManager> refData = engineFactory->referenceData();
    if (refData && refData->hasData("Credit", entity)) {
        auto refDatum = refData->getData("Credit", entity);
        QuantLib::ext::shared_ptr<CreditReferenceDatum> creditRefDatum =
            QuantLib::ext::dynamic_pointer_cast<CreditReferenceDatum>(refDatum);
        additionalData_["isdaSubProduct"] = creditRefDatum->creditData().entityType;
        if (creditRefDatum->creditData().entityType == "") {
            ALOG("EntityType is blank in credit reference data for entity " << entity);
        }
    } else {
        ALOG("Credit reference data missing for entity " << entity << ", isdaSubProduct left blank");
    }
    // Skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    // Notionals
    const auto& legData = swap_.leg();
    const auto& ntls = legData.notionals();
    QL_REQUIRE(ntls.size() == 1, "CreditDefaultSwapOption requires a single notional.");
    notional_ = ntls.front();
    notionalCurrency_ = legData.currency();

    // Type of instrument we build depends on whether the reference entity has already defaulted.
    if (asi_) {
        buildDefaulted(engineFactory);
    } else {
        buildNoDefault(engineFactory);
    }
}

const OptionData& CreditDefaultSwapOption::option() const {
    return option_;
}

const CreditDefaultSwapData& CreditDefaultSwapOption::swap() const {
    return swap_;
}

Real CreditDefaultSwapOption::strike() const {
    return strike_;
}

const string& CreditDefaultSwapOption::strikeType() const {
    return strikeType_;
}

bool CreditDefaultSwapOption::knockOut() const {
    return knockOut_;
}

const string& CreditDefaultSwapOption::term() const {
    return term_;
}

using ASI = CreditDefaultSwapOption::AuctionSettlementInformation;
const boost::optional<ASI>& CreditDefaultSwapOption::auctionSettlementInformation() const {
    return asi_;
}

void CreditDefaultSwapOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* cdsOptionData = XMLUtils::getChildNode(node, "CreditDefaultSwapOptionData");
    QL_REQUIRE(cdsOptionData, "Expected CreditDefaultSwapOptionData node on trade " << id() << ".");
    strike_ = XMLUtils::getChildValueAsDouble(cdsOptionData, "Strike", false, Null<Real>());
    strikeType_ = "Spread";
    if (auto n = XMLUtils::getChildNode(cdsOptionData, "StrikeType")) {
        strikeType_ = XMLUtils::getNodeValue(n);
    }
    knockOut_ = XMLUtils::getChildValueAsBool(cdsOptionData, "KnockOut", false, true);
    term_ = XMLUtils::getChildValue(cdsOptionData, "Term", false);

    if (XMLNode* asiNode = XMLUtils::getChildNode(cdsOptionData, "AuctionSettlementInformation"))
        asi_->fromXML(asiNode);

    XMLNode* cdsData = XMLUtils::getChildNode(cdsOptionData, "CreditDefaultSwapData");
    QL_REQUIRE(cdsData, "Expected CreditDefaultSwapData node on trade " << id() << ".");
    swap_.fromXML(cdsData);

    XMLNode* optionData = XMLUtils::getChildNode(cdsOptionData, "OptionData");
    QL_REQUIRE(optionData, "Expected OptionData node on trade " << id() << ".");
    option_.fromXML(optionData);
}

XMLNode* CreditDefaultSwapOption::toXML(XMLDocument& doc) const {

    // Trade node
    XMLNode* node = Trade::toXML(doc);

    // CreditDefaultSwapOptionData node
    XMLNode* cdsOptionDataNode = doc.allocNode("CreditDefaultSwapOptionData");
    if (strike_ != Null<Real>())
        XMLUtils::addChild(doc, cdsOptionDataNode, "Strike", strike_);
    if (strikeType_ != "")
        XMLUtils::addChild(doc, cdsOptionDataNode, "StrikeType", strikeType_);
    XMLUtils::addChild(doc, cdsOptionDataNode, "KnockOut", knockOut_);
    if (!term_.empty())
        XMLUtils::addChild(doc, cdsOptionDataNode, "Term", term_);

    if (asi_)
        XMLUtils::appendNode(cdsOptionDataNode, asi_->toXML(doc));

    XMLUtils::appendNode(cdsOptionDataNode, swap_.toXML(doc));
    XMLUtils::appendNode(cdsOptionDataNode, option_.toXML(doc));

    // Add CreditDefaultSwapOptionData node to Trade node
    XMLUtils::appendNode(node, cdsOptionDataNode);

    return node;
}

void CreditDefaultSwapOption::buildNoDefault(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CreditDefaultSwapOption: building CDS option trade " << id() << " given no default.");

    // Need fixed leg data with one rate. This should be the standard running coupon on the CDS e.g. 
    // generally 100bp for IG CDS and 500bp for HY CDS. For single name CDS options, one can use this field to give 
    // the strike spread. It may matter for the resulting valuation depending on the engine that is used - see 
    // "A CDS Option Miscellany, Richard J. Martin, 2019, Section 2.4".
    const auto& legData = swap_.leg();
    QL_REQUIRE(legData.legType() == "Fixed", "CDS option " << id() << " requires fixed leg.");
    auto fixedLegData = QuantLib::ext::dynamic_pointer_cast<FixedLegData>(legData.concreteLegData());
    QL_REQUIRE(fixedLegData->rates().size() == 1, "Index CDS option " << id() << " requires single fixed rate.");
    auto runningCoupon = fixedLegData->rates().front();

    // Payer (Receiver) swaption if the leg is paying (receiving).
    auto side = legData.isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;

    // Day counter. In general for CDS, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    Actual360 standardDayCounter;
    const std::string& strDc = legData.dayCounter();
    DayCounter dc = strDc.empty() ? standardDayCounter : parseDayCounter(strDc);
    DayCounter lastPeriodDayCounter = dc == standardDayCounter ? Actual360(true) : dc;

    // Schedule
    Schedule schedule = makeSchedule(legData.schedule());
    BusinessDayConvention payConvention = legData.paymentConvention().empty() ? Following :
        parseBusinessDayConvention(legData.paymentConvention());

    // Don't support upfront fee on the underlying CDS for the moment.
    QL_REQUIRE(swap_.upfrontFee() == Null<Real>() || close(swap_.upfrontFee(), 0.0),
        "Upfront fee on the CDS underlying a CDS option is not supported.");

    // The underlying CDS trade
    auto cds = QuantLib::ext::make_shared<QuantLib::CreditDefaultSwap>(side, notional_, runningCoupon, schedule,
        payConvention, dc, swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(),
        QuantLib::ext::shared_ptr<Claim>(), lastPeriodDayCounter, true, swap_.tradeDate(), swap_.cashSettlementDays());

    // Copying here what is done for the index CDS option. The comment there is:
    // Align option product maturities with ISDA AANA/GRID guidance as of November 2020.
    maturity_ = std::max(cds->coupons().back()->date(), option_.premiumData().latestPremiumDate());

    // Set engine on the underlying CDS.
    auto cdsBuilder = QuantLib::ext::dynamic_pointer_cast<CreditDefaultSwapEngineBuilder>(
        engineFactory->builder("CreditDefaultSwap"));
    QL_REQUIRE(cdsBuilder, "CreditDefaultSwapOption expected CDS engine " <<
        " builder for underlying while building trade " << id() << ".");
    npvCurrency_ = legData.currency();
    auto ccy = parseCurrency(npvCurrency_);
    cds->setPricingEngine(cdsBuilder->engine(ccy, swap_.creditCurveId(), swap_.recoveryRate()));
    setSensitivityTemplate(*cdsBuilder);

    // Check option data
    QL_REQUIRE(option_.style() == "European", "CreditDefaultSwapOption option style must" <<
        " be European but got " << option_.style() << ".");
    QL_REQUIRE(!option_.payoffAtExpiry(), "CreditDefaultSwapOption payoff must be at exercise.");
    QL_REQUIRE(option_.exerciseFees().empty(), "CreditDefaultSwapOption cannot handle exercise fees.");

    // Exercise must be European
    const auto& exerciseDates = option_.exerciseDates();
    QL_REQUIRE(exerciseDates.size() == 1, "CreditDefaultSwapOption expects one exercise date" <<
        " but got " << exerciseDates.size() << " exercise dates.");
    Date exerciseDate = parseDate(exerciseDates.front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    // Limit strike type to Spread for now.
    auto strikeType = parseCdsOptionStrikeType(strikeType_);
    QL_REQUIRE(strikeType == QuantExt::CdsOption::Spread, "CreditDefaultSwapOption strike type must be Spread.");

    // If the strike_ is Null, the strike is taken as the running coupon.
    Real strike = strike_ == Null<Real>() ? runningCoupon : strike_;

    // Build the option instrument
    auto cdsOption = QuantLib::ext::make_shared<QuantExt::CdsOption>(cds, exercise, knockOut_, strike, strikeType);

    // Set the option engine
    auto cdsOptionEngineBuilder = QuantLib::ext::dynamic_pointer_cast<CreditDefaultSwapOptionEngineBuilder>(
        engineFactory->builder("CreditDefaultSwapOption"));
    QL_REQUIRE(cdsOptionEngineBuilder, "CreditDefaultSwapOption expected CDS option engine " <<
        " builder for underlying while building trade " << id() << ".");
    cdsOption->setPricingEngine(cdsOptionEngineBuilder->engine(ccy, swap_.creditCurveId(), term_));
    setSensitivityTemplate(*cdsOptionEngineBuilder);

    // Set Trade members.
    legs_ = { cds->coupons() };
    legCurrencies_ = { npvCurrency_ };
    legPayers_ = { legData.isPayer() };

    // Include premium if enough information is provided
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;
    string marketConfig = cdsOptionEngineBuilder->configuration(MarketContext::pricing);
    addPremium(engineFactory, ccy, marketConfig, additionalInstruments, additionalMultipliers);

    // Instrument wrapper depends on the settlement type.
    Position::Type positionType = parsePositionType(option_.longShort());
    Settlement::Type settleType = parseSettlementType(option_.settlement());
    // The instrument build should be indpednent of the evaluation date. However, the general behavior
    // in ORE (e.g. IR swaptions) for normal pricing runs is that the option is considered expired on
    // the expiry date with no assumptions on an (automatic) exercise. Therefore we build a vanilla
    // instrument if the exercise date is <= the eval date at build time.
    if (settleType == Settlement::Cash || exerciseDate <= Settings::instance().evaluationDate()) {
        Real indicatorLongShort = positionType == Position::Long ? 1.0 : -1.0;
        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(cdsOption, indicatorLongShort,
            additionalInstruments, additionalMultipliers);
    } else {
        bool isLong = positionType == Position::Long;
        bool isPhysical = settleType == Settlement::Physical;
        instrument_ = QuantLib::ext::make_shared<EuropeanOptionWrapper>(cdsOption, isLong, exerciseDate,
            isPhysical, cds, 1.0, 1.0, additionalInstruments, additionalMultipliers);
    }
}

void CreditDefaultSwapOption::buildDefaulted(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CreditDefaultSwapOption: building CDS option trade " << id() << " given default occurred.");

    // We add a simple payment for CDS options where the reference entity has already defaulted.
    // If it is a knock-out CDS option, we add a dummy payment of 0.0 with date today instead of throwing.
    const auto& legData = swap_.leg();
    Date paymentDate = engineFactory->market()->asofDate();
    Real amount = 0.0;
    if (!knockOut_) {
        paymentDate = asi_->auctionSettlementDate();
        amount = notional_ * (1 - asi_->auctionFinalPrice());
        // If it is a receiver option, i.e. selling protection, the FEP is paid out.
        if (!legData.isPayer())
            amount *= -1.0;
    }

    Position::Type positionType = parsePositionType(option_.longShort());
    Real indicatorLongShort = positionType == Position::Long ? 1.0 : -1.0;

    // Use the add premiums method to add the payment.
    string marketConfig = Market::defaultConfiguration;
    auto ccy = parseCurrency(notionalCurrency_);
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;
    Date premiumPayDate =
        addPremiums(additionalInstruments, additionalMultipliers, indicatorLongShort,
                    PremiumData(amount, notionalCurrency_, paymentDate), 1.0, ccy, engineFactory, marketConfig);
    DLOG("FEP payment (date = " << paymentDate << ", amount = " << amount << ") added for CDS option " << id() << ".");

    // Use the instrument added as the main instrument and clear the vectors
    auto qlInst = additionalInstruments.back();
    QL_REQUIRE(qlInst, "Expected a FEP payment to have been added for CDS option " << id() << ".");
    maturity_ = std::max(paymentDate, premiumPayDate);
    additionalInstruments.clear();
    additionalMultipliers.clear();

    // Include premium if enough information is provided
    addPremium(engineFactory, ccy, marketConfig, additionalInstruments, additionalMultipliers);

    // Instrument wrapper.
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlInst, indicatorLongShort,
        additionalInstruments, additionalMultipliers);
}

Date CreditDefaultSwapOption::addPremium(const QuantLib::ext::shared_ptr<EngineFactory>& ef,
    const Currency& tradeCurrency,
    const string& marketConfig,
    vector<QuantLib::ext::shared_ptr<Instrument>>& additionalInstruments,
    vector<Real>& additionalMultipliers) {
        // The premium amount is always provided as a non-negative amount. Assign the correct sign here i.e.
        // pay the premium if long the option and receive the premium if short the option.
        Position::Type positionType = parsePositionType(option_.longShort());
        Real indicatorLongShort = positionType == Position::Long ? 1.0 : -1.0;
        return addPremiums(additionalInstruments, additionalMultipliers, indicatorLongShort, option_.premiumData(),
                           indicatorLongShort, tradeCurrency, ef, marketConfig);
}

}
}
