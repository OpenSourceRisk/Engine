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

#include <ored/portfolio/trs.hpp>
#include <ored/portfolio/trsunderlyingbuilder.hpp>
#include <ored/portfolio/trswrapper.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/compositeindex.hpp>

#include <ored/utilities/indexnametranslator.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/bimap.hpp>
#include <boost/optional.hpp>
#include <boost/range/adaptor/map.hpp>

namespace ore {
namespace data {

void addTRSRequiredFixings(RequiredFixings& fixings, const std::vector<Leg>& returnLegs, 
    const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& ind = nullptr) {
    QL_REQUIRE(returnLegs.size() > 0, "TrsUnderlyingBuilder: No returnLeg built");
    auto fdg = QuantLib::ext::make_shared<FixingDateGetter>(fixings);
    fdg->setAdditionalFxIndex(ind);
    for (const auto& rl : returnLegs)
        addToRequiredFixings(rl, fdg);
}

void TRS::ReturnData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReturnData");
    payer_ = XMLUtils::getChildValueAsBool(node, "Payer", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    scheduleData_.fromXML(XMLUtils::getChildNode(node, "ScheduleData"));
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", false);
    observationConvention_ = XMLUtils::getChildValue(node, "ObservationConvention", false);
    observationCalendar_ = XMLUtils::getChildValue(node, "ObservationCalendar", false);
    paymentLag_ = XMLUtils::getChildValue(node, "PaymentLag", false);
    paymentConvention_ = XMLUtils::getChildValue(node, "PaymentConvention", false);
    paymentCalendar_ = XMLUtils::getChildValue(node, "PaymentCalendar", false);
    paymentDates_ = XMLUtils::getChildrenValues(node, "PaymentDates", "PaymentDate", false);
    initialPrice_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "InitialPrice")) {
        initialPrice_ = parseReal(XMLUtils::getNodeValue(n));
    }
    initialPriceCurrency_ = XMLUtils::getChildValue(node, "InitialPriceCurrency");
    payUnderlyingCashFlowsImmediately_ = boost::none;
    if (auto n = XMLUtils::getChildNode(node, "PayUnderlyingCashFlowsImmediately")) {
        payUnderlyingCashFlowsImmediately_ = parseBool(XMLUtils::getNodeValue(n));
    }
    fxTerms_ = XMLUtils::getChildrenValues(node, "FXTerms", "FXIndex", false);
}

XMLNode* TRS::ReturnData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("ReturnData");
    XMLUtils::addChild(doc, n, "Payer", payer_);
    XMLUtils::addChild(doc, n, "Currency", currency_);
    XMLUtils::appendNode(n, scheduleData_.toXML(doc));
    if (!observationLag_.empty())
        XMLUtils::addChild(doc, n, "ObservationLag", observationLag_);
    if (!observationConvention_.empty())
        XMLUtils::addChild(doc, n, "ObservationConvention", observationConvention_);
    if (!observationCalendar_.empty())
        XMLUtils::addChild(doc, n, "ObservationCalendar", observationCalendar_);
    if (!paymentLag_.empty())
        XMLUtils::addChild(doc, n, "PaymentLag", paymentLag_);
    if (!paymentConvention_.empty())
        XMLUtils::addChild(doc, n, "PaymentConvention", paymentConvention_);
    if (!paymentCalendar_.empty())
        XMLUtils::addChild(doc, n, "PaymentCalendar", paymentCalendar_);
    if (!paymentDates_.empty())
        XMLUtils::addChildren(doc, n, "PaymentDates", "PaymentDate", paymentDates_);
    if (initialPrice_ != Null<Real>())
        XMLUtils::addChild(doc, n, "InitialPrice", initialPrice_);
    if (!initialPriceCurrency_.empty())
        XMLUtils::addChild(doc, n, "InitialPriceCurrency", initialPriceCurrency_);
    if (payUnderlyingCashFlowsImmediately_)
        XMLUtils::addChild(doc, n, "PayUnderlyingCashFlowsImmediately", *payUnderlyingCashFlowsImmediately_);
    if (!fxTerms_.empty())
        XMLUtils::addChildren(doc, n, "FXTerms", "FXIndex", fxTerms_);
    return n;
}

void TRS::FundingData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "FundingData");
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(node, "LegData");
    for (auto const n : nodes) {
        LegData ld;
        ld.fromXML(n);
        legData_.push_back(ld);
    }
    vector<XMLNode*> nodes2 = XMLUtils::getChildrenNodes(node, "NotionalType");
    for (auto const n : nodes2) {
        notionalType_.push_back(parseTrsFundingNotionalType(XMLUtils::getNodeValue(n)));
    }
    fundingResetGracePeriod_ = XMLUtils::getChildValueAsInt(node, "FundingResetGracePeriod", false, 0);
}

XMLNode* TRS::FundingData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("FundingData");
    for (auto& l : legData_) {
        XMLUtils::appendNode(n, l.toXML(doc));
    }
    for (auto const t : notionalType_) {
        XMLUtils::addChild(doc, n, "NotionalType", ore::data::to_string(t));
    }
    if(fundingResetGracePeriod_ > 0) {
        XMLUtils::addChild(doc, n, "FundingResetGracePeriod", std::to_string(fundingResetGracePeriod_));
    }
    return n;
}

void TRS::AdditionalCashflowData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AdditionalCashflowData");
    XMLNode* tmp = XMLUtils::getChildNode(node, "LegData");
    if (tmp)
        legData_.fromXML(tmp);
    else
        legData_ = LegData();
}

XMLNode* TRS::AdditionalCashflowData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("AdditionalCashflowData");
    if (legData_.concreteLegData())
        XMLUtils::appendNode(n, legData_.toXML(doc));
    return n;
}

std::map<AssetClass, std::set<std::string>>
TRS::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    for (Size i = 0; i < underlying_.size(); ++i) {
        QL_REQUIRE(underlying_[i], "TRS::underlyingIndices(): underlying trade is null");
        // a builder might update the underlying (e.g. promote it from bond to convertible bond)
        if (underlyingDerivativeId_[i].empty()) {
            for (auto const& b : TrsUnderlyingBuilderFactory::instance().getBuilders()) {
                b.second->updateUnderlying(referenceDataManager, underlying_[i], id());
            }
        }
        for (auto const& tmp : underlying_[i]->underlyingIndices(referenceDataManager))
            result[tmp.first].insert(tmp.second.begin(), tmp.second.end());
    }
    return result;
}

void TRS::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    // set id early since we use it below to set the underlying trade's id
    this->id() = XMLUtils::getAttribute(node, "id");

    // trs data node
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType_ + "Data");
    QL_REQUIRE(dataNode, tradeType_ + "Data node required");

    // read underlying data
    XMLNode* underlyingDataNode = XMLUtils::getChildNode(dataNode, "UnderlyingData");
    QL_REQUIRE(underlyingDataNode, "UnderlyingData node required");
    std::vector<XMLNode*> underlyingTradeNodes = XMLUtils::getChildrenNodes(underlyingDataNode, "Trade");
    std::vector<XMLNode*> underlyingTradeNodes2 = XMLUtils::getChildrenNodes(underlyingDataNode, "Derivative");
    QL_REQUIRE(!underlyingTradeNodes.empty() || !underlyingTradeNodes2.empty(),
               "at least one 'Trade' or 'Derivative' node required");
    Size underlyingCounter = 0;
    underlying_.clear();
    underlyingDerivativeId_.clear();
    for (auto const n : underlyingTradeNodes) {
        std::string tradeType = XMLUtils::getChildValue(n, "TradeType", true);
        QuantLib::ext::shared_ptr<Trade> u;
        try {
            u = TradeFactory::instance().build(tradeType);
        } catch (const std::exception& e) {
            QL_FAIL("Failed for build TRS underlying trade # " << underlyingCounter + 1 << ": " << e.what());
        }
        u->id() = this->id() + "_underlying" +
                  (underlyingTradeNodes.size() > 1 ? "_" + std::to_string(underlyingCounter++) : "");
        u->fromXML(n);
        underlyingDerivativeId_.push_back(std::string());
        underlying_.push_back(u);
    }
    for (auto const n : underlyingTradeNodes2) {
        underlyingDerivativeId_.push_back(XMLUtils::getChildValue(n, "Id", true));
        auto t = XMLUtils::getChildNode(n, "Trade");
        QL_REQUIRE(t != nullptr, "expected 'Trade' node under 'Derivative' node");
        std::string tradeType = XMLUtils::getChildValue(t, "TradeType", true);
        auto u = TradeFactory::instance().build(tradeType);
        QL_REQUIRE(u, "No trade builder found for TRS derivative trade type '"
                          << tradeType << "' when processing underlying trade #" << (underlyingCounter + 1));
        u->id() = this->id() + "_underlying" +
                  (underlyingTradeNodes.size() > 1 ? "_" + std::to_string(underlyingCounter++) : "");
        u->fromXML(t);
        underlying_.push_back(u);
    }

    // read return data
    XMLNode* returnDataNode = XMLUtils::getChildNode(dataNode, "ReturnData");
    returnData_.fromXML(returnDataNode);

    // read funding data
    XMLNode* fundingDataNode = XMLUtils::getChildNode(dataNode, "FundingData");
    if (fundingDataNode)
        fundingData_.fromXML(fundingDataNode);
    else
        fundingData_ = FundingData();

    // read additional cashflow data
    XMLNode* additionalCashflowDataNode = XMLUtils::getChildNode(dataNode, "AdditionalCashflowData");
    if (additionalCashflowDataNode)
        additionalCashflowData_.fromXML(additionalCashflowDataNode);
    else
        additionalCashflowData_ = AdditionalCashflowData();
}

XMLNode* TRS::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType_ + "Data");
    XMLUtils::appendNode(node, dataNode);

    XMLNode* underlyingDataNode = doc.allocNode("UnderlyingData");
    XMLUtils::appendNode(dataNode, underlyingDataNode);

    for (Size i = 0; i < underlying_.size(); ++i) {
        if (underlyingDerivativeId_[i].empty()) {
            XMLUtils::appendNode(underlyingDataNode, underlying_[i]->toXML(doc));
        } else {
            auto d = XMLUtils::addChild(doc, underlyingDataNode, "Derivative");
            XMLUtils::addChild(doc, d, "Id", underlyingDerivativeId_[i]);
            XMLUtils::appendNode(d, underlying_[i]->toXML(doc));
        }
    }

    XMLUtils::appendNode(dataNode, returnData_.toXML(doc));
    if (!fundingData_.legData().empty())
        XMLUtils::appendNode(dataNode, fundingData_.toXML(doc));
    if (additionalCashflowData_.legData().concreteLegData())
        XMLUtils::appendNode(dataNode, additionalCashflowData_.toXML(doc));

    return node;
}

QuantLib::ext::shared_ptr<QuantExt::FxIndex>
TRS::getFxIndex(const QuantLib::ext::shared_ptr<Market> market, const std::string& configuration, const std::string& domestic,
                const std::string& foreign, std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices,
                std::set<std::string>& missingFxIndexPairs) const {
    if (domestic == foreign)
        return nullptr;
    std::set<std::string> requiredCcys = {domestic, foreign};
    for (auto const& f : returnData_.fxTerms()) {
        auto fx = parseFxIndex(f);
        std::set<std::string> indexCcys = {fx->sourceCurrency().code(), fx->targetCurrency().code()};
        if (requiredCcys == indexCcys) {
            auto h = fxIndices.find(f);
            if (h != fxIndices.end())
                return h->second;
            DLOG("setting up fx index for domestic=" << domestic << " foreign=" << foreign);
            auto fx = buildFxIndex(f, domestic, foreign, market, configuration, false);
            fxIndices[f] = fx;
            return fx;
        }
    }

    // build a fx index, so that the processing can continue, but add to the error messages,
    // which - if not empty - will fail the trade build eventually

    std::string f("FX-GENERIC-" + domestic + "-" + foreign);
    auto fx = buildFxIndex(f, domestic, foreign, market, configuration, false);
    fxIndices[f] = fx;
    missingFxIndexPairs.insert(domestic + foreign);
    return fx;
}

void TRS::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("TRS::build() called for id = " << id());

    // clear trade members

    reset();

    creditRiskCurrency_.clear();
    creditQualifierMapping_.clear();
    notionalCurrency_ = returnData_.currency();

    // checks

    std::set<bool> fundingLegPayers;
    std::set<std::string> fundingCurrencies;

    bool fundingLegPayer = !(returnData_.payer());
    std::string fundingCurrency = returnData_.currency();

    for (auto const& l : fundingData_.legData()) {
        fundingLegPayer = l.isPayer();
        fundingCurrency = l.currency();
        fundingLegPayers.insert(fundingLegPayer);
        fundingCurrencies.insert(fundingCurrency);
    }

    QL_REQUIRE(fundingLegPayers.size() <= 1, "funding leg payer flags must match");
    QL_REQUIRE(fundingCurrencies.size() <= 1, "funding leg currencies must match");

    // a builder might update the underlying (e.g. promote it from bond to convertible bond)

    for (Size i = 0; i < underlying_.size(); ++i) {
        if (underlyingDerivativeId_[i].empty()) {
            for (auto const& b : TrsUnderlyingBuilderFactory::instance().getBuilders()) {
                b.second->updateUnderlying(engineFactory->referenceData(), underlying_[i], id());
            }
        }
    }

    // build underlying trade, add required fixing from there to this trade

    for (Size i = 0; i < underlying_.size(); ++i) {
        DLOG("build underlying trade #" << (i + 1) << " of type '" << underlying_[i]->tradeType() << "'");
        underlying_[i]->reset();
        underlying_[i]->build(engineFactory);
        requiredFixings_.addData(underlying_[i]->requiredFixings());
        // populate sensi template from first underlying, we have to make _some_ assumption here!
        if (sensitivityTemplate_.empty()) {
            setSensitivityTemplate(underlying_[i]->sensitivityTemplate());
        }
    }

    // propagate additional data from underlyings to trs trade
    for (Size i = 0; i < underlying_.size(); ++i) {
        for (auto const& [key, value] : underlying_[i]->additionalData()) {
            additionalData_["und_ad_" + std::to_string(i + 1) + "_" + key] = value;
        }
    }

    // we use dirty prices, so we need accrued amounts in the past
    requiredFixings_.unsetPayDates();

    // build return leg valuation and payment date vectors

    DLOG("build valuation and payment dates vectors");

    std::vector<Date> valuationDates, paymentDates;

    QuantLib::Schedule schedule = makeSchedule(returnData_.scheduleData());
    QL_REQUIRE(schedule.dates().size() >= 2, "at least two dates required in return schedule");

    Calendar observationCalendar = parseCalendar(returnData_.observationCalendar());
    BusinessDayConvention observationConvention = returnData_.observationConvention().empty()
                                                      ? Unadjusted
                                                      : parseBusinessDayConvention(returnData_.observationConvention());
    Period observationLag = returnData_.observationLag().empty() ? 0 * Days : parsePeriod(returnData_.observationLag());

    Calendar paymentCalendar = parseCalendar(returnData_.paymentCalendar());
    BusinessDayConvention paymentConvention = returnData_.paymentConvention().empty()
                                                  ? Unadjusted
                                                  : parseBusinessDayConvention(returnData_.paymentConvention());
    PaymentLag paymentLag = parsePaymentLag(returnData_.paymentLag());
    Period plPeriod = boost::apply_visitor(PaymentLagPeriod(), paymentLag);

    for (auto const& d : schedule.dates()) {
        valuationDates.push_back(observationCalendar.advance(d, -observationLag, observationConvention));
        if (d != schedule.dates().front())
            paymentDates.push_back(paymentCalendar.advance(d, plPeriod, paymentConvention));
    }

    if (!returnData_.paymentDates().empty()) {
        paymentDates.clear();
        QL_REQUIRE(returnData_.paymentDates().size() + 1 == valuationDates.size(),
                   "paymentDates size (" << returnData_.paymentDates().size() << ") does no match valuatioDates size ("
                                         << valuationDates.size() << ") minus 1");
        for (auto const& s : returnData_.paymentDates())
            paymentDates.push_back(parseDate(s));
    }

    DLOG("valuation schedule:");
    for (auto const& d : valuationDates)
        DLOG(ore::data::to_string(d));

    DLOG("payment schedule:");
    for (auto const& d : paymentDates)
        DLOG(ore::data::to_string(d));

    // build indices corresponding to underlying trades and populate necessary data

    std::map<std::string, double> indexNamesAndQty;
    std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> initialFxIndices, fxIndices, fxIndicesDummy;

    // get fx indices for conversion return and add cf ccy to funding ccy

    std::set<std::string> missingFxIndexPairs;

    auto fxIndexReturn = getFxIndex(engineFactory->market(), engineFactory->configuration(MarketContext::pricing),
                                    returnData_.currency(), fundingCurrency, initialFxIndices, missingFxIndexPairs);
    auto fxIndexAdditionalCashflows =
        additionalCashflowData_.legData().currency().empty()
            ? fxIndexReturn
            : getFxIndex(engineFactory->market(), engineFactory->configuration(MarketContext::pricing),
                         additionalCashflowData_.legData().currency(), fundingCurrency, fxIndicesDummy,
                         missingFxIndexPairs);

    Real initialPrice = returnData_.initialPrice();

    std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>> underlyingIndex(underlying_.size(), nullptr);
    std::vector<Real> underlyingMultiplier(underlying_.size(), Null<Real>());
    std::vector<std::string> assetCurrency(underlying_.size(), fundingCurrency);
    std::vector<QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexAsset(underlying_.size(), nullptr);

    maturity_ = Date::minDate();

    for (Size i = 0; i < underlying_.size(); ++i) {

        DLOG("build underlying index for underlying #" << (i + 1));

        std::string localCreditRiskCurrency;
        std::map<std::string, double> localIndexNamesAndQuantities;
        std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> localFxIndices = initialFxIndices;
        Real dummyInitialPrice = 1.0; // initial price is only updated if we have one underlying
        
        std::vector<Leg> returnLegs;
        auto builder = TrsUnderlyingBuilderFactory::instance().getBuilder(
            underlyingDerivativeId_[i].empty() ? underlying_[i]->tradeType() : "Derivative");
        builder->build(id(), underlying_[i], valuationDates, paymentDates, fundingCurrency, engineFactory,
                       underlyingIndex[i], underlyingMultiplier[i], localIndexNamesAndQuantities, localFxIndices,
                       underlying_.size() == 1 ? initialPrice : dummyInitialPrice, assetCurrency[i],
                       localCreditRiskCurrency, creditQualifierMapping_,
                       std::bind(&TRS::getFxIndex, this, std::placeholders::_1, std::placeholders::_2,
                                 std::placeholders::_3, std::placeholders::_4, std::placeholders::_5,
                                 std::ref(missingFxIndexPairs)),
                       underlyingDerivativeId_[i], requiredFixings_, returnLegs);

        addTRSRequiredFixings(requiredFixings_, returnLegs, fxIndexReturn);

        // update global credit risk currency

        if (creditRiskCurrency_.empty()) {
            creditRiskCurrency_ = localCreditRiskCurrency;
        } else if (!localCreditRiskCurrency.empty() && creditRiskCurrency_ != localCreditRiskCurrency) {
            ore::data::StructuredTradeErrorMessage(id(), tradeType(), "Ambiguous SIMM CreditQ currencies for TRS",
                                                        "Will use '" + creditRiskCurrency_ + "', found '" +
                                                       localCreditRiskCurrency + "' in addition.")
                .log();
        }

        // get fx indices for conversion of asset to funding ccy

        DLOG("underlying #" << (i + 1) << " has asset ccy " << assetCurrency[i] << ", funding ccy is "
                            << fundingCurrency << ", return ccy is " << returnData_.currency());

        fxIndexAsset[i] = getFxIndex(engineFactory->market(), engineFactory->configuration(MarketContext::pricing),
                                     assetCurrency[i], fundingCurrency, localFxIndices, missingFxIndexPairs);
        DLOG("underlying #" << (i + 1) << " index (" << underlyingIndex[i]->name() << ") built.");
        DLOG("underlying #" << (i + 1) << " multiplier is " << underlyingMultiplier[i]);      

        // update global indexNames and  fxIndices

        for (const auto& [indexName, qty] : localIndexNamesAndQuantities) {
            indexNamesAndQty[indexName] += (returnData_.payer() ? -1.0 : 1.0) * qty;
        }

        fxIndices.insert(localFxIndices.begin(), localFxIndices.end());
    }

    // ISDA taxonomy
    bool assetClassIsUnique = true;
    std::string assetClass;
    for (auto u : underlying_) {
        auto it = u->additionalData().find("isdaAssetClass");
        if (it != u->additionalData().end()) {
            std::string ac = boost::any_cast<std::string>(it->second);
            if (assetClass == "")
                assetClass = ac;
            else if (ac != assetClass)
                assetClassIsUnique = false;
        }
    }

    additionalData_["isdaAssetClass"] = string("");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    if (assetClass == "") {
        ALOG("ISDA asset class not found for TRS " << id() << ", ISDA taxonomy undefined");
    } else {
        additionalData_["isdaAssetClass"] = assetClass;
        if (!assetClassIsUnique) {
            WLOG("ISDA asset class not unique in TRS " << id() << " using first hit: " << assetClass);
        }
        additionalData_["isdaBaseProduct"] = string("Total Return Swap");
        if (assetClass == "Equity") {
            if (tradeType_ == "ContractForDifference")
                additionalData_["isdaBaseProduct"] = string("Contract For Difference");
            else
                additionalData_["isdaBaseProduct"] = string("Swap");
            additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
        } else if (assetClass == "Credit") {
            additionalData_["isdaBaseProduct"] = string("Total Return Swap");
            additionalData_["isdaSubProduct"] = string("");
        } else {
            WLOG("ISDA asset class " << assetClass << " not explicitly covered for TRS trade " << id()
                                     << " using default BaseProduct 'Total Retuen Swap' and leaving sub-product blank");
        }
    }

    // check that we have all fx terms that we needed to build the fx indices

    QL_REQUIRE(missingFxIndexPairs.empty(), "TRS::build(): missing FXTerms for the following pairs: "
                                                << boost::algorithm::join(missingFxIndexPairs, ", "));

    // set initial price currency

    QL_REQUIRE(!assetCurrency.empty(), "TRS::build(): no underlying given.");

    std::string initialPriceCurrency =
        returnData_.initialPriceCurrency().empty() ? assetCurrency.front() : returnData_.initialPriceCurrency();

    if (initialPrice != Null<Real>() && returnData_.initialPriceCurrency().empty()) {
        for (auto const& ccy : assetCurrency) {
            QL_REQUIRE(ccy == initialPriceCurrency, "TRS::build(): can not determine unique initial price currency "
                                                    "from asset currencies for initial price ("
                                                        << returnData_.initialPrice()
                                                        << "), please add the initial price currency to the trade xml");
        }
    }

    // log some results from the build, convert initial price to major ccy if necessary

    if (initialPrice != Null<Real>()) {
        DLOG("initial price is given as " << initialPrice << " " << initialPriceCurrency);
	initialPrice = convertMinorToMajorCurrency(initialPriceCurrency, initialPrice);
	DLOG("initial price after conversion to major ccy " << initialPrice);
    } else {
        DLOG("no initial price is given");
    }

    DLOG("fundingCurrency is " << fundingCurrency);
    DLOG("creditRiskCurrency is " << creditRiskCurrency_);
    for (Size i = 0; i < assetCurrency.size(); ++i)
        DLOG("assetCurrency #" << i << " is " << assetCurrency[i]);

    // build funding legs, so far the supported types are
    // Fixed, Floating

    QL_REQUIRE(
        fundingData_.notionalType().empty() || fundingData_.notionalType().size() == fundingData_.legData().size(),
        "TRS::build(): got " << fundingData_.notionalType().size() << " NotionalType tags in FundingData, but "
                             << fundingData_.legData().size()
                             << " LegData nodes. These two must match. The NotionalType can also be omitted entirely.");

    std::vector<Leg> fundingLegs;
    std::vector<TRS::FundingData::NotionalType> fundingNotionalTypes;
    for (Size i = 0; i < fundingData_.legData().size(); ++i) {

        auto& ld = fundingData_.legData()[i];
        QL_REQUIRE(ld.legType() == "Fixed" || ld.legType() == "Floating" || ld.legType() == "CMS" ||
                       ld.legType() == "CMB",
                   "TRS::build(): funding leg type: only fixed, floating, CMS, CMB are supported");
        TRS::FundingData::NotionalType notionalType =
            fundingData_.notionalType().empty() ? (ld.notionals().empty() ? TRS::FundingData::NotionalType::PeriodReset
                                                                          : TRS::FundingData::NotionalType::Fixed)
                                                : fundingData_.notionalType()[i];
        QL_REQUIRE(ld.notionals().empty() || notionalType == TRS::FundingData::NotionalType::Fixed,
                   "TRS::build(): if notional is given in funding leg data, the notional type must be fixed, got "
                       << notionalType << " for funding leg #" << (i + 1));

        struct TempNotionalSetter {
            TempNotionalSetter(LegData& ld) : ld_(ld) {
                if (ld.notionals().empty()) {
                    ld.notionals() = std::vector<Real>(1, 1.0);
                    ld.notionalDates().clear();
                    restoreEmptyLegDataNotionals_ = true;
                } else {
                    restoreEmptyLegDataNotionals_ = false;
                }
            }
            ~TempNotionalSetter() {
                if (restoreEmptyLegDataNotionals_)
                    ld_.notionals().clear();
            }
            LegData& ld_;
            bool restoreEmptyLegDataNotionals_;
        };

        TempNotionalSetter tmpSetter(ld);

        auto legBuilder = engineFactory->legBuilder(ld.legType());
        fundingLegs.push_back(legBuilder->buildLeg(ld, engineFactory, requiredFixings_,
                                                   engineFactory->configuration(MarketContext::pricing)));
        fundingNotionalTypes.push_back(notionalType);

        // update credit risk currency and credit qualifier mapping for CMB leg
        if (ld.legType() == "CMB") {
            auto cmbData = QuantLib::ext::dynamic_pointer_cast<ore::data::CMBLegData>(ld.concreteLegData());
            QL_REQUIRE(cmbData, "TRS::build(): internal error, could to cast to CMBLegData.");
            if(creditRiskCurrency_.empty())
                creditRiskCurrency_ = getCmbLegCreditRiskCurrency(*cmbData, engineFactory->referenceData());
            auto [source, target] =
                getCmbLegCreditQualifierMapping(*cmbData, engineFactory->referenceData(), id(), tradeType());
            creditQualifierMapping_[source] = target;
            creditQualifierMapping_[ore::data::creditCurveNameFromSecuritySpecificCreditCurveName(source)] = target;
        }
    }

    // add required fixings for funding legs with daily resets

    DLOG("add required fixings for fundings legs with daily resets (if any)");

    for (Size i = 0; i < fundingLegs.size(); ++i) {
        if (fundingNotionalTypes[i] == TRS::FundingData::NotionalType::DailyReset) {
            for (auto const& c : fundingLegs[i]) {
                if (auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(c)) {
                    QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<QuantLib::FixedRateCoupon>(c) ||
                                   QuantLib::ext::dynamic_pointer_cast<QuantLib::IborCoupon>(c) ||
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c) ||
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c),
                               "daily reset funding legs support fixed rate, ibor and overnight indexed coupons only");
                    for (QuantLib::Date d = cpn->accrualStartDate(); d < cpn->accrualEndDate(); ++d) {
                        for (Size j = 0; j < underlying_.size(); ++j) {
                            Date fixingDate = underlyingIndex[j]->fixingCalendar().adjust(d, Preceding);
                            for (auto const& [n, _] : indexNamesAndQty)
                                requiredFixings_.addFixingDate(fixingDate, n, cpn->date(), false, false);
                            for (auto const& n : fxIndices) {
                                requiredFixings_.addFixingDate(n.second->fixingCalendar().adjust(fixingDate, Preceding),
                                                               n.first, cpn->date(), false, false);
                            }
                        }
                    }
                }
            }
        }  else if (fundingNotionalTypes[i] == TRS::FundingData::NotionalType::PeriodReset) {
            for (auto const& c : fundingLegs[i]) {
                if (auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(c)) {
                    for (Size j = 0; j < underlying_.size(); ++j) {
                        Date fundingStartDate = cpn->accrualStartDate();
                        Size currentIdx = std::distance(valuationDates.begin(),
                                            std::upper_bound(valuationDates.begin(),
                                                             valuationDates.end(),
                                                             fundingStartDate + fundingData_.fundingResetGracePeriod()));
                        if (currentIdx > 0)
                            --currentIdx;
                        Date fixingDate = valuationDates[currentIdx];
                        for (auto const& [n, _] : indexNamesAndQty)
                            requiredFixings_.addFixingDate(fixingDate, n, cpn->date(), false, false);
                        for (auto const& n : fxIndices) {
                            requiredFixings_.addFixingDate(n.second->fixingCalendar().adjust(fixingDate, Preceding),
                                                           n.first, cpn->date(), false, false);
                        }
                    }
                }
            }
        }
    }

    // set start date

    Date startDate = Date::maxDate();
    for (auto const& l : fundingLegs) {
        for (auto const& cf : l) {
            QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(cf);
            if (coupon)
                startDate = std::min(startDate, coupon->accrualStartDate());
        }
    }

    additionalData_["startDate"] = to_string(startDate);
    for (const auto& [name, qty] : indexNamesAndQty) {
        additionalData_["underlying_quantity_" + name] = qty;
    }
    
    // build additional cashflow leg (if given)

    DLOG("build additional cashflow leg");

    Leg additionalCashflowLeg;
    bool additionalCashflowLegPayer = false;
    std::string additionalCashflowLegCurrency = fundingCurrency;
    if (additionalCashflowData_.legData().concreteLegData()) {
        QL_REQUIRE(additionalCashflowData_.legData().legType() == "Cashflow",
                   "TRS::build(): additional cashflow data leg must have type 'Cashflow'");
        additionalCashflowLeg = engineFactory->legBuilder(additionalCashflowData_.legData().legType())
                                    ->buildLeg(additionalCashflowData_.legData(), engineFactory, requiredFixings_,
                                               engineFactory->configuration(MarketContext::pricing));
        additionalCashflowLegPayer = additionalCashflowData_.legData().isPayer();
        additionalCashflowLegCurrency = additionalCashflowData_.legData().currency();
    }

    // parse asset currencies

    std::vector<QuantLib::Currency> parsedAssetCurrencies;
    for (auto const& c : assetCurrency) {
        parsedAssetCurrencies.push_back(parseCurrency(c));
    }

    // build instrument

    DLOG("build instrument and set trade member");

    bool includeUnderlyingCashflowsInReturn;
    if (returnData_.payUnderlyingCashFlowsImmediately()) {
        includeUnderlyingCashflowsInReturn = !(*returnData_.payUnderlyingCashFlowsImmediately());
    } else {
        includeUnderlyingCashflowsInReturn = tradeType_ != "ContractForDifference";
    }

    auto wrapper = QuantLib::ext::make_shared<TRSWrapper>(
        underlying_, underlyingIndex, underlyingMultiplier, includeUnderlyingCashflowsInReturn, initialPrice,
        parseCurrencyWithMinors(initialPriceCurrency), parsedAssetCurrencies, parseCurrency(returnData_.currency()),
        valuationDates, paymentDates, fundingLegs, fundingNotionalTypes, parseCurrency(fundingCurrency),
        fundingData_.fundingResetGracePeriod(), returnData_.payer(), fundingLegPayer, additionalCashflowLeg,
        additionalCashflowLegPayer, parseCurrency(additionalCashflowLegCurrency), fxIndexAsset, fxIndexReturn,
        fxIndexAdditionalCashflows, fxIndices);
    wrapper->setPricingEngine(QuantLib::ext::make_shared<TRSWrapperAccrualEngine>());
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(wrapper);

    // if the first valuation date is > today, we potentially need fixings for fx conversion as of "today"

    if (Date today = Settings::instance().evaluationDate(); !valuationDates.empty() && valuationDates.front() > today) {
        std::set<QuantLib::ext::shared_ptr<QuantExt::FxIndex>> tmp;
        auto fxIndicesVal = fxIndices | boost::adaptors::map_values;
        tmp.insert(fxIndicesVal.begin(), fxIndicesVal.end());
        tmp.insert(fxIndexAsset.begin(), fxIndexAsset.end());
        tmp.insert(fxIndexReturn);
        tmp.insert(fxIndexAdditionalCashflows);
        for (auto const& fx : tmp) {
            if (fx != nullptr) {
                requiredFixings_.addFixingDate(fx->fixingCalendar().adjust(today, Preceding),
                                               IndexNameTranslator::instance().oreName(fx->name()));
            }
        }
    }

    // set trade member variables (leave legs empty for the time being, we just have the funding leg really)

    npvCurrency_ = fundingCurrency;

    notional_ = 0.0; // we have overridden notional() to return this

    // if the maturity date was not set by the trs underlying builder, set it here
    if (maturity_ == Date::minDate()) {
        maturity_ = std::max(valuationDates.back(), paymentDates.back());
        for (auto const& l : fundingLegs) {
            maturity_ = std::max(maturity_, CashFlows::maturityDate(l));
        }
    }
}

QuantLib::Real TRS::notional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument()->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "currentNotional not provided"))
            ALOG("error when retrieving notional: " << e.what());
    }
    // if not provided, return null
    return Null<Real>();
}

// clang-format off
boost::bimap<std::string, TRS::FundingData::NotionalType>
    types = boost::assign::list_of<boost::bimap<std::string, TRS::FundingData::NotionalType>::relation>
    ("PeriodReset", TRS::FundingData::NotionalType::PeriodReset)
    ("DailyReset", TRS::FundingData::NotionalType::DailyReset)
    ("Fixed", TRS::FundingData::NotionalType::Fixed);
// clang-format on

TRS::FundingData::NotionalType parseTrsFundingNotionalType(const std::string& s) {
    auto it = types.left.find(s);
    QL_REQUIRE(it != types.left.end(),
               "parseTrsFundingNotionalType '" << s << "' failed, expected PeriodReset, DailyReset, Fixed");
    return it->second;
}

std::ostream& operator<<(std::ostream& os, const TRS::FundingData::NotionalType t) {
    auto it = types.right.find(t);
    QL_REQUIRE(it != types.right.end(), "operator<<(" << static_cast<int>(t) << ") failed, this is an internal error.");
    os << it->second;
    return os;
}

} // namespace data
} // namespace ore
