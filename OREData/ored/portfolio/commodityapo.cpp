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

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/builders/commodityapo.hpp>
#include <ored/portfolio/commodityapo.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/marketdata.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/instruments/commodityapo.hpp>

using namespace QuantExt;
using namespace QuantLib;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace data {

CommodityAveragePriceOption::CommodityAveragePriceOption(
    const Envelope& envelope, const OptionData& optionData, Real quantity, Real strike, const string& currency,
    const string& name, CommodityPriceType priceType, const string& startDate, const string& endDate,
    const string& paymentCalendar, const string& paymentLag, const string& paymentConvention,
    const string& pricingCalendar, const string& paymentDate, Real gearing, Spread spread,
    CommodityQuantityFrequency commodityQuantityFrequency, CommodityPayRelativeTo commodityPayRelativeTo,
    QuantLib::Natural futureMonthOffset, QuantLib::Natural deliveryRollDays, bool includePeriodEnd,
    const BarrierData& barrierData, const std::string& fxIndex)
    : Trade("CommodityAveragePriceOption", envelope), optionData_(optionData), barrierData_(barrierData),
      quantity_(quantity), strike_(strike), currency_(currency), name_(name), priceType_(priceType),
      startDate_(startDate), endDate_(endDate), paymentCalendar_(paymentCalendar), paymentLag_(paymentLag),
      paymentConvention_(paymentConvention), pricingCalendar_(pricingCalendar), paymentDate_(paymentDate),
      gearing_(gearing), spread_(spread), commodityQuantityFrequency_(commodityQuantityFrequency),
      commodityPayRelativeTo_(commodityPayRelativeTo), futureMonthOffset_(futureMonthOffset),
      deliveryRollDays_(deliveryRollDays), includePeriodEnd_(includePeriodEnd), fxIndex_(fxIndex), allAveraging_(false) {}

void CommodityAveragePriceOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    reset();


    DLOG("CommodityAveragePriceOption::build() called for trade " << id());

    // ISDA taxonomy, assuming Commodity follows the Equity template
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string();

    QL_REQUIRE(gearing_ > 0.0, "Gearing (" << gearing_ << ") should be positive.");
    QL_REQUIRE(spread_ < strike_ || QuantLib::close_enough(spread_, strike_),
               "Spread (" << spread_ << ") should be less than strike (" << strike_ << ").");

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = strike_;
    additionalData_["strikeCurrency"] = currency_;

    // Notional = effective_quantity * effective_strike = (G x Q) x ((K - s) / G) = Q x (K - s)
    notional_ = quantity_ * (strike_ - spread_);
    notionalCurrency_ = npvCurrency_ = currency_;

    // Allow exercise dates not to be specified for an APO. In this case, the exercise date is deduced below when
    // building the APO or a standard option.
    Date exDate;
    if (!optionData_.exerciseDates().empty()) {
        QL_REQUIRE(optionData_.exerciseDates().size() == 1, "Commodity average price option must be European");
        exDate = parseDate(optionData_.exerciseDates().front());
    }

    // Just to get the configuration string for now
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(
        barrierData_.initialized() ? "CommodityAveragePriceBarrierOption" : "CommodityAveragePriceOption");
    string configuration = builder->configuration(MarketContext::pricing);

    // Build the leg. In this method the allAveraging_ flag is set to true if the future itself is averaging and we
    // can then use a standard commodity option pricer below.
    Leg leg = buildLeg(engineFactory, configuration);

    // Based on allAveraging_ flag, set up a standard or averaging commodity option
    if (allAveraging_) {
        buildStandardOption(engineFactory, leg, exDate);
    } else {
        buildApo(engineFactory, leg, exDate, builder);
    }

    // Add leg to legs_ so that fixings method can work.
    legs_.push_back(leg);
    legPayers_.push_back(false);
    legCurrencies_.push_back(currency_);
}

std::map<AssetClass, std::set<std::string>> CommodityAveragePriceOption::underlyingIndices(
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({name_})}};
}

void CommodityAveragePriceOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* apoNode = XMLUtils::getChildNode(node, "CommodityAveragePriceOptionData");
    QL_REQUIRE(apoNode, "No CommodityAveragePriceOptionData Node");

    optionData_.fromXML(XMLUtils::getChildNode(apoNode, "OptionData"));
    if(auto b = XMLUtils::getChildNode(apoNode, "BarrierData")) {
        barrierData_.fromXML(b);
    }
    name_ = XMLUtils::getChildValue(apoNode, "Name", true);
    currency_ = XMLUtils::getChildValue(apoNode, "Currency", true);
    quantity_ = XMLUtils::getChildValueAsDouble(apoNode, "Quantity", true);
    strike_ = XMLUtils::getChildValueAsDouble(apoNode, "Strike", true);
    priceType_ = parseCommodityPriceType(XMLUtils::getChildValue(apoNode, "PriceType", true));
    startDate_ = XMLUtils::getChildValue(apoNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(apoNode, "EndDate", true);
    paymentCalendar_ = XMLUtils::getChildValue(apoNode, "PaymentCalendar", true);
    paymentLag_ = XMLUtils::getChildValue(apoNode, "PaymentLag", true);
    paymentConvention_ = XMLUtils::getChildValue(apoNode, "PaymentConvention", true);
    pricingCalendar_ = XMLUtils::getChildValue(apoNode, "PricingCalendar", true);

    paymentDate_ = XMLUtils::getChildValue(apoNode, "PaymentDate", false);

    gearing_ = 1.0;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "Gearing")) {
        gearing_ = parseReal(XMLUtils::getNodeValue(n));
    }

    spread_ = XMLUtils::getChildValueAsDouble(apoNode, "Spread", false);

    commodityQuantityFrequency_ = CommodityQuantityFrequency::PerCalculationPeriod;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "CommodityQuantityFrequency")) {
        commodityQuantityFrequency_ = parseCommodityQuantityFrequency(XMLUtils::getNodeValue(n));
    }

    commodityPayRelativeTo_ = CommodityPayRelativeTo::CalculationPeriodEndDate;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "CommodityPayRelativeTo")) {
        commodityPayRelativeTo_ = parseCommodityPayRelativeTo(XMLUtils::getNodeValue(n));
    }

    futureMonthOffset_ = XMLUtils::getChildValueAsInt(apoNode, "FutureMonthOffset", false);
    deliveryRollDays_ = XMLUtils::getChildValueAsInt(apoNode, "DeliveryRollDays", false);

    includePeriodEnd_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "IncludePeriodEnd")) {
        includePeriodEnd_ = parseBool(XMLUtils::getNodeValue(n));
    }

    if (XMLNode* n = XMLUtils::getChildNode(apoNode, "FXIndex")){
        fxIndex_ = XMLUtils::getNodeValue(n);
    }
}

XMLNode* CommodityAveragePriceOption::toXML(XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* apoNode = doc.allocNode("CommodityAveragePriceOptionData");
    XMLUtils::appendNode(node, apoNode);

    XMLUtils::appendNode(apoNode, optionData_.toXML(doc));
    if (barrierData_.initialized())
        XMLUtils::appendNode(apoNode, barrierData_.toXML(doc));
    XMLUtils::addChild(doc, apoNode, "Name", name_);
    XMLUtils::addChild(doc, apoNode, "Currency", currency_);
    XMLUtils::addChild(doc, apoNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, apoNode, "Strike", strike_);
    XMLUtils::addChild(doc, apoNode, "PriceType", to_string(priceType_));
    XMLUtils::addChild(doc, apoNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, apoNode, "EndDate", endDate_);
    XMLUtils::addChild(doc, apoNode, "PaymentCalendar", paymentCalendar_);
    XMLUtils::addChild(doc, apoNode, "PaymentLag", paymentLag_);
    XMLUtils::addChild(doc, apoNode, "PaymentConvention", paymentConvention_);
    XMLUtils::addChild(doc, apoNode, "PricingCalendar", pricingCalendar_);
    XMLUtils::addChild(doc, apoNode, "PaymentDate", paymentDate_);
    XMLUtils::addChild(doc, apoNode, "Gearing", gearing_);
    XMLUtils::addChild(doc, apoNode, "Spread", spread_);
    XMLUtils::addChild(doc, apoNode, "CommodityQuantityFrequency", to_string(commodityQuantityFrequency_));
    XMLUtils::addChild(doc, apoNode, "CommodityPayRelativeTo", to_string(commodityPayRelativeTo_));
    XMLUtils::addChild(doc, apoNode, "FutureMonthOffset", static_cast<int>(futureMonthOffset_));
    XMLUtils::addChild(doc, apoNode, "DeliveryRollDays", static_cast<int>(deliveryRollDays_));
    XMLUtils::addChild(doc, apoNode, "IncludePeriodEnd", includePeriodEnd_);
    if(!fxIndex_.empty()){
        XMLUtils::addChild(doc, apoNode, "FXIndex", fxIndex_);
    }

    return node;
}

Leg CommodityAveragePriceOption::buildLeg(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                          const string& configuration) {

    // Create the ScheduleData for use in the LegData. Tenor is not needed.
    ScheduleData scheduleData(ScheduleDates("NullCalendar", "Unadjusted", "", {startDate_, endDate_}));

    // Create the CommodityFloatingLegData. Want to generate a single averaging commodity coupon.
    vector<Real> quantities{quantity_};
    vector<string> quantityDates;
    vector<Real> spreads{spread_};
    vector<string> spreadDates;
    vector<Real> gearings{gearing_};
    vector<string> gearingDates;
    vector<string> pricingDates;
    bool isAveraged = true;
    bool isInArrears = false;
    QuantLib::ext::shared_ptr<CommodityFloatingLegData> commLegData = QuantLib::ext::make_shared<CommodityFloatingLegData>(
        name_, priceType_, quantities, quantityDates, commodityQuantityFrequency_, commodityPayRelativeTo_, spreads,
        spreadDates, gearings, gearingDates, CommodityPricingDateRule::FutureExpiryDate, pricingCalendar_, 0,
        pricingDates, isAveraged, isInArrears, futureMonthOffset_, deliveryRollDays_, includePeriodEnd_, true,
        QuantLib::Null<QuantLib::Natural>(), true, "", QuantLib::Null<QuantLib::Natural>(), false, QuantLib::Null<QuantLib::Natural>(),
            fxIndex_);

    // Create the LegData. All defaults are as in the LegData ctor.
    vector<string> paymentDates = paymentDate_.empty() ? vector<string>() : vector<string>(1, paymentDate_);
    LegData legData(commLegData, true, currency_, scheduleData, "", vector<Real>(), vector<string>(),
                    paymentConvention_, false, false, false, true, "", 0, "", vector<AmortizationData>(), 
                    paymentLag_, "", paymentCalendar_, paymentDates);

    // Get the leg builder, set the allAveraging_ flag and build the leg
    auto legBuilder = engineFactory->legBuilder(legData.legType());
    QuantLib::ext::shared_ptr<CommodityFloatingLegBuilder> cflb =
        QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegBuilder>(legBuilder);
    QL_REQUIRE(cflb, "Expected a CommodityFloatingLegBuilder for leg type " << legData.legType());
    Leg leg = cflb->buildLeg(legData, engineFactory, requiredFixings_, configuration);
    allAveraging_ = cflb->allAveraging();

    return leg;
}

void CommodityAveragePriceOption::buildStandardOption(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                                      const Leg& leg, Date exerciseDate) {

    QL_REQUIRE(!barrierData_.initialized(), "Commodity APO: standard option does not support barriers");

    // Expect the leg to hold a single commodity averaging cashflow on which the option is written
    QL_REQUIRE(leg.size() == 1, "Single flow expected but found " << leg.size());
    auto flow = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(leg[0]);
    QL_REQUIRE(flow, "Expected a cashflow of type CommodityIndexedCashFlow");

    // If exercise date is given use it. If not given, take the cashflow's pricing date.
    if (exerciseDate != Date()) {
        QL_REQUIRE(exerciseDate >= flow->pricingDate(),
                   "Exercise date, " << io::iso_date(exerciseDate) << ", should be on or after the pricing date, "
                                     << io::iso_date(flow->pricingDate()));
        DLOG("buildStandardOption: explicit exercise date given for APO " << io::iso_date(exerciseDate) << ".");
    } else {
        exerciseDate = flow->pricingDate();
        optionData_.setExerciseDates({to_string(exerciseDate)});
        DLOG("buildStandardOption: set exercise date on APO to cashflow's pricing date " << io::iso_date(exerciseDate)
                                                                                         << ".");
    }
    DLOG("buildStandardOption: pricing date on APO is " << io::iso_date(flow->pricingDate()) << ".");

    // Update the optionData_ if necessary
    if (!optionData_.automaticExercise()) {
        optionData_.setAutomaticExercise(true);
        DLOG("buildStandardOption: setting automatic exercise to true on APO.");
    }

    if (!optionData_.paymentData()) {
        QL_REQUIRE(exerciseDate <= flow->date(), "Exercise date, " << io::iso_date(exerciseDate)
                                                                   << ", should be on or before payment date, "
                                                                   << io::iso_date(flow->date()));
        string strDate = to_string(flow->date());
        optionData_.setPaymentData(OptionPaymentData({strDate}));
        DLOG("buildStandardOption: setting payment date to " << strDate << " on APO.");
    } else {
        DLOG("buildStandardOption: using explicitly provided payment data on APO.");
    }

    // Build the commodity option.
    TradeStrike effectiveStrike((strike_ - spread_) / gearing_, currency_);
    Real effectiveQuantity = gearing_ * quantity_;
    CommodityOption commOption(envelope(), optionData_, name_, currency_, effectiveQuantity, effectiveStrike,
                               flow->index()->isFuturesIndex(), flow->pricingDate());
    commOption.build(engineFactory);
    setSensitivityTemplate(commOption.sensitivityTemplate());
    instrument_ = commOption.instrument();
    maturity_ = commOption.maturity();
}

void CommodityAveragePriceOption::buildApo(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const Leg& leg,
                                           Date exerciseDate, const QuantLib::ext::shared_ptr<EngineBuilder>& builder) {

    // Expect the leg to hold a single commodity averaging cashflow on which the option is written
    QL_REQUIRE(leg.size() == 1, "Single flow expected but found " << leg.size());
    auto apoFlow = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(leg[0]);
    QL_REQUIRE(apoFlow, "Expected a cashflow of type CommodityIndexedAverageCashFlow");

    // Populate relevant Trade members
    maturity_ = std::max(optionData_.premiumData().latestPremiumDate(), apoFlow->date());
    
    Date lastApoFixingDate = apoFlow->indices().rbegin()->first;

    // If exercise date is given use it. If not given, take the cashflow's last pricing date.
    if (exerciseDate != Date()) {
        QL_REQUIRE(exerciseDate >= lastApoFixingDate, "Exercise date, "
                                                          << io::iso_date(exerciseDate)
                                                          << ", should be on or after the last APO fixing date, "
                                                          << io::iso_date(lastApoFixingDate));
        DLOG("buildApo: explicit exercise date given for APO " << io::iso_date(exerciseDate) << ".");
    } else {
        exerciseDate = lastApoFixingDate;
        string strDate = to_string(lastApoFixingDate);
        optionData_.setExerciseDates({strDate});
        DLOG("buildApo: set exercise date on APO to cashflow's last pricing date " << io::iso_date(lastApoFixingDate)
                                                                                   << ".");
    }
    DLOG("buildApo: pricing date on APO is " << io::iso_date(lastApoFixingDate) << ".");

    QL_REQUIRE(exerciseDate <= apoFlow->date(), "Exercise date, " << io::iso_date(exerciseDate)
                                                                  << ", should be on or before payment date, "
                                                                  << io::iso_date(apoFlow->date()));

    // If the apo is payout and the underlying are quoted in different currencies, handle the fxIndex
    QuantLib::ext::shared_ptr<FxIndex> fxIndex;
    if(!fxIndex_.empty()) {
        auto underlyingCcy =
            QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(leg[0])->index()->priceCurve()->currency();
        // check currencies consistency
        QL_REQUIRE(npvCurrency_ == underlyingCcy.code() || npvCurrency_ == currency_, "Commodity cross-currency APO: inconsistent currencies in trade.");
        
        // only add an fx index if underlying currency differs from trade currency
        if (npvCurrency_ != underlyingCcy.code()) {
            fxIndex = buildFxIndex(fxIndex_, npvCurrency_, underlyingCcy.code(), engineFactory->market(),
                                   engineFactory->configuration(MarketContext::pricing));
            for (auto cf : leg) { // request appropriate fixings
                auto cacf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf);
                for (auto kv : cacf->indices()) {
                    if (!fxIndex->fixingCalendar().isBusinessDay(
                            kv.first)) { // If fx index is not available for the commodity pricing day,
                                         // this ensures to require the previous valid one which will be used in pricing
                                         // from fxIndex()->fixing(...)
                        Date adjustedFixingDate = fxIndex->fixingCalendar().adjust(kv.first, Preceding);
                        requiredFixings_.addFixingDate(adjustedFixingDate, fxIndex_);

                    } else {
                        requiredFixings_.addFixingDate(kv.first, fxIndex_);
                    }
                }
            }
        }
    }

    // get barrier info
    Real barrierLevel = Null<Real>();
    Barrier::Type barrierType = Barrier::DownIn;
    Exercise::Type barrierStyle = Exercise::American;
    if (barrierData_.initialized()) {
        QL_REQUIRE(barrierData_.levels().size() == 1, "Commodity APO: Expected exactly one barrier level.");
        barrierLevel = barrierData_.levels().front().value();
        barrierType = parseBarrierType(barrierData_.type());
        if (!barrierData_.style().empty()) {
            barrierStyle = parseExerciseType(barrierData_.style());
            QL_REQUIRE(barrierStyle == Exercise::European || barrierStyle == Exercise::American,
                       "Commodity APO: Expected 'European' or 'American' as barrier style");
        }
    }

    // Create the APO instrument
    QuantLib::ext::shared_ptr<QuantLib::Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);
    auto apo = QuantLib::ext::make_shared<QuantExt::CommodityAveragePriceOption>(
        apoFlow, exercise, apoFlow->periodQuantity(), strike_, parseOptionType(optionData_.callPut()),
        Settlement::Physical, Settlement::PhysicalOTC, barrierLevel, barrierType, barrierStyle, fxIndex);

    // Set the pricing engine
    Currency ccy = parseCurrency(currency_);
    auto engineBuilder = QuantLib::ext::dynamic_pointer_cast<CommodityApoBaseEngineBuilder>(builder);
    QuantLib::ext::shared_ptr<PricingEngine> engine = engineBuilder->engine(ccy, name_, id(), apo);
    apo->setPricingEngine(engine);
    setSensitivityTemplate(*engineBuilder);

    // position type and trade multiplier
    Position::Type positionType = parsePositionType(optionData_.longShort());
    Real multiplier = positionType == Position::Long ? 1.0 : -1.0;

    // Take care of fees
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, multiplier, optionData_.premiumData(),
                positionType == Position::Long ? -1.0 : 1.0, ccy, engineFactory,
                engineBuilder->configuration(MarketContext::pricing));

    // Populate instrument wrapper
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(apo, multiplier, additionalInstruments, additionalMultipliers);
}

} // namespace data
} // namespace ore
