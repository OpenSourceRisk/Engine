/*
  Copyright (C) 2020 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file ored/portfolio/asianoption.hpp
\brief Asian option representation
\ingroup tradedata
*/

#include <ored/portfolio/asianoption.hpp>
#include <ored/portfolio/builders/asianoption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/instruments/asianoption.hpp>
#include <ql/instruments/averagetype.hpp>

namespace ore {
namespace data {

void AsianOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    if (underlying_->type() == "Equity") {
        additionalData_["isdaAssetClass"] = string("Equity");
        additionalData_["isdaBaseProduct"] = string("Option");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");  
    } else if (underlying_->type() == "FX") {
        additionalData_["isdaAssetClass"] = string("Foreign Exchange");
        additionalData_["isdaBaseProduct"] = string("Vanilla Option");
        additionalData_["isdaSubProduct"] = string("");
    } else if (underlying_->type() == "Commodity") {
        // guessing that Commodities are treated like Equity
        additionalData_["isdaAssetClass"] = string("Commodity");
        additionalData_["isdaBaseProduct"] = string("Option");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");  
    }
    else {
        WLOG("ISDA taxonomy not set for trade " << id());
    }
    additionalData_["isdaTransaction"] = string("");  

    Currency payCcy = parseCurrency(currency_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for AsianOption");

    Option::Type type = parseOptionType(option_.callPut());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Expected exactly one exercise date");
    Date expiryDate = parseDate(option_.exerciseDates().front());

    string tradeTypeBuilder = tradeType_;

    // Add Arithmetic/Geometric

    if (option_.payoffType2() == "Arithmetic" || option_.payoffType2().empty())
        tradeTypeBuilder += "Arithmetic";
    else if (option_.payoffType2() == "Geometric")
        tradeTypeBuilder += "Geometric";
    else {
        QL_FAIL("payoff type 2 must be 'Arithmetic' or 'Geometric'");
    }

    // Add Price/Strike

    if (option_.payoffType() == "Asian")
        tradeTypeBuilder += "Price";
    else if (option_.payoffType() == "AverageStrike")
        tradeTypeBuilder += "Strike";
    else {
        QL_FAIL("payoff type must be 'Asian' or 'AverageStrike'");
    }

    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeTypeBuilder);
    QL_REQUIRE(builder, "No builder found for " << tradeTypeBuilder);

    // check for delegating engine builder

    if (auto db = QuantLib::ext::dynamic_pointer_cast<DelegatingEngineBuilder>(builder)) {

        // let the delegating builder build the trade and link the results to this trade

        delegatingBuilderTrade_ = db->build(this, engineFactory);

        instrument_ = delegatingBuilderTrade_->instrument();
        maturity_ = delegatingBuilderTrade_->maturity();
        npvCurrency_ = delegatingBuilderTrade_->npvCurrency();
        additionalData_ = delegatingBuilderTrade_->additionalData();
	requiredFixings_ = delegatingBuilderTrade_->requiredFixings();
        setSensitivityTemplate(delegatingBuilderTrade_->sensitivityTemplate());

        // notional and notional currency are defined in overriden methods!

        return;
    }

    // we do not have a delegating engine builder

    QuantLib::ext::shared_ptr<AsianOptionEngineBuilder> asianOptionBuilder =
        QuantLib::ext::dynamic_pointer_cast<AsianOptionEngineBuilder>(builder);

    QL_REQUIRE(asianOptionBuilder, "engine builder is not an AsianOption engine builder" << tradeTypeBuilder);

    std::string processType = asianOptionBuilder->processType();
    QL_REQUIRE(!processType.empty(), "ProcessType must be configured, this is unexpected");

    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, tradeStrike_.value()));

    auto index = parseIndex(indexName());

    if (auto fxIndex = QuantLib::ext::dynamic_pointer_cast<QuantExt::FxIndex>(index)) {
        QL_REQUIRE(fxIndex->targetCurrency() == payCcy,
                   "FX domestic ccy " << fxIndex->targetCurrency() << " must match pay ccy " << payCcy);
        assetName_ = fxIndex->sourceCurrency().code();
    } else if (auto eqIndex = QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityIndex2>(index)) {
        // FIXME for EQ and COMM indices check whether EQ, COMM ccy = payCcy (in the engine builders probably)
        assetName_ = eqIndex->name();
    } else if (auto commIndex = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
        assetName_ = commIndex->underlyingName();
    }

    // FIXME the engine should handle the historical part of the averaging as well!
    QuantLib::ext::shared_ptr<QuantLib::Instrument> asian;
    auto exercise = QuantLib::ext::make_shared<QuantLib::EuropeanExercise>(expiryDate);
    if (processType == "Discrete") {
        QuantLib::Date today = engineFactory->market()->asofDate();
        Real runningAccumulator = option_.payoffType2() == "Geometric" ? 1.0 : 0.0;
        Size pastFixings = 0;
        Schedule observationSchedule = makeSchedule(observationDates_);
        std::vector<QuantLib::Date> observationDates = observationSchedule.dates();

        // Sort for the engine's sake. Not needed - instrument also sorts...?
        std::sort(observationDates.begin(), observationDates.end());

        for (QuantLib::Date observationDate : observationDates) {
            // TODO: Verify. Should today be read too? a enforcesTodaysHistoricFixings() be used?
            if (observationDate < today ||
                (observationDate == today && Settings::instance().enforcesTodaysHistoricFixings())) {
                // FIXME all observation dates lead to a required fixing
                requiredFixings_.addFixingDate(observationDate, indexName());
                Real fixingValue = index->fixing(observationDate);
                if (option_.payoffType2() == "Geometric") {
                    runningAccumulator *= fixingValue;
                } else if (option_.payoffType2() == "Arithmetic") {
                    runningAccumulator += fixingValue;
                }
                ++pastFixings;
            }
        }
        asian = QuantLib::ext::make_shared<QuantLib::DiscreteAveragingAsianOption>(
            option_.payoffType2() == "Geometric" ? QuantLib::Average::Type::Geometric
                                                 : QuantLib::Average::Type::Arithmetic,
            runningAccumulator, pastFixings, observationDates, payoff, exercise);
    } else if (processType == "Continuous") {
        // FIXME how is the accumulated average handled in this case?
        asian = QuantLib::ext::make_shared<QuantLib::ContinuousAveragingAsianOption>(option_.payoffType2() == "Geometric"
                                                                                 ? QuantLib::Average::Type::Geometric
                                                                                 : QuantLib::Average::Type::Arithmetic,
                                                                             payoff, exercise);
    } else {
        QL_FAIL("unexpected ProcessType, valid options are Discrete/Continuous");
    }

    // Only try to set an engine on the option instrument if it is not expired. This avoids errors in
    // engine builders that rely on the expiry date being in the future.
    string configuration = asianOptionBuilder->configuration(MarketContext::pricing);
    if (!asian->isExpired()) {
        asian->setPricingEngine(asianOptionBuilder->engine(assetName_, payCcy, expiryDate));
        setSensitivityTemplate(*asianOptionBuilder);
    } else {
        DLOG("No engine attached for option on trade " << id() << " with expiry date " << io::iso_date(expiryDate)
                                                       << " because it is expired.");
    }

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = quantity_ * bsInd;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    maturity_ = expiryDate;
    maturity_ =
        std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(),
                                        positionType == QuantLib::Position::Long ? -1.0 : 1.0, payCcy, engineFactory,
                                        configuration));

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(asian, mult, additionalInstruments, additionalMultipliers);

    npvCurrency_ = currency_;
    notional_ = tradeStrike_.value() * quantity_;
    notionalCurrency_ = currency_;
}

void AsianOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* n = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(n, "No " + tradeType_ + "Data node found.");

    quantity_ = XMLUtils::getChildValueAsDouble(n, "Quantity", true);

    tradeStrike_.fromXML(n);

    currency_ = XMLUtils::getChildValue(n, "Currency", false);

    XMLNode* tmp = XMLUtils::getChildNode(n, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(n, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    option_.fromXML(XMLUtils::getChildNode(n, "OptionData"));

    settlementDate_ = parseDate(XMLUtils::getChildValue(n, "Settlement", false));

    observationDates_.fromXML(XMLUtils::getChildNode(n, "ObservationDates"));
}

XMLNode* AsianOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* n = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, n);
    XMLUtils::addChild(doc, n, "Quantity", quantity_);
    XMLUtils::appendNode(n, tradeStrike_.toXML(doc));
    XMLUtils::addChild(doc, n, "Currency", currency_);
    XMLUtils::appendNode(n, underlying_->toXML(doc));
    XMLUtils::appendNode(n, option_.toXML(doc));
    if (settlementDate_ != Null<Date>())
        XMLUtils::addChild(doc, n, "Settlement", ore::data::to_string(settlementDate_));
    auto tmp = observationDates_.toXML(doc);
    XMLUtils::setNodeName(doc, tmp, "ObservationDates");
    XMLUtils::appendNode(n, tmp);
    return node;
}

std::map<AssetClass, std::set<std::string>>
AsianOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    if (isEquityIndex(indexName())) {
        result[AssetClass::EQ].insert(indexName());
    } else if (isFxIndex(indexName_)) {
        result[AssetClass::FX].insert(indexName());
    } else if (isCommodityIndex(indexName_)) {
        result[AssetClass::COM].insert(indexName());
    }
    return result;
}

void AsianOption::populateIndexName() const {
    if (!indexName_.empty())
        return;
    if (underlying_->type() == "Equity") {
        indexName_ = "EQ-" + underlying_->name();
    } else if (underlying_->type() == "FX") {
        indexName_ = "FX-" + underlying_->name();
    } else if (underlying_->type() == "Commodity") {
        QuantLib::ext::shared_ptr<CommodityUnderlying> comUnderlying =
            QuantLib::ext::dynamic_pointer_cast<CommodityUnderlying>(underlying_);
        std::string tmp = "COMM-" + comUnderlying->name();
        if (comUnderlying->priceType().empty() || comUnderlying->priceType() == "Spot") {
            indexName_ = tmp;
        } else if (comUnderlying->priceType() == "FutureSettlement") {
            auto conventions = InstrumentConventions::instance().conventions();
            QL_REQUIRE(conventions->has(comUnderlying->name()),
                       "future settlement requires conventions for commodity '" << comUnderlying->name() << "'");
            auto convention =
                QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(comUnderlying->name()));
            Size futureMonthsOffset =
                comUnderlying->futureMonthOffset() == Null<Size>() ? 0 : comUnderlying->futureMonthOffset();
            Size deliveryRollDays =
                comUnderlying->deliveryRollDays() == Null<Size>() ? 0 : comUnderlying->deliveryRollDays();
            Calendar cal = parseCalendar(comUnderlying->deliveryRollCalendar());
            ConventionsBasedFutureExpiry expiryCalculator(*convention);
            QL_REQUIRE(option_.exerciseDates().size() == 1, "expected exactly one exercise date");
            Date expiryDate = parseDate(option_.exerciseDates().front());
            Date adjustedObsDate =
                deliveryRollDays != 0 ? cal.advance(expiryDate, deliveryRollDays * Days) : expiryDate;
            auto tmp = parseCommodityIndex(comUnderlying->name(), false, Handle<QuantExt::PriceTermStructure>(),
                                           convention->calendar(), true);
            tmp = tmp->clone(expiryCalculator.nextExpiry(true, adjustedObsDate, futureMonthsOffset));
            indexName_ = tmp->name();
        } else {
            QL_FAIL("underlying price type '" << comUnderlying->priceType() << "' for commodity underlying '"
                                              << comUnderlying->name() << "' not handled.");
        }
    } else if (underlying_->type() == "Basic") {
        indexName_ = underlying_->name();
    } else {
        QL_FAIL("invalid underlying type: " << underlying_->type());
    }
}

QuantLib::Real AsianOption::notional() const {
    return delegatingBuilderTrade_ != nullptr ? delegatingBuilderTrade_->notional() : Trade::notional();
}

string AsianOption::notionalCurrency() const {
    return delegatingBuilderTrade_ != nullptr ? delegatingBuilderTrade_->notionalCurrency() : Trade::notionalCurrency();
}

} // namespace data
} // namespace ore
