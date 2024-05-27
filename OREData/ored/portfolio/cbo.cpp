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

#include <ored/portfolio/builders/cbo.hpp>
#include <ored/portfolio/cbo.hpp>
#include <qle/instruments/bondbasket.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <qle/indexes/genericindex.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std::placeholders;
using namespace ore::data;

namespace ore {
namespace data {

void CBO::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("CBO::build() called for trade " << id());

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has CBO underlyings
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    requiredFixings_.clear();

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CBO");

    populateFromCboReferenceData(engineFactory->referenceData());
    validateCbo();

    Schedule schedule = makeSchedule(scheduleData_);

    bondbasket_ = bondbasketdata_.build(engineFactory, parseCurrency(ccy_), reinvestmentEndDate_);
    requiredFixings_.addData(bondbasketdata_.requiredFixings());

    vector<QuantExt::Tranche> tranches;
    size_t investCount = 0;
    size_t idx = 0;
    multiplier_ = 1.0;
    for(size_t i = 0; i < trancheData_.size(); i++){

        QuantExt::Tranche tranche;

        tranche.name = trancheData_[i]->name();;
        tranche.faceAmount = trancheData_[i]->faceAmount();
        tranche.icRatio = trancheData_[i]->icRatio();
        tranche.ocRatio = trancheData_[i]->ocRatio();

        LegData legdata = LegData();
        legdata.notionals() = {trancheData_[i]->faceAmount()};
        legdata.schedule() = scheduleData_;
        legdata.dayCounter() = daycounter_;
        legdata.concreteLegData() = trancheData_[i]->concreteLegData();
        legdata.paymentConvention() = paymentConvention_;

        auto legBuilder = engineFactory->legBuilder(trancheData_[i]->concreteLegData()->legType());
        RequiredFixings requiredFixingsLeg;
        auto configuration = builder->configuration(MarketContext::pricing);
        Leg leg = legBuilder->buildLeg(legdata, engineFactory, requiredFixingsLeg, configuration);
        tranche.leg = leg;

        requiredFixings_.addData(requiredFixingsLeg);
        tranches.push_back(tranche);

        //check if invested tranche
        if(tranche.name == investedTrancheName_){
            idx = i; //Set for leg output
            multiplier_ = investedNotional_ / tranche.faceAmount;
            if(multiplier_ > 1.0)
                ALOG("Ratio bigger than 1 : investment=" << investedNotional_ << " vs. faceAmount=" << tranche.faceAmount);
            investCount++;
        }
    }
    //check if tranche name could be found...
    QL_REQUIRE(investCount == 1, "Could not assign CBOInvestment TrancheName " << investedTrancheName_ << " to Names of CBOTranches.");

    // CHECK DATES BONDS vs TRANCHES
    Date longestBondDate = Settings::instance().evaluationDate();
    for (const auto& bond : bondbasket_->bonds()) {
        Date bondMat = bond.second->maturityDate();
        if (bondMat > longestBondDate)
            longestBondDate = bondMat;
    }
    QL_REQUIRE(schedule.endDate() > longestBondDate, " Tranche Maturity should be after Bond Maturity: Bond "
                                                          << longestBondDate << " vs. Tranche " << schedule.endDate());

    // set QuantExt instrument
    QuantLib::ext::shared_ptr<QuantExt::CBO> cbo =
        QuantLib::ext::make_shared<QuantExt::CBO>(bondbasket_, schedule, parseReal(seniorFee_), parseDayCounter(feeDayCounter_),
                                          tranches, parseReal(subordinatedFee_), parseReal(equityKicker_),
                                          parseCurrency(ccy_), investedTrancheName_);
    // set pricing engine...
    QuantLib::ext::shared_ptr<CboMCEngineBuilder> cboBuilder = QuantLib::ext::dynamic_pointer_cast<CboMCEngineBuilder>(builder);
    QL_REQUIRE(cboBuilder, "No Builder found for CBO: " << id());
    cbo->setPricingEngine(cboBuilder->engine(bondbasket_->pool()));
    setSensitivityTemplate(*cboBuilder);
    instrument_.reset(new VanillaInstrument(cbo, multiplier_));

    maturity_ = schedule.endDate();
    npvCurrency_ = ccy_;
    notional_ = investedNotional_;
    legs_ = vector<QuantLib::Leg>(1, tranches[idx].leg);
    legCurrencies_ = vector<string>(1, ccy_);
    legPayers_ = vector<bool>(1, false);

    // sensitivity...
    for (const auto& bond : bondBasketData().bonds()) {
        cbo->registerWith(securitySpecificCreditCurve(market, bond->bondData().securityId(),
                                                      bond->bondData().creditCurveId()));
        cbo->registerWith(bond->instrument()->qlInstrument());

        // to capture floater...
        Leg bondleg = bond->legs().front();
        for (auto const& c : bondleg)
            cbo->registerWith(c);
    }

    std::map<string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexMap = bondbasket_->fxIndexMap();
    std::map<string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>::iterator it;
    for (it = fxIndexMap.begin(); it != fxIndexMap.end(); ++it)
        cbo->registerWith(it->second);
}

void CBO::populateFromCboReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager){

    QL_REQUIRE(!structureId_.empty(), "CBO::populateFromCboReferenceDat(): no structure id given");
    if (!referenceDataManager || !referenceDataManager->hasData(CboReferenceDatum::TYPE, structureId_)) {
        DLOG("Could not get CboReferenceDatum for Id " << structureId_ << " leave data in trade unchanged");
     } else {
        auto cboRefData = QuantLib::ext::dynamic_pointer_cast<CboReferenceDatum>(
            referenceDataManager->getData(CboReferenceDatum::TYPE, structureId_));
        QL_REQUIRE(cboRefData, "could not cast to CboReferenceDatum, this is unexpected");
        populateFromCboReferenceData(cboRefData);
    }
}

void CBO::fromXML(XMLNode* node) {

    Trade::fromXML(node);
    XMLNode* cboData = XMLUtils::getChildNode(node, "CBOData");
    QL_REQUIRE(cboData, "expected node CBOData");

    //investment
    XMLNode* cboInvestment = XMLUtils::getChildNode(cboData, "CBOInvestment");
    QL_REQUIRE(cboInvestment, "expected node CBOInvestment");

    investedTrancheName_ = XMLUtils::getChildValue(cboInvestment, "TrancheName", true);
    investedNotional_ = XMLUtils::getChildValueAsDouble(cboInvestment, "Notional", true);
    structureId_ = XMLUtils::getChildValue(cboInvestment, "StructureId", true);

    //structure
    XMLNode* cboStructure = XMLUtils::getChildNode(cboData, "CBOStructure");
    //QL_REQUIRE(cboStructure, "expected node CBOStructure");
    if(cboStructure){
        daycounter_ = XMLUtils::getChildValue(cboStructure, "DayCounter", false);
        paymentConvention_ = XMLUtils::getChildValue(cboStructure, "PaymentConvention", false);
        ccy_ = XMLUtils::getChildValue(cboStructure, "Currency", false);
        seniorFee_ = XMLUtils::getChildValue(cboStructure, "SeniorFee", false);
        subordinatedFee_ = XMLUtils::getChildValue(cboStructure, "SubordinatedFee", false);
        equityKicker_ = XMLUtils::getChildValue(cboStructure, "EquityKicker", false);
        feeDayCounter_ = XMLUtils::getChildValue(cboStructure, "FeeDayCounter", false);
        reinvestmentEndDate_ = XMLUtils::getChildValue(cboStructure, "ReinvestmentEndDate", false, "");

        scheduleData_ = ScheduleData();
        XMLNode* scheduleData = XMLUtils::getChildNode(cboStructure, "ScheduleData");
        if (scheduleData)
            scheduleData_.fromXML(scheduleData);

        bondbasketdata_.clear();
        XMLNode* bondbasketNode = XMLUtils::getChildNode(cboStructure, "BondBasketData");
        if(bondbasketNode)
            bondbasketdata_.fromXML(bondbasketNode);

        trancheData_.clear();
        XMLNode* tranchesNode = XMLUtils::getChildNode(cboStructure, "CBOTranches");
        if(tranchesNode){
            for (XMLNode* child = XMLUtils::getChildNode(tranchesNode, "Tranche"); child; child = XMLUtils::getNextSibling(child)) {
                auto data = QuantLib::ext::make_shared<ore::data::TrancheData>();
                data->fromXML(child);
                trancheData_.push_back(data);
            }
        }
    }

}

XMLNode* CBO::toXML(ore::data::XMLDocument& doc) const {

    XMLNode* node = Trade::toXML(doc);
    XMLNode* cboData = doc.allocNode("CBOData");
    XMLUtils::appendNode(node, cboData);

    XMLNode* cboInvestment = doc.allocNode("CBOInvestment");
    XMLUtils::appendNode(cboData, cboInvestment);
    XMLUtils::addChild(doc, cboInvestment, "TrancheName", investedTrancheName_);
    XMLUtils::addChild(doc, cboInvestment, "Notional", investedNotional_);
    XMLUtils::addChild(doc, cboInvestment, "StructureId", structureId_);

    XMLNode* cboStructure = doc.allocNode("CBOStructure");
    XMLUtils::appendNode(cboData, cboStructure);
    XMLUtils::addChild(doc, cboStructure, "DayCounter", daycounter_);
    XMLUtils::addChild(doc, cboStructure, "PaymentConvention", paymentConvention_);
    XMLUtils::addChild(doc, cboStructure, "Currency", ccy_);
    XMLUtils::addChild(doc, cboStructure, "SeniorFee", seniorFee_);
    XMLUtils::addChild(doc, cboStructure, "SubordinatedFee", subordinatedFee_);
    XMLUtils::addChild(doc, cboStructure, "EquityKicker", equityKicker_);
    XMLUtils::addChild(doc, cboStructure, "FeeDayCounter", feeDayCounter_);
    XMLUtils::addChild(doc, cboStructure, "ReinvestmentEndDate", reinvestmentEndDate_);

    XMLUtils::appendNode(cboStructure, scheduleData_.toXML(doc));
    XMLUtils::appendNode(cboStructure, bondbasketdata_.toXML(doc));

    XMLNode* cboTranches = doc.allocNode("CBOTranches");
    XMLUtils::appendNode(cboStructure, cboTranches);
    for(size_t i = 0; i < trancheData_.size(); i++)
        XMLUtils::appendNode(cboTranches, trancheData_[i]->toXML(doc));

    return node;

}

std::map<AssetClass, std::set<std::string>>
CBO::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return bondbasketdata_.underlyingIndices(referenceDataManager);
}

void CBOTrsUnderlyingBuilder::build(
    const std::string& parentId, const QuantLib::ext::shared_ptr<Trade>& underlying, const std::vector<Date>& valuationDates,
    const std::vector<Date>& paymentDates, const std::string& fundingCurrency,
    const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, QuantLib::ext::shared_ptr<QuantLib::Index>& underlyingIndex,
    Real& underlyingMultiplier, std::map<std::string, double>& indexQuantities,
    std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices, Real& initialPrice,
    std::string& assetCurrency, std::string& creditRiskCurrency,
    std::map<std::string, SimmCreditQualifierMapping>& creditQualifierMapping, 
    const std::function<QuantLib::ext::shared_ptr<QuantExt::FxIndex>(
        const QuantLib::ext::shared_ptr<Market> market, const std::string& configuration, const std::string& domestic,
        const std::string& foreign, std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices)>&
        getFxIndex,
    const std::string& underlyingDerivativeId, RequiredFixings& fixings, std::vector<Leg>& returnLegs) const {
    auto t = QuantLib::ext::dynamic_pointer_cast<ore::data::CBO>(underlying);
    QL_REQUIRE(t, "could not cast to ore::data::CBO, this is unexpected");
    string indexName = "GENERIC-" + t->investedTrancheName();
    IndexNameTranslator::instance().add(indexName, indexName);
    indexQuantities[indexName] = t->underlyingMultiplier();
    underlyingIndex = QuantLib::ext::make_shared<QuantExt::GenericIndex>(indexName);
    underlyingMultiplier = t->underlyingMultiplier();
    assetCurrency = t->npvCurrency();
    creditRiskCurrency = t->npvCurrency();

    auto fxIndex = getFxIndex(engineFactory->market(), engineFactory->configuration(MarketContext::pricing),
                              assetCurrency, fundingCurrency, fxIndices);
    returnLegs.push_back(QuantExt::TRSLeg(valuationDates, paymentDates, underlyingMultiplier, underlyingIndex, fxIndex)
        .withInitialPrice(initialPrice));

    //fill the SimmCreditQualifierMapping
    auto bonds = t->bondBasketData().bonds();
    for (Size i = 0; i < bonds.size(); ++i) {
        creditQualifierMapping[ore::data::securitySpecificCreditCurveName(bonds[i]->bondData().securityId(),
                                                                          bonds[i]->bondData().creditCurveId())] =
            SimmCreditQualifierMapping(bonds[i]->bondData().securityId(), bonds[i]->bondData().creditGroup());
        creditQualifierMapping[bonds[i]->bondData().creditCurveId()] =
            SimmCreditQualifierMapping(bonds[i]->bondData().securityId(), bonds[i]->bondData().creditGroup());
    }
}


void CboReferenceDatum::CboStructure::fromXML(XMLNode* node) {

    QL_REQUIRE(node, "CboReferenceDatum::CboStructure::fromXML(): no node given");

    daycounter = XMLUtils::getChildValue(node, "DayCounter", true);
    paymentConvention = XMLUtils::getChildValue(node, "PaymentConvention", true);
    ccy = XMLUtils::getChildValue(node, "Currency", true);
    seniorFee  = XMLUtils::getChildValue(node, "SeniorFee", true);
    subordinatedFee = XMLUtils::getChildValue(node, "SubordinatedFee", true);
    equityKicker = XMLUtils::getChildValue(node, "EquityKicker", true);
    feeDayCounter = XMLUtils::getChildValue(node, "FeeDayCounter", true);
    reinvestmentEndDate = XMLUtils::getChildValue(node, "ReinvestmentEndDate", false, "");

    XMLNode* scheduleNode = XMLUtils::getChildNode(node, "ScheduleData");
    QL_REQUIRE(scheduleNode, "No CBOTranches Node");
    scheduleData.fromXML(scheduleNode);

    bondbasketdata.clear();
    XMLNode* bondbasketNode = XMLUtils::getChildNode(node, "BondBasketData");
    QL_REQUIRE(bondbasketNode, "No BondBasketData Node");
    bondbasketdata.fromXML(bondbasketNode);

    trancheData.clear();
    XMLNode* tranchesNode = XMLUtils::getChildNode(node, "CBOTranches");
    QL_REQUIRE(tranchesNode, "No CBOTranches Node");
    for (XMLNode* child = XMLUtils::getChildNode(tranchesNode, "Tranche"); child; child = XMLUtils::getNextSibling(child)) {
        auto data = QuantLib::ext::make_shared<ore::data::TrancheData>();
        data->fromXML(child);
        trancheData.push_back(data);
    }

}

XMLNode* CboReferenceDatum::CboStructure::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CboStructure");

    XMLUtils::addChild(doc, node, "DayCounter", daycounter);
    XMLUtils::addChild(doc, node, "PaymentConvention", paymentConvention);
    XMLUtils::addChild(doc, node, "Currency", ccy);
    XMLUtils::addChild(doc, node, "SeniorFee", seniorFee);
    XMLUtils::addChild(doc, node, "SubordinatedFee", subordinatedFee);
    XMLUtils::addChild(doc, node, "EquityKicker", equityKicker);
    XMLUtils::addChild(doc, node, "FeeDayCounter", feeDayCounter);
    XMLUtils::addChild(doc, node, "ReinvestmentEndDate", reinvestmentEndDate);

    XMLUtils::appendNode(node, scheduleData.toXML(doc));
    XMLUtils::appendNode(node, bondbasketdata.toXML(doc));

    XMLNode* cboTranches = doc.allocNode("CBOTranches");
    XMLUtils::appendNode(node, cboTranches);
    for(size_t i = 0; i < trancheData.size(); i++)
        XMLUtils::appendNode(cboTranches, trancheData[i]->toXML(doc));

    return node;
}

void CboReferenceDatum::fromXML(XMLNode* node) {

    ReferenceDatum::fromXML(node);
    cboStructure_.fromXML(XMLUtils::getChildNode(node, "CboReferenceData"));

}

XMLNode* CboReferenceDatum::toXML(XMLDocument& doc) const {

    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* dataNode = cboStructure_.toXML(doc);
    XMLUtils::setNodeName(doc, dataNode, "CboReferenceData");
    XMLUtils::appendNode(node, dataNode);

    return node;
}


void CBO::populateFromCboReferenceData(const QuantLib::ext::shared_ptr<CboReferenceDatum>& cboReferenceDatum){

    DLOG("populating data cbo from reference data");
    QL_REQUIRE(cboReferenceDatum, "populateFromCboReferenceData(): empty cbo reference datum given");

    if (seniorFee_.empty()) {
        seniorFee_ = cboReferenceDatum->cbostructure().seniorFee;
        TLOG("overwrite SeniorFee with '" << seniorFee_ << "'");
    }

    if (subordinatedFee_.empty()) {
        subordinatedFee_ = cboReferenceDatum->cbostructure().subordinatedFee;
        TLOG("overwrite SubordinatedFee with '" << subordinatedFee_ << "'");
    }

    if (equityKicker_.empty()) {
        equityKicker_ = cboReferenceDatum->cbostructure().equityKicker;
        TLOG("overwrite EquityKicker with '" << equityKicker_ << "'");
    }

    if (feeDayCounter_.empty()) {
        feeDayCounter_ = cboReferenceDatum->cbostructure().feeDayCounter;
        TLOG("overwrite FeeDayCounter with '" << feeDayCounter_ << "'");
    }

    if (ccy_.empty()) {
        ccy_ = cboReferenceDatum->cbostructure().ccy;
        TLOG("overwrite currency with '" << ccy_ << "'");
    }

    if (reinvestmentEndDate_.empty()) {
        reinvestmentEndDate_ = cboReferenceDatum->cbostructure().reinvestmentEndDate;
        TLOG("overwrite ReinvestmentEndDate with '" << reinvestmentEndDate_ << "'");
    }

    if (daycounter_.empty()) {
        daycounter_ = cboReferenceDatum->cbostructure().daycounter;
        TLOG("overwrite DayCounter with '" << daycounter_ << "'");
    }

    if (paymentConvention_.empty()) {
        paymentConvention_ = cboReferenceDatum->cbostructure().paymentConvention;
        TLOG("overwrite PaymentConvention with '" << paymentConvention_ << "'");
    }

    if (!scheduleData_.hasData()) {
        scheduleData_ = cboReferenceDatum->cbostructure().scheduleData;
        TLOG("overwrite ScheduleData");
    }

    if (bondbasketdata_.empty()) {
        bondbasketdata_ = cboReferenceDatum->cbostructure().bondbasketdata;
        TLOG("overwrite BondBasketData");
    }

    if (trancheData_.empty()) {
        trancheData_ = cboReferenceDatum->cbostructure().trancheData;
        TLOG("overwrite TrancheData");
    }

}

void CBO::validateCbo(){

    //Check for mandatroy fields
    //reinvestmentEndDate_ not required
    //TrancheName, Notional, StructuredID already checked in fromXML

    std::string missing = "";

    if (seniorFee_.empty())
        missing.append("SeniorFee ");

    if (subordinatedFee_.empty())
        missing.append("SubordinatedFee ");

    if (equityKicker_.empty())
        missing.append("EquityKicker ");

    if (feeDayCounter_.empty())
        missing.append("FeeDayCounter ");

    if (ccy_.empty())
        missing.append("Currency ");

    if (daycounter_.empty())
        missing.append("DayCounter ");

    if (paymentConvention_.empty())
        missing.append("PaymentConvention ");

    if (!scheduleData_.hasData())
        missing.append("ScheduleData ");

    if (bondbasketdata_.empty())
         missing.append("BondBasketData ");

    if (trancheData_.empty())
        missing.append("TrancheData ");

    if(!missing.empty())
        QL_FAIL("CBO " << structureId_ << " expects " + missing +"elements");

}

} // namespace data
} // namespace ore
