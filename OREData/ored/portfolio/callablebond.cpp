/*
 Copyright (C) 2025 Quaternion Risk Management Ltd

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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/callablebond.hpp>
#include <ored/portfolio/callablebond.hpp>
#include <ored/portfolio/callablebondreferencedata.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/instruments/callablebond.hpp>
#include <qle/utilities/inflation.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

std::vector<QuantExt::CallableBond::CallabilityData>
buildCallabilityData(const CallableBondData::CallabilityData& callData, const Date& openEndDateReplacement) {
    std::vector<QuantExt::CallableBond::CallabilityData> result;
    if (callData.initialised()) {
        QuantLib::Schedule schedule = makeSchedule(callData.dates(), openEndDateReplacement);
        std::vector<Date> callDatesPlusInf = schedule.dates();
        callDatesPlusInf.push_back(Date::maxDate());
        auto styles = buildScheduledVectorNormalised<std::string>(callData.styles(), callData.styleDates(),
                                                                  callDatesPlusInf, "Bermudan", true);
        auto prices = buildScheduledVectorNormalised<double>(callData.prices(), callData.priceDates(), callDatesPlusInf,
                                                             1.0, true);
        auto priceTypes = buildScheduledVectorNormalised<std::string>(callData.priceTypes(), callData.priceTypeDates(),
                                                                      callDatesPlusInf, "Clean", true);
        auto includeAccrual = buildScheduledVectorNormalised<bool>(
            callData.includeAccrual(), callData.includeAccrualDates(), callDatesPlusInf, true, true);

        for (Size i = 0; i < callDatesPlusInf.size() - 1; ++i) {
            QuantExt::CallableBond::CallabilityData::ExerciseType exerciseType;
            QuantExt::CallableBond::CallabilityData::PriceType priceType;
            if (styles[i] == "Bermudan") {
                exerciseType = QuantExt::CallableBond::CallabilityData::ExerciseType::OnThisDate;
            } else if (styles[i] == "American") {
                QL_REQUIRE(
                    callDatesPlusInf.size() > 2,
                    "for exercise style 'American' at least two dates (start, end) are required (call/put data)");
                if (i == callDatesPlusInf.size() - 2)
                    exerciseType = QuantExt::CallableBond::CallabilityData::ExerciseType::OnThisDate;
                else
                    exerciseType = QuantExt::CallableBond::CallabilityData::ExerciseType::FromThisDateOn;
            } else {
                QL_FAIL("invalid exercise style '" << styles[i] << "', expected Bermudan, American (call/put data)");
            }
            if (priceTypes[i] == "Clean") {
                priceType = QuantExt::CallableBond::CallabilityData::PriceType::Clean;
            } else if (priceTypes[i] == "Dirty") {
                priceType = QuantExt::CallableBond::CallabilityData::PriceType::Dirty;
            } else {
                QL_FAIL("invalid price type '" << priceTypes[i] << "', expected Clean, Dirty");
            }
            result.push_back(QuantExt::CallableBond::CallabilityData{callDatesPlusInf[i], exerciseType, prices[i],
                                                                     priceType, includeAccrual[i]});
        }
    }
    return result;
} // buildCallabilityData()

void CallableBondData::CallabilityData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, nodeName_);
    dates_.fromXML(XMLUtils::getChildNode(node, "ScheduleData"));
    styles_ = XMLUtils::getChildrenValuesWithAttributes(node, "Styles", "Style", "startDate", styleDates_, true);
    prices_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Prices", "Price", "startDate", priceDates_,
                                                                &parseReal, true);
    priceTypes_ =
        XMLUtils::getChildrenValuesWithAttributes(node, "PriceTypes", "PriceType", "startDate", priceTypeDates_, true);
    includeAccrual_ = XMLUtils::getChildrenValuesWithAttributes<bool>(
        node, "IncludeAccruals", "IncludeAccrual", "startDate", includeAccrualDates_, &parseBool, true);
    initialised_ = true;
}

XMLNode* CallableBondData::CallabilityData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(nodeName_);
    XMLUtils::appendNode(node, dates_.toXML(doc));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Styles", "Style", styles_, "startDate", styleDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Prices", "Price", prices_, "startDate", priceDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "PriceTypes", "PriceType", priceTypes_, "startDate",
                                                priceTypeDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "IncludeAccruals", "IncludeAccrual", includeAccrual_,
                                                "startDate", includeAccrualDates_);
    return node;
}

void CallableBondData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CallableBondData");
    bondData_.fromXML(XMLUtils::getChildNode(node, "BondData"));
    if (auto tmp = XMLUtils::getChildNode(node, "CallData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty()) {
            callData_.fromXML(tmp);
        }
    }
    if (auto tmp = XMLUtils::getChildNode(node, "PutData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            putData_.fromXML(tmp);
    }
}

XMLNode* CallableBondData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CallableBondData");
    XMLUtils::appendNode(node, bondData_.toXML(doc));
    if (callData_.initialised())
        XMLUtils::appendNode(node, callData_.toXML(doc));
    if (putData_.initialised())
        XMLUtils::appendNode(node, putData_.toXML(doc));
    return node;
}

void CallableBond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    originalData_.fromXML(XMLUtils::getChildNode(node, "CallableBondData"));
    data_ = originalData_;
}

XMLNode* CallableBond::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, originalData_.toXML(doc));
    return node;
}

void CallableBondData::populateFromBondReferenceData(
    const boost::shared_ptr<ore::data::ReferenceDataManager>& referenceData) {

    QL_REQUIRE(!bondData_.securityId().empty(),
               "CallableBondData::populateFromBondReferenceData(): no security id given");
    if (!referenceData || !referenceData->hasData(CallableBondReferenceDatum::TYPE, bondData_.securityId())) {
        DLOG("could not get CallableBondReferenceDatum for name " << bondData_.securityId()
                                                                  << " leave data in trade unchanged");
    } else {
        auto bondRefData = boost::dynamic_pointer_cast<CallableBondReferenceDatum>(
            referenceData->getData(CallableBondReferenceDatum::TYPE, bondData_.securityId()));
        QL_REQUIRE(bondRefData, "could not cast to CallableBondReferenceDatum, this is unexpected");
        DLOG("Got CallableBondReferenceDatum for name " << bondData_.securityId()
                                                        << " overwrite empty elements in trade");
        bondData_.populateFromBondReferenceData(
            boost::make_shared<BondReferenceDatum>(bondData_.securityId(), bondRefData->bondData()));
        if (!callData_.initialised()) {
            DLOG("overwrite CallData from reference data")
            callData_ = bondRefData->callData();
        }
        if (!putData_.initialised()) {
            DLOG("overwrite PutData from reference data")
            putData_ = bondRefData->putData();
        }
    }
}

std::map<AssetClass, std::set<std::string>>
CallableBond::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    data_ = originalData_;
    data_.populateFromBondReferenceData(referenceDataManager);
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {data_.bondData().securityId()};
    return result;
}

void CallableBond::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    DLOG("CallableBond::build() called for trade " << id());

    auto builder = boost::dynamic_pointer_cast<CallableBondEngineBuilder>(engineFactory->builder("CallableBond"));
    QL_REQUIRE(builder, "CallableBond::build(): could not cast to CallableBondBuilder, this is unexpected");

    data_ = originalData_;
    data_.populateFromBondReferenceData(engineFactory->referenceData());

    // build vanilla bond part (i.e. without calls / puts), add to required fixings

    ore::data::Bond underlyingBond(Envelope(), data_.bondData());
    underlyingBond.build(engineFactory);
    requiredFixings_.addData(underlyingBond.requiredFixings());
    auto qlUnderlyingBond = boost::dynamic_pointer_cast<QuantLib::Bond>(underlyingBond.instrument()->qlInstrument());
    QL_REQUIRE(qlUnderlyingBond,
               "CallableBond::build(): internal error, could not cast underlying bond to QuantLib::Bond");
    auto qlUnderlyingBondCoupons = qlUnderlyingBond->cashflows();
    qlUnderlyingBondCoupons.erase(
        std::remove_if(qlUnderlyingBondCoupons.begin(), qlUnderlyingBondCoupons.end(),
                       [](boost::shared_ptr<CashFlow> c) { return boost::dynamic_pointer_cast<Coupon>(c) == nullptr; }),
        qlUnderlyingBondCoupons.end());

    // get open end date replacement from vanilla builder to handle perpetuals

    boost::shared_ptr<EngineBuilder> vanillaBuilder = engineFactory->builder("Bond");
    QL_REQUIRE(builder, "CallableBond::build(): internal error, vanilla builder is null");
    std::string openEndDateStr = vanillaBuilder->modelParameter("OpenEndDateReplacement", {}, false, "");
    Date openEndDateReplacement = getOpenEndDateReplacement(openEndDateStr, parseCalendar(data_.bondData().calendar()));

    // the multiplier, basically the number of bonds and a sign for long / short positions

    Real multiplier = data_.bondData().bondNotional() * (data_.bondData().isPayer() ? -1.0 : 1.0);

    // build callable bond data

    std::vector<QuantExt::CallableBond::CallabilityData> callData =
        buildCallabilityData(data_.callData(), openEndDateReplacement);
    std::vector<QuantExt::CallableBond::CallabilityData> putData =
        buildCallabilityData(data_.putData(), openEndDateReplacement);

    // build callable bond instrument and attach pricing engine

    // get the last relevant date of the convertible bond, this is used as the last calibration date for the model
    Date lastDate = qlUnderlyingBond->maturityDate();

    auto qlInstr = boost::make_shared<QuantExt::CallableBond>(
        qlUnderlyingBond->settlementDays(), qlUnderlyingBond->calendar(), qlUnderlyingBond->issueDate(),
        qlUnderlyingBondCoupons, callData, putData);
    qlInstr->setPricingEngine(builder->engine(id(), data_.bondData().currency(), data_.bondData().creditCurveId(),
                                              data_.bondData().hasCreditRisk(), data_.bondData().securityId(),
                                              data_.bondData().referenceCurveId(), lastDate));

    // set up other trade member variables

    instrument_ = boost::make_shared<VanillaInstrument>(qlInstr, multiplier);
    npvCurrency_ = notionalCurrency_ = data_.bondData().currency();
    maturity_ = qlUnderlyingBond->maturityDate();
    notional_ = qlUnderlyingBond->notional();
    legs_ = {qlUnderlyingBond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {data_.bondData().isPayer()};
}

BondBuilder::Result CallableBondBuilder::build(const boost::shared_ptr<EngineFactory>& engineFactory,
                                               const boost::shared_ptr<ReferenceDataManager>& referenceData,
                                               const std::string& securityId) const {
    CallableBondData data(BondData(securityId, 1.0));
    data.populateFromBondReferenceData(referenceData);
    ore::data::CallableBond bond(Envelope(), data);
    bond.id() = "CallableBondBuilder_" + securityId;
    bond.build(engineFactory);

    QL_REQUIRE(bond.instrument(), "CallableBondBuilder: constructed bond is null, this is unexpected");
    auto qlBond = boost::dynamic_pointer_cast<QuantLib::Bond>(bond.instrument()->qlInstrument());

    QL_REQUIRE(
        qlBond,
        "CallableBondBuilder: constructed bond trade does not provide a valid ql instrument, this is unexpected");

    Result res;
    res.bond = qlBond;
    if (data.bondData().isInflationLinked()) {
        res.isInflationLinked = true;
    }
    res.hasCreditRisk = data.bondData().hasCreditRisk() && !data.bondData().creditCurveId().empty();
    res.currency = data.bondData().currency();
    res.creditCurveId = data.bondData().creditCurveId();
    res.securityId = data.bondData().securityId();
    res.creditGroup = data.bondData().creditGroup();
    res.priceQuoteMethod = data.bondData().priceQuoteMethod();
    res.priceQuoteBaseValue = data.bondData().priceQuoteBaseValue();
    return res;
}

} // namespace data
} // namespace ore
