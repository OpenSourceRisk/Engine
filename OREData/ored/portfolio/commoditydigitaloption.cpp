/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>

#include <ql/errors.hpp> 
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/commodityindex.hpp>

#include <ored/portfolio/commoditydigitaloption.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

using namespace std;
using namespace QuantLib;
using QuantExt::CommodityFuturesIndex;
using QuantExt::PriceTermStructure;

namespace ore {
namespace data {

CommodityDigitalOption::CommodityDigitalOption() { tradeType_ = "CommodityDigitalOption"; }

CommodityDigitalOption::CommodityDigitalOption(const Envelope& env, const OptionData& optionData, const string& name,
					       const string& currency, Real strike, Real payoff,
					       const boost::optional<bool>& isFuturePrice, const Date& futureExpiryDate)
    : optionData_(optionData), name_(name), currency_(currency), strike_(strike), payoff_(payoff),
      isFuturePrice_(isFuturePrice), futureExpiryDate_(futureExpiryDate) {
    tradeType_ = "CommodityDigitalOption";
}

void CommodityDigitalOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy, assuming Commodity follows the Equity template
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");

    // Checks
    QL_REQUIRE((strike_ > 0 || close_enough(strike_, 0.0)), "Commodity digital option requires a positive strike");
    QL_REQUIRE(optionData_.exerciseDates().size() == 1, "Invalid number of exercise dates");

    expiryDate_ = parseDate(optionData_.exerciseDates().front());

    // Populate the index_ in case the option is automatic exercise.
    // Intentionally use null calendar because we will ask for index value on the expiry date without adjustment.
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    index_ = *market->commodityIndex(name_, engineFactory->configuration(MarketContext::pricing));
    if (!isFuturePrice_ || *isFuturePrice_) {

        // Assume future price if isFuturePrice_ is not explicitly set or if it is and true.
        auto index = *market->commodityIndex(name_, engineFactory->configuration(MarketContext::pricing));

        // If we are given an explicit future contract expiry date, use it, otherwise use option's expiry.
        Date expiryDate;
        if (futureExpiryDate_ != Date()) {
            expiryDate = futureExpiryDate_;
        } else {
            // Get the expiry date of the option. This is the expiry date of the commodity future index.
            const vector<string>& expiryDates = optionData_.exerciseDates();
            QL_REQUIRE(expiryDates.size() == 1,
                       "Expected exactly one expiry date for CommodityDigitalOption but got " << expiryDates.size() << ".");
            expiryDate = parseDate(expiryDates[0]);
        }

        // Clone the index with the relevant expiry date.
        index_ = index->clone(expiryDate);

        // Set the VanillaOptionTrade forwardDate_ if the index is a CommodityFuturesIndex - we possibly still have a 
        // CommoditySpotIndex at this point so check. Also, will only work for European exercise.
        auto et = parseExerciseType(optionData_.style());
	QL_REQUIRE(et == Exercise::European, "European style expected for CommodityDigitalOption");
        if (et == Exercise::European && QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(index_)) {
            forwardDate_ = expiryDate;
        }
    }

    // Build digital as call or put spread 
    Real strikeSpread = strike_ * 0.01; // FIXME, what is a usual spread here, and where should we put it?
    Real strike1 = strike_ - strikeSpread/2;
    Real strike2 = strike_ + strikeSpread/2;
    CommodityOption opt1(envelope(), optionData_, name_, currency_, 1.0, TradeStrike(strike1, currency_), isFuturePrice_, futureExpiryDate_);
    CommodityOption opt2(envelope(), optionData_, name_, currency_, 1.0, TradeStrike(strike2, currency_),
                         isFuturePrice_, futureExpiryDate_);
    opt1.build(engineFactory);
    opt2.build(engineFactory);
    QuantLib::ext::shared_ptr<Instrument> inst1 = opt1.instrument()->qlInstrument();
    QuantLib::ext::shared_ptr<Instrument> inst2 = opt2.instrument()->qlInstrument();

    setSensitivityTemplate(opt1.sensitivityTemplate());

    QuantLib::ext::shared_ptr<CompositeInstrument> composite = QuantLib::ext::make_shared<CompositeInstrument>();
    // add and subtract such that the long call spread and long put spread have positive values
    if (optionData_.callPut() == "Call") {
        composite->add(inst1);
        composite->subtract(inst2);
    }
    else if (optionData_.callPut() == "Put") {
        composite->add(inst2);
        composite->subtract(inst1);
    }
    else {
        QL_FAIL("OptionType Call or Put required in CommodityDigitalOption " << id());
    }

    Position::Type positionType = parsePositionType(optionData_.longShort());
    Real bsIndicator = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real multiplier = payoff_ * bsIndicator / strikeSpread;
    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    // FIXME: Do we need to retrieve the engine builder's configuration
    string configuration = Market::defaultConfiguration; 
    Currency ccy = parseCurrencyWithMinors(currency_);
    maturity_ = std::max(expiryDate_, addPremiums(additionalInstruments, additionalMultipliers, multiplier,
						  optionData_.premiumData(), -bsIndicator, ccy, engineFactory, configuration));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(composite, multiplier, additionalInstruments, additionalMultipliers));

    npvCurrency_ = currency_;
    notional_ = payoff_;
    notionalCurrency_ = currency_;
    
    // LOG the volatility if the trade expiry date is in the future.
    if (expiryDate_ > Settings::instance().evaluationDate()) {
        DLOG("Implied vol for " << tradeType_ << " on " << name_ << " with expiry " << expiryDate_
                                << " and strike " << strike_ << " is "
                                << market->commodityVolatility(name_)->blackVol(expiryDate_, strike_));
    }

    additionalData_["payoff"] = payoff_;
    additionalData_["strike"] = strike_;
    additionalData_["optionType"] = optionData_.callPut();
    additionalData_["strikeCurrency"] = currency_;    
}

std::map<AssetClass, std::set<std::string>>
CommodityDigitalOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({name_})}};
}

void CommodityDigitalOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* commodityNode = XMLUtils::getChildNode(node, "CommodityDigitalOptionData");
    QL_REQUIRE(commodityNode, "A commodity option needs a 'CommodityDigitalOptionData' node");

    optionData_.fromXML(XMLUtils::getChildNode(commodityNode, "OptionData"));

    name_ = XMLUtils::getChildValue(commodityNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityNode, "Strike", true);
    payoff_ = XMLUtils::getChildValueAsDouble(commodityNode, "Payoff", true);

    isFuturePrice_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "IsFuturePrice"))
        isFuturePrice_ = parseBool(XMLUtils::getNodeValue(n));

    futureExpiryDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(commodityNode, "FutureExpiryDate"))
        futureExpiryDate_ = parseDate(XMLUtils::getNodeValue(n));
}

XMLNode* CommodityDigitalOption::toXML(XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* commodityNode = doc.allocNode("CommodityDigitalOptionData");
    XMLUtils::appendNode(node, commodityNode);

    XMLUtils::appendNode(commodityNode, optionData_.toXML(doc));

    XMLUtils::addChild(doc, commodityNode, "Name", name_);
    XMLUtils::addChild(doc, commodityNode, "Currency", currency_);
    XMLUtils::addChild(doc, commodityNode, "Strike", strike_);
    XMLUtils::addChild(doc, commodityNode, "Payoff", payoff_);

    if (isFuturePrice_)
        XMLUtils::addChild(doc, commodityNode, "IsFuturePrice", *isFuturePrice_);

    if (futureExpiryDate_ != Date())
        XMLUtils::addChild(doc, commodityNode, "FutureExpiryDate", to_string(futureExpiryDate_));

    return node;
}

} // namespace data
} // namespace ore
