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
#include <ored/portfolio/builders/commodityspreadoption.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/commodityspreadoption.hpp>

using namespace QuantExt;
using namespace QuantLib;
using std::max;
using std::string;
using std::vector;

namespace ore::data {

class OptionPaymentDateAdjuster {
public:
    virtual ~OptionPaymentDateAdjuster() = default;
    virtual void updatePaymentDate(const QuantLib::Date& exiryDate, Date& paymentDate) const {
    // unadjusted
    }
};


class OptionPaymentDataAdjuster : public OptionPaymentDateAdjuster {
public:
    OptionPaymentDataAdjuster(const OptionPaymentData& opd) : opd_(opd) {}

    void updatePaymentDate(const QuantLib::Date& expiryDate, Date& paymentDate) const override {
        if (opd_.rulesBased()) {
            const Calendar& cal = opd_.calendar();
            QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
            paymentDate = cal.advance(expiryDate, opd_.lag(), Days, opd_.convention());
        } else {
            const vector<Date>& dates = opd_.dates();
            QL_REQUIRE(dates.size() == 1, "Need exactly one payment date for cash settled European option.");
            paymentDate = dates[0];
        }
    }

private:
    OptionPaymentData opd_;
};

class OptionStripPaymentDateAdjuster : public OptionPaymentDateAdjuster {
public:
    OptionStripPaymentDateAdjuster(const std::vector<Date>& expiryDates,
                                   const CommoditySpreadOptionData::OptionStripData& stripData)
        : calendar_(stripData.calendar()), bdc_(stripData.bdc()), lag_(stripData.lag()) {
        optionStripSchedule_ = makeSchedule(stripData.schedule());
        latestExpiryDateInStrip_.resize(optionStripSchedule_.size(), Date());
        QL_REQUIRE(optionStripSchedule_.size() >= 2,
                   "Need at least a start and end date in the optionstripschedule. Please check the trade xml");
        // Check if the optionstrip definition include all expiries
        Date minExpiryDate = *std::min_element(expiryDates.begin(), expiryDates.end());
        Date maxExpiryDate = *std::max_element(expiryDates.begin(), expiryDates.end());
        Date minOptionStripDate =
            *std::min_element(optionStripSchedule_.dates().begin(), optionStripSchedule_.dates().end());
        Date maxOptionStripDate =
            *std::max_element(optionStripSchedule_.dates().begin(), optionStripSchedule_.dates().end());
        QL_REQUIRE(
            minOptionStripDate <= minExpiryDate && maxOptionStripDate > maxExpiryDate,
            "optionStrips ending before latest expiry date, please check the optionstrip definition in the trade xml");
        for (const auto& e : expiryDates) {
            auto it = std::upper_bound(optionStripSchedule_.begin(), optionStripSchedule_.end(), e);
            if (it != optionStripSchedule_.end()) {
                auto idx = std::distance(optionStripSchedule_.begin(), it);
                if (e > latestExpiryDateInStrip_[idx])
                    latestExpiryDateInStrip_[idx] = e;
            }
        }
    }

    void updatePaymentDate(const QuantLib::Date& expiryDate, Date& paymentDate) const override {
        auto upperBound =
            std::upper_bound(optionStripSchedule_.dates().begin(), optionStripSchedule_.dates().end(), expiryDate);
        if (upperBound != optionStripSchedule_.dates().end()) {
            size_t idx = std::distance(optionStripSchedule_.dates().begin(), upperBound);
            paymentDate = calendar_.advance(latestExpiryDateInStrip_[idx], lag_ * Days, bdc_);
        } else {
            // Skip adjustment
        }
    }

private:
    vector<Date> latestExpiryDateInStrip_;
    Schedule optionStripSchedule_;
    Calendar calendar_;
    BusinessDayConvention bdc_;
    int lag_;
};

QuantLib::ext::shared_ptr<OptionPaymentDateAdjuster> makeOptionPaymentDateAdjuster(CommoditySpreadOptionData& optionData,
                                                                           const std::vector<Date>& expiryDates) {

    if (optionData.optionStrip().has_value()) {
        return QuantLib::ext::make_shared<OptionStripPaymentDateAdjuster>(expiryDates, optionData.optionStrip().get());
    } else if (optionData.optionData().paymentData().has_value()) {
        return QuantLib::ext::make_shared<OptionPaymentDataAdjuster>(optionData.optionData().paymentData().get());
    } else {
        return QuantLib::ext::make_shared<OptionPaymentDateAdjuster>();
    }
}

void CommoditySpreadOption::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    DLOG("CommoditySpreadOption::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Other");
    additionalData_["isdaSubProduct"] = std::string("");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");

    reset();
    auto legData_ = csoData_.legData();
    auto optionData_ = csoData_.optionData();
    auto strike_ = csoData_.strike();

    QL_REQUIRE(legData_.size() == 2, "Only two legs supported");
    QL_REQUIRE(legData_[0].currency() == legData_[1].currency(), "Both legs must have same currency");
    QL_REQUIRE(legData_[0].isPayer() != legData_[1].isPayer(), "Need one payer and one receiver leg");

    if (!optionData_.style().empty()) {
        QuantLib::Exercise::Type exerciseType = parseExerciseType(optionData_.style());
        QL_REQUIRE(exerciseType == QuantLib::Exercise::Type::European, "Only European spread option supported");
    }

    maturity_ = Date();
    npvCurrency_ = legData_[0].currency();
    Size payerLegId = legData_[0].isPayer() ? 0 : 1;

    // build the relevant fxIndexes;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexes(2, nullptr);
    Currency ccy = parseCurrency(npvCurrency_);
    Option::Type optionType = parseOptionType(optionData_.callPut());

    // init engine factory builder
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    auto engineBuilder = QuantLib::ext::dynamic_pointer_cast<CommoditySpreadOptionEngineBuilder>(builder);
    // get config
    auto config = builder->configuration(MarketContext::pricing);

    // set exercise date to the pricing date of the coupon
    QL_REQUIRE(optionData_.exerciseDates().size() == 0,
               "Only European spread option supported, expiry date is end_date of the period");

    // Build the commodity legs

    for (Size i = 0; i < legData_.size();
         ++i) { // The order is important, the first leg is always the long position, the second is the short;
        legPayers_.push_back(legData_[i].isPayer());

        // build legs

        auto commLegData = (QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(legData_[i].concreteLegData()));
        QL_REQUIRE(commLegData, "CommoditySpreadOption leg data should be of type CommodityFloating");

        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        auto cflb = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegBuilder>(legBuilder);

        QL_REQUIRE(cflb, "CommoditySpreadOption: Expected a CommodityFloatingLegBuilder for leg "
                             << i << " but got " << legData_[i].legType());
        Leg leg = cflb->buildLeg(legData_[i], engineFactory, requiredFixings_, config);

        // setup the cf indexes
        QL_REQUIRE(!leg.empty(), "CommoditySpreadOption: Leg " << i << " has no coupons");
        auto index = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(leg.front())->index();

        // check ccy consistency
        auto underlyingCcy = index->priceCurve()->currency();
        auto tmpFx = commLegData->fxIndex();
        if (tmpFx.empty()) {
            QL_REQUIRE(underlyingCcy.code() == npvCurrency_,
                       "CommoditySpreadOption, inconsistent currencies: Settlement currency is "
                           << npvCurrency_ << ", leg " << i + 1 << " currency " << legData_[i].currency()
                           << ", underlying currency " << underlyingCcy << ", no FxIndex provided");
        } else {
            QL_REQUIRE(underlyingCcy.code() != npvCurrency_,
                       "CommoditySpreadOption, inconsistent currencies: Settlement currency is "
                           << npvCurrency_ << ", leg " << i + 1 << " currency " << legData_[i].currency()
                           << ", underlying currency " << underlyingCcy << ", FxIndex " << tmpFx << "provided");
            auto domestic = npvCurrency_;
            auto foreign = underlyingCcy.code();
            fxIndexes[i] = buildFxIndex(tmpFx, domestic, foreign, engineFactory->market(),
                                        engineFactory->configuration(MarketContext::pricing));
            // update required fixings. This is handled automatically in the leg builder in the averaging case
            if (commLegData->isAveraged()) {
                for (auto cf : leg) {
                    auto fixingDate = cf->date();
                    if (!fxIndexes[i]->fixingCalendar().isBusinessDay(
                            fixingDate)) { // If fx index is not available for the cashflow pricing day,
                        // this ensures to require the previous valid one which will be used in pricing
                        // from fxIndex()->fixing(...)
                        Date adjustedFixingDate = fxIndexes[i]->fixingCalendar().adjust(fixingDate, Preceding);
                        requiredFixings_.addFixingDate(adjustedFixingDate, tmpFx);
                    } else {
                        requiredFixings_.addFixingDate(fixingDate, tmpFx);
                    }
                }
            }
        }
        legs_.push_back(leg);
        legCurrencies_.push_back(legData_[i].currency()); // all legs and cf are priced with the same ccy
    }

    QL_REQUIRE(legs_[0].size() == legs_[1].size(),
               "CommoditySpreadOption: the two legs must contain the same number of options.");

    QL_REQUIRE(legs_[0].size() > 0, "CommoditySpreadOption: need at least one option, please check the trade xml");

    Position::Type positionType = parsePositionType(optionData_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    QuantLib::ext::shared_ptr<QuantLib::Instrument> firstInstrument;
    double firstMultiplier = 0.0;
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;

    vector<Date> expiryDates;
    for (size_t i = 0; i < legs_[0].size(); ++i) {
        auto longFlow = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[1 - payerLegId][i]);
        auto shortFlow = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[payerLegId][i]);
        expiryDates.push_back(std::max(longFlow->lastPricingDate(), shortFlow->lastPricingDate()));
    }

    auto paymentDateAdjuster = makeOptionPaymentDateAdjuster(csoData_, expiryDates);

    for (size_t i = 0; i < legs_[0].size(); ++i) {
        auto longFlow = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[1 - payerLegId][i]);
        auto shortFlow = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[payerLegId][i]);

        QuantLib::Real quantity = longFlow->periodQuantity();

        QL_REQUIRE(quantity == shortFlow->periodQuantity(), "all cashflows must refer to the same quantity");

        QuantLib::Date expiryDate = expiryDates[i];

        QuantLib::Date paymentDate = longFlow->date();

        QL_REQUIRE(paymentDate == shortFlow->date(),
                   "all cashflows must refer to the same paymentDate, its used as the settlementDate of the option");

        paymentDateAdjuster->updatePaymentDate(expiryDate, paymentDate);

        QL_REQUIRE(paymentDate >= expiryDate, "Payment date must be greater than or equal to expiry date.");

        QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

        // maturity gets overwritten every time, and it is ok. If the last option is settled with delay, maturity is set
        // to the settlement date.
        maturity_ = maturity_ == Date() ? paymentDate : std::max(maturity_, paymentDate);

        // build the instrument for the i-th cfs
        QuantLib::ext::shared_ptr<QuantExt::CommoditySpreadOption> spreadOption =
            QuantLib::ext::make_shared<QuantExt::CommoditySpreadOption>(longFlow, shortFlow, exercise, quantity, strike_,
                                                                optionType, paymentDate, fxIndexes[1 - payerLegId],
                                                                fxIndexes[1 - payerLegId]);

        // build and assign the engine
        QuantLib::ext::shared_ptr<PricingEngine> commoditySpreadOptionEngine =
            engineBuilder->engine(ccy, longFlow->index(), shortFlow->index(), id());
        spreadOption->setPricingEngine(commoditySpreadOptionEngine);
        setSensitivityTemplate(*engineBuilder);
        if (i > 0) {
            additionalInstruments.push_back(spreadOption);
            additionalMultipliers.push_back(bsInd);
        } else {
            firstInstrument = spreadOption;
            firstMultiplier = bsInd;
        }
    }

    // Add premium
    auto configuration = engineBuilder->configuration(MarketContext::pricing);
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, firstMultiplier,
                                                optionData_.premiumData(), -bsInd, ccy, engineFactory, configuration));

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(firstInstrument, firstMultiplier, additionalInstruments,
                                                        additionalMultipliers);
    if (!optionData_.premiumData().premiumData().empty()) {
        auto premium = optionData_.premiumData().premiumData().front();
        additionalData_["premiumAmount"] = -bsInd * premium.amount;
        additionalData_["premiumPaymentDate"] = premium.payDate;
        additionalData_["premiumCurrency"] = premium.ccy;
    }
}

std::map<ore::data::AssetClass, std::set<std::string>>
CommoditySpreadOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<ore::data::AssetClass, std::set<std::string>> result;
    auto legData = csoData_.legData();
    for (const auto& leg : legData) {
        set<string> indices = leg.indices();
        for (auto ind : indices) {
            QuantLib::ext::shared_ptr<Index> index = parseIndex(ind);
            // only handle commodity
            if (auto ci = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                result[ore::data::AssetClass::COM].insert(ci->name());
            }
        }
    }
    return result;
}


void CommoditySpreadOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* csoNode = XMLUtils::getChildNode(node, "CommoditySpreadOptionData");
    csoData_.fromXML(csoNode);
}
XMLNode* CommoditySpreadOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    auto csoNode = csoData_.toXML(doc);
    XMLUtils::appendNode(node, csoNode);
    return node;
}

void CommoditySpreadOptionData::fromXML(XMLNode* csoNode) {
    XMLUtils::checkNode(csoNode, "CommoditySpreadOptionData");

    XMLNode* optionDataNode = XMLUtils::getChildNode(csoNode, "OptionData");
    QL_REQUIRE(optionDataNode, "Invalid CommmoditySpreadOption trade xml: found no OptionData Node");

    optionData_.fromXML(optionDataNode);
    strike_ = XMLUtils::getChildValueAsDouble(csoNode, "SpreadStrike", true);

    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(csoNode, "LegData");
    QL_REQUIRE(nodes.size() == 2, "CommoditySpreadOption: Exactly two LegData nodes expected");
    for (auto& node : nodes) {
        auto ld = createLegData();
        ld->fromXML(node);
        legData_.push_back(*ld);
    }

    XMLNode* optionStripNode = XMLUtils::getChildNode(csoNode, "OptionStripPaymentDates");
    if (optionStripNode) {
        optionStrip_ = OptionStripData();
        optionStrip_->fromXML(optionStripNode);
    }

    QL_REQUIRE(legData_[0].isPayer() != legData_[1].isPayer(),
               "CommoditySpreadOption: both a long and a short Assets are required.");
}

XMLNode* CommoditySpreadOptionData::toXML(XMLDocument& doc) const {
    XMLNode* csoNode = doc.allocNode("CommoditySpreadOptionData");
    for (size_t i = 0; i < legData_.size(); ++i) {
        XMLUtils::appendNode(csoNode, legData_[i].toXML(doc));
    }
    XMLUtils::appendNode(csoNode, optionData_.toXML(doc));
    XMLUtils::addChild(doc, csoNode, "SpreadStrike", strike_);
    if (optionStrip_.has_value()) {
        XMLUtils::appendNode(csoNode, optionStrip_->toXML(doc));
    }
    return csoNode;
}


void CommoditySpreadOptionData::OptionStripData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "OptionStripPaymentDates");
    XMLNode* optionStripScheduleNode = XMLUtils::getChildNode(node, "OptionStripDefinition");
    QL_REQUIRE(optionStripScheduleNode, "Schedule required to define the option strips");
    schedule_.fromXML(optionStripScheduleNode);
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "PaymentCalendar", false, "NullCalendar"));
    lag_ = parseInteger(XMLUtils::getChildValue(node, "PaymentLag", false, "0"));
    bdc_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "PaymentConvention", false, "MF"));
}

XMLNode* CommoditySpreadOptionData::OptionStripData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("OptionStripPaymentDates");
    auto tmp = schedule_.toXML(doc);
    XMLUtils::setNodeName(doc, tmp, "OptionStripDefinition");
    XMLUtils::appendNode(node, tmp);
    XMLUtils::addChild(doc, node, "PaymentCalendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "PaymentLag", to_string(lag_));
    XMLUtils::addChild(doc, node, "PaymentConvention", to_string(bdc_));
    return node;
}

} // namespace ore::data
