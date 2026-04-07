/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/ratedigitaloption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/ratedigitaloption.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void RateDigitalOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    LOG("RateDigitalOption::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = std::string("Interest Rate");
    additionalData_["isdaBaseProduct"] = std::string("CapFloor");
    additionalData_["isdaSubProduct"] = std::string("Digital");
    additionalData_["isdaTransaction"] = std::string("");

    QL_REQUIRE(option_.style() == "European", "RateDigitalOption: only European style is supported, got "
                                                  << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "RateDigitalOption: exactly one exercise date required");
    QL_REQUIRE(strike_ > 0.0 && strike_ != Null<Real>(), "RateDigitalOption: invalid strike " << strike_);
    QL_REQUIRE(payoffAmount_ > 0.0 && payoffAmount_ != Null<Real>(),
               "RateDigitalOption: invalid payoff amount " << payoffAmount_);
    QL_REQUIRE(!payoffCurrency_.empty(), "RateDigitalOption: PayoffCurrency is required");
    QL_REQUIRE(!index_.empty(), "RateDigitalOption: Index is required");
    QL_REQUIRE(!fixingDate_.empty(), "RateDigitalOption: FixingDate is required");

    Date expiryDate = parseDate(option_.exerciseDates().front());
    Date fixDate = parseDate(fixingDate_);
    Date payDate = paymentDate_.empty() ? expiryDate : parseDate(paymentDate_);

    Currency ccy = parseCurrency(payoffCurrency_);

    Option::Type type = parseOptionType(option_.callPut());

    ext::shared_ptr<StrikedTypePayoff> payoff = ext::make_shared<CashOrNothingPayoff>(type, strike_, payoffAmount_);

    // European exercise — use the payment date so that the AnalyticEuropeanEngine
    // discounts to the payment date (consistent with the RangeAccrual BGM pricer
    // which uses DF(paymentDate) as the deflator for all observation-date digitals).
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(payDate);

    ext::shared_ptr<VanillaOption> vanilla = ext::make_shared<VanillaOption>(payoff, exercise);

    // Get the engine builder and set the pricing engine
    ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "RateDigitalOption: no builder found for " << tradeType_);
    auto rateDigitalBuilder = ext::dynamic_pointer_cast<RateDigitalOptionEngineBuilder>(builder);
    QL_REQUIRE(rateDigitalBuilder, "RateDigitalOption: wrong builder type for " << tradeType_);

    vanilla->setPricingEngine(rateDigitalBuilder->engine(index_, strike_, fixDate, payDate));
    setSensitivityTemplate(*rateDigitalBuilder);

    Position::Type positionType = parsePositionType(option_.longShort());
    Real mult = (positionType == Position::Long ? 1.0 : -1.0);

    std::vector<ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    std::string discountCurve = envelope().additionalField("discount_curve", false, std::string());
    Date lastPremiumDate =
        addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(), -1.0, ccy,
                    discountCurve, engineFactory, rateDigitalBuilder->configuration(MarketContext::pricing));

    instrument_ = ext::make_shared<VanillaInstrument>(vanilla, mult, additionalInstruments, additionalMultipliers);

    npvCurrency_ = payoffCurrency_;
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_;
    maturity_ = std::max(lastPremiumDate, std::max(expiryDate, payDate));
    maturityType_ = "Expiry Date";

    // Additional data for reporting
    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;
    additionalData_["strike"] = strike_;
    additionalData_["index"] = index_;
    additionalData_["fixingDate"] = fixingDate_;
    additionalData_["paymentDate"] = paymentDate_.empty() ? ore::data::to_string(expiryDate) : paymentDate_;

    LOG("RateDigitalOption::build() done for trade " << id());
}

void RateDigitalOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, "RateDigitalOptionData");
    QL_REQUIRE(dataNode, "RateDigitalOption: no RateDigitalOptionData node found");

    option_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    index_ = XMLUtils::getChildValue(dataNode, "Index", true);
    strike_ = XMLUtils::getChildValueAsDouble(dataNode, "Strike", true);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(dataNode, "PayoffAmount", true);
    payoffCurrency_ = XMLUtils::getChildValue(dataNode, "PayoffCurrency", true);
    fixingDate_ = XMLUtils::getChildValue(dataNode, "FixingDate", true);
    paymentDate_ = XMLUtils::getChildValue(dataNode, "PaymentDate", false);
}

XMLNode* RateDigitalOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode("RateDigitalOptionData");
    XMLUtils::appendNode(node, dataNode);

    XMLUtils::appendNode(dataNode, option_.toXML(doc));
    XMLUtils::addChild(doc, dataNode, "Index", index_);
    XMLUtils::addChild(doc, dataNode, "Strike", strike_);
    XMLUtils::addChild(doc, dataNode, "PayoffAmount", payoffAmount_);
    XMLUtils::addChild(doc, dataNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, dataNode, "FixingDate", fixingDate_);
    if (!paymentDate_.empty())
        XMLUtils::addChild(doc, dataNode, "PaymentDate", paymentDate_);

    return node;
}

} // namespace data
} // namespace ore
