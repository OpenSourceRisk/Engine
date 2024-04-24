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

#include <algorithm>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/builders/commodityapo.hpp>
#include <ored/portfolio/commodityapo.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/commodityoptionstrip.hpp>
#include <ored/portfolio/commoditydigitalapo.hpp>
#include <ored/portfolio/commoditydigitaloption.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>

using namespace ore::data;
using namespace QuantExt;
using namespace QuantLib;
using std::max;
using std::string;
using std::transform;
using std::vector;

namespace {

vector<string> strPositions(const vector<Position::Type>& positions) {
    vector<string> res(positions.size());
    transform(positions.begin(), positions.end(), res.begin(), [](const Position::Type& pt) { return to_string(pt); });
    return res;
}

struct TempOptionData {
    string type;
    Position::Type position;
    Real strike;
    string id;
};

} // namespace

namespace ore {
namespace data {

CommodityOptionStrip::CommodityOptionStrip(const Envelope& envelope, const LegData& legData,
                                           const vector<Position::Type>& callPositions, const vector<Real>& callStrikes,
                                           const vector<Position::Type>& putPositions, const vector<Real>& putStrikes,
                                           Real premium, const string& premiumCurrency, const Date& premiumPayDate,
                                           const string& style, const string& settlement,
                                           const BarrierData& callBarrierData, const BarrierData& putBarrierData,
                                           const string& fxIndex, const bool isDigital, Real payoffPerUnit)
    : Trade("CommodityOptionStrip", envelope), legData_(legData), callPositions_(callPositions),
      callStrikes_(callStrikes), putPositions_(putPositions), putStrikes_(putStrikes),
      style_(style),
      settlement_(settlement),
      callBarrierData_(callBarrierData), putBarrierData_(putBarrierData), fxIndex_(fxIndex), isDigital_(isDigital),
      unaryPayoff_(payoffPerUnit) {
    if (!QuantLib::close_enough(premium, 0.0)) {
        QL_REQUIRE(premiumPayDate != Date(), "The premium is non-zero so its payment date needs to be provided");
        QL_REQUIRE(!premiumCurrency.empty(), "The premium is non-zero so its currency needs to be provided");
        premiumData_ = PremiumData(premium, premiumCurrency, premiumPayDate);
    }
}


void CommodityOptionStrip::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    reset();

    DLOG("CommodityOptionStrip::build() called for trade " << id());

    // ISDA taxonomy, assuming Commodity follows the Equity template
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string();

    npvCurrency_ = notionalCurrency_ = legData_.currency();

    // Check that the leg data is of type CommodityFloating
    auto conLegData = legData_.concreteLegData();
    commLegData_ = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(conLegData);
    QL_REQUIRE(commLegData_, "CommodityOptionStrip leg data should be of type CommodityFloating");
    if(!commLegData_->fxIndex().empty())
        fxIndex_= commLegData_->fxIndex();
    // Build the commodity floating leg data
    auto legBuilder = engineFactory->legBuilder(legData_.legType());
    auto cflb = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegBuilder>(legBuilder);
    QL_REQUIRE(cflb, "Expected a CommodityFloatingLegBuilder for leg type " << legData_.legType());
    Leg leg =
        cflb->buildLeg(legData_, engineFactory, requiredFixings_, engineFactory->configuration(MarketContext::pricing));

    // Perform checks
    check(leg.size());

    // We update the notional_ in either buildAPOs or buildStandardOptions below.
    notional_ = Null<Real>();

    // Build the strip of option trades
    legData_.concreteLegData();

    if (commLegData_->isAveraged() && !cflb->allAveraging()) {
        buildAPOs(leg, engineFactory);
    } else {
        buildStandardOptions(leg, engineFactory);
    }

    // Add leg to legs_ so that fixings method can work.
    legs_.push_back(leg);
    legPayers_.push_back(false);
    legCurrencies_.push_back(npvCurrency_);
}

std::map<ore::data::AssetClass, std::set<std::string>>
CommodityOptionStrip::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<ore::data::AssetClass, std::set<std::string>> result;

    set<string> indices = legData_.indices();
    for (auto ind : indices) {
        QuantLib::ext::shared_ptr<Index> index = parseIndex(ind);
        // only handle commodity
        if (auto ci = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
            result[ore::data::AssetClass::COM].insert(ci->name());
        }
    }
    return result;
}

void CommodityOptionStrip::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* stripNode = XMLUtils::getChildNode(node, "CommodityOptionStripData");
    QL_REQUIRE(stripNode, "No CommodityOptionStripData Node");

    legData_.fromXML(XMLUtils::getChildNode(stripNode, "LegData"));

    if (XMLNode* n = XMLUtils::getChildNode(stripNode, "Calls")) {
        vector<string> ls = XMLUtils::getChildrenValues(n, "LongShorts", "LongShort", false);
        callPositions_ = parseVectorOfValues<Position::Type>(ls, &parsePositionType);
        callStrikes_ = XMLUtils::getChildrenValuesAsDoubles(n, "Strikes", "Strike", false);
        if (XMLNode* n2 = XMLUtils::getChildNode(n, "BarrierData")) {
            callBarrierData_.fromXML(n2);
        }
    }

    if (XMLNode* n = XMLUtils::getChildNode(stripNode, "Puts")) {
        vector<string> ls = XMLUtils::getChildrenValues(n, "LongShorts", "LongShort", false);
        putPositions_ = parseVectorOfValues<Position::Type>(ls, &parsePositionType);
        putStrikes_ = XMLUtils::getChildrenValuesAsDoubles(n, "Strikes", "Strike", false);
        if (XMLNode* n2 = XMLUtils::getChildNode(n, "BarrierData")) {
            putBarrierData_.fromXML(n2);
        }
    }
    premiumData_.fromXML(stripNode);
    style_ = "";
    if (XMLNode* n = XMLUtils::getChildNode(stripNode, "Style"))
        style_ = XMLUtils::getNodeValue(n);

    settlement_ = "";
    if (XMLNode* n = XMLUtils::getChildNode(stripNode, "Settlement"))
        settlement_ = XMLUtils::getNodeValue(n);

    isDigital_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(stripNode, "IsDigital") )
        isDigital_ = parseBool(XMLUtils::getNodeValue(n));
    if (isDigital_) {
        XMLNode* n = XMLUtils::getChildNode(stripNode, "PayoffPerUnit");
        QL_REQUIRE(n, "A strip of commodity digital options requires PayoffPerUnit node");
        unaryPayoff_ = parseReal(XMLUtils::getNodeValue(n));
    }
}

XMLNode* CommodityOptionStrip::toXML(XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);

    XMLNode* stripNode = doc.allocNode("CommodityOptionStripData");
    XMLUtils::appendNode(node, stripNode);

    XMLUtils::appendNode(stripNode, legData_.toXML(doc));

    if (!callStrikes_.empty()) {
        XMLNode* callsNode = doc.allocNode("Calls");
        XMLUtils::addChildren(doc, callsNode, "LongShorts", "LongShort", strPositions(callPositions_));
        XMLUtils::addChildren(doc, callsNode, "Strikes", "Strike", callStrikes_);
        if (callBarrierData_.initialized()) {
            XMLUtils::appendNode(callsNode, callBarrierData_.toXML(doc));
        }
        XMLUtils::appendNode(stripNode, callsNode);
    }

    if (!putStrikes_.empty()) {
        XMLNode* putsNode = doc.allocNode("Puts");
        XMLUtils::addChildren(doc, putsNode, "LongShorts", "LongShort", strPositions(putPositions_));
        XMLUtils::addChildren(doc, putsNode, "Strikes", "Strike", putStrikes_);
        if (putBarrierData_.initialized()) {
            XMLUtils::appendNode(putsNode, putBarrierData_.toXML(doc));
        }
        XMLUtils::appendNode(stripNode, putsNode);
    }

    // These are all optional, really they should be grouped here
    if (!premiumData_.premiumData().empty())
        XMLUtils::appendNode(stripNode, premiumData_.toXML(doc));

    if (!style_.empty())
        XMLUtils::addChild(doc, stripNode, "Style", style_);

    if (!settlement_.empty())
        XMLUtils::addChild(doc, stripNode, "Settlement", settlement_);

    if (isDigital_){
        XMLUtils::addChild(doc, stripNode, "IsDigital", isDigital_);
        XMLUtils::addChild(doc, stripNode, "PayoffPerUnit", unaryPayoff_);
    }
    return node;
}

void CommodityOptionStrip::buildAPOs(const Leg& leg, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // If style is set and not European, log a warning.
    if (!style_.empty() && style_ != "European") {
        WLOG("Style should be European when the commodity option strip is a strip of APOs. Ignoring style "
             << style_ << " and proceeding as if European.");
    }

    // If settlement is set and not Cash, log a warning. Physical settlement for APOs does not make sense.
    if (!settlement_.empty() && settlement_ != "Cash") {
        WLOG("Settlement should be Cash when the commodity option strip is a strip of APOs. Ignoring settlement "
             << settlement_ << " and proceeding as if Cash.");
    }


    // Populate these with the call/put options requested in each period
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;

    for (Size i = 0; i < leg.size(); i++) {
        auto cf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(leg[i]);
        QL_REQUIRE(cf, "Expected a CommodityIndexedAverageCashFlow while building APO");

        // Populate call and/or put data at this leg period
        vector<TempOptionData> tempData;
        Date exerciseDate = cf->indices().rbegin()->first;
        vector<string> strExerciseDate = {to_string(exerciseDate)};
        maturity_ = maturity_ == Date() ? cf->date() : max(maturity_, cf->date());
        string stemId = id() + "_" + strExerciseDate[0] + "_";
        if (!callStrikes_.empty()) {
            Position::Type position = callPositions_.size() == 1 ? callPositions_[0] : callPositions_[i];
            Real strike = callStrikes_.size() == 1 ? callStrikes_[0] : callStrikes_[i];
            string id = stemId + "call";
            tempData.push_back({"Call", position, strike, id});
        }
        if (!putStrikes_.empty()) {
            Position::Type position = putPositions_.size() == 1 ? putPositions_[0] : putPositions_[i];
            Real strike = putStrikes_.size() == 1 ? putStrikes_[0] : putStrikes_[i];
            string id = stemId + "put";
            tempData.push_back({"Put", position, strike, id});
        }

        // Each CommodityAveragePriceOption is set up to go through the commodity floating leg builder which for
        // averaging cashflows includes the start date on the first coupon in the leg and includes the end date on
        // the last coupon in the leg. Only one coupon in each of the cases here so need to do this manually here.
        Date start = cf->indices().begin()->first;

        // Build a commodity APO for the call and/or put in this period
        
        for (const auto& tempDatum : tempData) {
            QuantLib::ext::shared_ptr<Trade> commOption;
            OptionData optionData(to_string(tempDatum.position), tempDatum.type, "European", true, strExerciseDate);
             if (!isDigital()) {
                commOption = QuantLib::ext::make_shared<CommodityAveragePriceOption>(
                    envelope(), optionData, cf->quantity(), tempDatum.strike, legData_.currency(), commLegData_->name(),
                    commLegData_->priceType(), to_string(start), to_string(cf->endDate()), legData_.paymentCalendar(),
                    legData_.paymentLag(), legData_.paymentConvention(), commLegData_->pricingCalendar(),
                    to_string(cf->date()), cf->gearing(), cf->spread(), cf->quantityFrequency(),
                    CommodityPayRelativeTo::CalculationPeriodEndDate, commLegData_->futureMonthOffset(),
                    commLegData_->deliveryRollDays(), true,
                    tempDatum.type == "Call" ? callBarrierData_ : putBarrierData_, fxIndex_);
             } else {
                 auto undCcy = cf->index()->priceCurve()->currency();
                 QL_REQUIRE(undCcy.code() == legData_.currency(),
                            "Strips of commodity digital options do not support intra-currency trades yet.");
                 commOption = QuantLib::ext::make_shared<CommodityDigitalAveragePriceOption>(
                     envelope(), optionData, tempDatum.strike, cf->quantity() * payoffPerUnit(),
                     legData_.currency(), commLegData_->name(), commLegData_->priceType(), to_string(start),
                     to_string(cf->endDate()), legData_.paymentCalendar(), legData_.paymentLag(),
                     legData_.paymentConvention(), commLegData_->pricingCalendar(), to_string(cf->date()),
                     cf->gearing(), cf->spread(), cf->quantityFrequency(),
                     CommodityPayRelativeTo::CalculationPeriodEndDate, commLegData_->futureMonthOffset(),
                     commLegData_->deliveryRollDays(), true,
                     tempDatum.type == "Call" ? callBarrierData_ : putBarrierData_, fxIndex_);
             }
            commOption->id() = tempDatum.id;
            commOption->build(engineFactory);
            QuantLib::ext::shared_ptr<InstrumentWrapper> instWrapper = commOption->instrument();
            setSensitivityTemplate(commOption->sensitivityTemplate());
            additionalInstruments.push_back(instWrapper->qlInstrument());
            additionalMultipliers.push_back(instWrapper->multiplier());

            // Update the notional_ each time. It will hold the notional of the last instrument which is arbitrary
            // but reasonable as this is the instrument that we use as the main instrument below.
            notional_ = commOption->notional();

            if (!fxIndex_.empty()) { // if fx is applied, the notional is still quoted in the domestic ccy
                auto underlyingCcy = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(leg[0])->index()->priceCurve()->currency();
                notionalCurrency_ = underlyingCcy.code();
            }
        }
    }

    QL_REQUIRE(additionalInstruments.size() > 0, "Expected commodity APO strip to have at least one instrument");

    // Take the last option and multiplier as the main instrument
    auto qlInst = additionalInstruments.back();
    auto qlInstMult = additionalMultipliers.back();
    additionalInstruments.pop_back();
    additionalMultipliers.pop_back();

    // Possibly add a premium to the additional instruments and multipliers
    // We expect here that the fee already has the correct sign

    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, qlInstMult, premiumData_,
                                                1.0, parseCurrency(legData_.currency()), engineFactory, ""));
    
    // Create the Trade's instrument wrapper
    instrument_ =
        QuantLib::ext::make_shared<VanillaInstrument>(qlInst, qlInstMult, additionalInstruments, additionalMultipliers);
}

void CommodityOptionStrip::buildStandardOptions(const Leg& leg, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    QL_REQUIRE(!callBarrierData_.initialized(), "Commodity APO: standard option does not support barriers");
    QL_REQUIRE(!putBarrierData_.initialized(), "Commodity APO: standard option does not support barriers");

    // Set style and settlement
    string style = style_.empty() ? "European" : style_;
    string settlement = settlement_.empty() ? "Cash" : settlement_;

    // Set automatic exercise to true for cash settlement.
    bool automaticExercise = settlement == "Cash";

    // Populate these with the call/put options requested in each period
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;

    for (Size i = 0; i < leg.size(); i++) {

        auto cf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(leg[i]);
        QL_REQUIRE(cf, "Expected a CommodityIndexedCashFlow while building standard option");

        // Exercise date is the pricing date.
        Date exerciseDate = cf->pricingDate();
        vector<string> strExerciseDate = {to_string(exerciseDate)};

        // If cash settlement and European, create the OptionPaymentData for the OptionData block below.
        boost::optional<OptionPaymentData> paymentData = boost::none;
        if (settlement == "Cash" && style == "European") {

            Date paymentDate = cf->date();
            vector<string> strPaymentDate = {to_string(paymentDate)};
            paymentData = OptionPaymentData(strPaymentDate);

            // Update the maturity - cash-settled strip, it is the maximum payment date.
            maturity_ = maturity_ == Date() ? paymentDate : max(maturity_, paymentDate);

        } else {
            // Update the maturity - physically-settled strip or American, it is the maximum
            // exercise date (no deferred delivery for Physical implemented yet).
            maturity_ = maturity_ == Date() ? exerciseDate : max(maturity_, exerciseDate);
        }

        // Populate call and/or put data at this leg period
        vector<TempOptionData> tempData;
        string stemId = id() + "_" + strExerciseDate[0] + "_";
        if (!callStrikes_.empty()) {
            Position::Type position = callPositions_.size() == 1 ? callPositions_[0] : callPositions_[i];
            Real strike = callStrikes_.size() == 1 ? callStrikes_[0] : callStrikes_[i];
            string id = stemId + "call";
            tempData.push_back({"Call", position, strike, id});
        }
        if (!putStrikes_.empty()) {
            Position::Type position = putPositions_.size() == 1 ? putPositions_[0] : putPositions_[i];
            Real strike = putStrikes_.size() == 1 ? putStrikes_[0] : putStrikes_[i];
            string id = stemId + "put";
            tempData.push_back({"Put", position, strike, id});
        }

        // Build a commodity option for the call and/or put in this period
        for (const auto& tempDatum : tempData) {

            // Check that gearing, strike and spread make sense
            QL_REQUIRE(cf->gearing() > 0.0, "Gearing (" << cf->gearing() << ") should be positive.");
            QL_REQUIRE(cf->spread() < tempDatum.strike || QuantLib::close_enough(cf->spread(), tempDatum.strike),
                       "Spread (" << cf->spread() << ") should be less than strike (" << tempDatum.strike << ").");

            TradeStrike effectiveStrike(TradeStrike::Type::Price, (tempDatum.strike - cf->spread()) / cf->gearing());
            Real effectiveQuantity = cf->gearing() * cf->periodQuantity();

            OptionData optionData(to_string(tempDatum.position), tempDatum.type, style, false, strExerciseDate,
                                  settlement, "", PremiumData(), {}, {}, "", "", "", {}, {}, "", "", "", "", "",
                                  automaticExercise, boost::none, paymentData);


            QuantLib::ext::shared_ptr<Trade> commOption;
            if(!isDigital()) {
                commOption = QuantLib::ext::make_shared<CommodityOption>(
                    envelope(), optionData, commLegData_->name(), legData_.currency(), effectiveQuantity,
                    effectiveStrike, cf->useFuturePrice(), cf->index()->expiryDate());
            } else {
                auto undCcy = cf->index()->priceCurve()->currency();
                QL_REQUIRE(undCcy.code() == legData_.currency(), "Strips of commodity digital options do not support intra-currency trades yet.");
                commOption = QuantLib::ext::make_shared<CommodityDigitalOption>(
                    envelope(), optionData, commLegData_->name(), legData_.currency(), effectiveStrike.value(), effectiveQuantity*payoffPerUnit(),
                    cf->useFuturePrice(), cf->index()->expiryDate());
            }
            commOption->id() = tempDatum.id;
            commOption->build(engineFactory);
            QuantLib::ext::shared_ptr<InstrumentWrapper> instWrapper = commOption->instrument();
            setSensitivityTemplate(commOption->sensitivityTemplate());
            additionalInstruments.push_back(instWrapper->qlInstrument());
            additionalMultipliers.push_back(instWrapper->multiplier());

            // Update the notional_ each time. It will hold the notional of the last instrument which is arbitrary
            // but reasonable as this is the instrument that we use as the main instrument below.
            notional_ = commOption->notional();
        }
    }

    QL_REQUIRE(additionalInstruments.size() > 0, "Expected commodity option strip to have at least one instrument");

    // Take the last option and multiplier as the main instrument
    auto qlInst = additionalInstruments.back();
    auto qlInstMult = additionalMultipliers.back();
    additionalInstruments.pop_back();
    additionalMultipliers.pop_back();

    // Possibly add a premium to the additional instruments and multipliers
    // We expect here that the fee already has the correct sign
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, qlInstMult, premiumData_,
                                                1.0, parseCurrency(legData_.currency()), engineFactory, ""));
    DLOG("Option premium added for commodity option strip " << id());
    
    // Create the Trade's instrument wrapper
    instrument_ =
        QuantLib::ext::make_shared<VanillaInstrument>(qlInst, qlInstMult, additionalInstruments, additionalMultipliers);
}

void CommodityOptionStrip::check(Size numberPeriods) const {

    QL_REQUIRE(numberPeriods > 0, "Expected at least one period in the commodity option strip");
    QL_REQUIRE(!callStrikes_.empty() || !putStrikes_.empty(), "Need at least one call or put to build a strip");

    if (!callStrikes_.empty()) {
        QL_REQUIRE(callStrikes_.size() == 1 || callStrikes_.size() == numberPeriods,
                   "The number of "
                       << "call strikes (" << callStrikes_.size() << ") should be 1 or equal to "
                       << "the number of periods in the strip (" << numberPeriods << ")");
        QL_REQUIRE(callPositions_.size() == 1 || callPositions_.size() == numberPeriods,
                   "The number of position "
                       << "flags provided with the call strikes (" << callPositions_.size()
                       << ") should be 1 or equal to "
                       << "the number of periods in the strip (" << numberPeriods << ")");
    }

    if (!putStrikes_.empty()) {
        QL_REQUIRE(putStrikes_.size() == 1 || putStrikes_.size() == numberPeriods,
                   "The number of "
                       << "put strikes (" << putStrikes_.size() << ") should be 1 or equal to "
                       << "the number of periods in the strip (" << numberPeriods << ")");
        QL_REQUIRE(putPositions_.size() == 1 || putPositions_.size() == numberPeriods,
                   "The number of position "
                       << "flags provided with the put strikes (" << putPositions_.size()
                       << ") should be 1 or equal to "
                       << "the number of periods in the strip (" << numberPeriods << ")");
    }
}

} // namespace data
} // namespace ore
