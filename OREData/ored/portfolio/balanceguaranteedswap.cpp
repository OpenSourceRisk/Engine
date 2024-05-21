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

#include <ored/portfolio/balanceguaranteedswap.hpp>
#include <ored/portfolio/builders/balanceguaranteedswap.hpp>
#include <qle/instruments/balanceguaranteedswap.hpp>

#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void BGSTrancheData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Tranche");
    description_ = XMLUtils::getChildValue(node, "Description", false);
    securityId_ = XMLUtils::getChildValue(node, "SecurityId", true);
    seniority_ = XMLUtils::getChildValueAsInt(node, "Seniority", true);
    notionals_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Notionals", "Notional", "startDate",
                                                                 notionalDates_, &parseReal, true);
}

XMLNode* BGSTrancheData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Tranche");
    XMLUtils::addChild(doc, node, "Description", description_);
    XMLUtils::addChild(doc, node, "SecurityId", securityId_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Notionals", "Notional", notionals_, "startDate",
                                                notionalDates_);
    return node;
}

void BalanceGuaranteedSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    LOG("BalanceGuaranteedSwap::build() for id \"" << id() << "\" called.");

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");  

    QuantLib::Schedule schedule = makeSchedule(this->schedule());

    std::vector<BGSTrancheData> sortedTranches(tranches());
    std::sort(sortedTranches.begin(), sortedTranches.end(),
              [](const BGSTrancheData& x, const BGSTrancheData& y) { return x.seniority() < y.seniority(); });
    std::vector<std::vector<Real>> trancheNotionals;
    Size referencedTranche = Null<Size>(), counter = 0;
    for (auto const& t : sortedTranches) {
        if (t.securityId() == referenceSecurity()) {
            QL_REQUIRE(referencedTranche == Null<Size>(),
                       "there is more than one tranche with id \"" << referenceSecurity() << "\"");
            referencedTranche = counter;
        }
        trancheNotionals.push_back(buildScheduledVectorNormalised(t.notionals(), t.notionalDates(), schedule, 0.0));
        ++counter;
    }
    QL_REQUIRE(referencedTranche != Null<Size>(), "referenced tranche not found");

    QL_REQUIRE(swap_.size() == 2, "swap must have 2 legs");
    QL_REQUIRE(swap_[0].currency() == swap_[1].currency(), "swap must be single currency");

    string ccy_str = swap_[0].currency();
    Currency currency = parseCurrency(ccy_str);

    Size fixedLegIndex, floatingLegIndex;
    if (swap_[0].legType() == "Floating" && swap_[1].legType() == "Fixed") {
        floatingLegIndex = 0;
        fixedLegIndex = 1;
    } else if (swap_[1].legType() == "Floating" && swap_[0].legType() == "Fixed") {
        floatingLegIndex = 1;
        fixedLegIndex = 0;
    } else {
        QL_FAIL("Invalid leg types " << swap_[0].legType() << " + " << swap_[1].legType());
    }

    QuantLib::ext::shared_ptr<FixedLegData> fixedLegData =
        QuantLib::ext::dynamic_pointer_cast<FixedLegData>(swap_[fixedLegIndex].concreteLegData());
    QuantLib::ext::shared_ptr<FloatingLegData> floatingLegData =
        QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(swap_[floatingLegIndex].concreteLegData());

    QL_REQUIRE(fixedLegData != nullptr, "expected fixed leg data");
    QL_REQUIRE(floatingLegData != nullptr, "expected floating leg data");

    QuantLib::ext::shared_ptr<EngineBuilder> tmp = engineFactory->builder("BalanceGuaranteedSwap");
    auto builder = QuantLib::ext::dynamic_pointer_cast<FlexiSwapBGSEngineBuilderBase>(tmp);
    QL_REQUIRE(builder, "No BGS Builder found for \"" << id() << "\"");

    Schedule fixedSchedule = makeSchedule(swap_[fixedLegIndex].schedule());
    Schedule floatingSchedule = makeSchedule(swap_[floatingLegIndex].schedule());
    vector<Real> fixedRate =
        buildScheduledVectorNormalised(fixedLegData->rates(), fixedLegData->rateDates(), fixedSchedule, 0.0);
    vector<Real> spreads = buildScheduledVectorNormalised(floatingLegData->spreads(), floatingLegData->spreadDates(),
                                                          floatingSchedule, 0.0);
    vector<Real> gearings = buildScheduledVectorNormalised(floatingLegData->gearings(), floatingLegData->gearingDates(),
                                                           floatingSchedule, 1.0);
    vector<Real> caps = buildScheduledVectorNormalised(floatingLegData->caps(), floatingLegData->capDates(),
                                                       floatingSchedule, (Real)Null<Real>());
    vector<Real> floors = buildScheduledVectorNormalised(floatingLegData->floors(), floatingLegData->floorDates(),
                                                         floatingSchedule, (Real)Null<Real>());
    std::string floatingIndex = floatingLegData->index();
    DayCounter fixedDayCounter = parseDayCounter(swap_[fixedLegIndex].dayCounter());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(floatingIndex, builder->configuration(MarketContext::pricing));
    DayCounter floatingDayCounter = parseDayCounter(swap_[floatingLegIndex].dayCounter());
    BusinessDayConvention paymentConvention = parseBusinessDayConvention(swap_[floatingLegIndex].paymentConvention());
    VanillaSwap::Type type = swap_[fixedLegIndex].isPayer() ? VanillaSwap::Payer : VanillaSwap::Receiver;

    auto bgSwap = QuantLib::ext::make_shared<QuantExt::BalanceGuaranteedSwap>(
        type, trancheNotionals, schedule, referencedTranche, fixedSchedule, fixedRate, fixedDayCounter,
        floatingSchedule, *index, gearings, spreads, caps, floors, floatingDayCounter, paymentConvention);

    auto fixLeg = bgSwap->leg(0);
    auto fltLeg = bgSwap->leg(1);

    // check that nominal schedule in legs is identical with the one we extracted from the tranche notionals
    Size legRatio = fltLeg.size() / fixLeg.size(); // no remainder by construction of a bg swap
    vector<Real> legFixedNominal = buildScheduledVectorNormalised(
        swap_[fixedLegIndex].notionals(), swap_[fixedLegIndex].notionalDates(), fixedSchedule, 0.0);
    vector<Real> legFloatingNominal = buildScheduledVectorNormalised(
        swap_[floatingLegIndex].notionals(), swap_[floatingLegIndex].notionalDates(), floatingSchedule, 0.0);
    for (Size i = 0; i < legFixedNominal.size(); ++i) {
        QL_REQUIRE(close_enough(bgSwap->trancheNominal(referencedTranche, fixedSchedule[i]), legFixedNominal[i]),
                   "fixed leg notional at " << i << " (" << legFixedNominal[i] << ") does not match tranche notional ("
                                            << bgSwap->trancheNominal(referencedTranche, fixedSchedule[i])
                                            << "), referenced tranche is " << referencedTranche);
    }
    for (Size i = 0; i < legFloatingNominal.size(); ++i) {
        // this is how we build the float notional schedule in the BGS internally as well, i.e. derived from the fixed
        // side
        QL_REQUIRE(close_enough(legFloatingNominal[i], legFixedNominal[i / legRatio]),
                   "floating leg notional at " << i << " (" << legFloatingNominal[i]
                                               << ") does not match fixed leg notional at " << (i / legRatio) << " ("
                                               << legFixedNominal[i / legRatio] << ")");
    }

    // set coupon pricers if needed (for flow report, discounting swap engine, not used in LGM engine)

    bool hasCapsFloors = false;
    for (auto const& k : caps) {
        if (k != Null<Real>())
            hasCapsFloors = true;
    }
    for (auto const& k : floors) {
        if (k != Null<Real>())
            hasCapsFloors = true;
    }
    if (hasCapsFloors) {
        QuantLib::ext::shared_ptr<EngineBuilder> cfBuilder = engineFactory->builder("CapFlooredIborLeg");
        QL_REQUIRE(cfBuilder, "No builder found for CapFlooredIborLeg");
        QuantLib::ext::shared_ptr<CapFlooredIborLegEngineBuilder> cappedFlooredIborBuilder =
            QuantLib::ext::dynamic_pointer_cast<CapFlooredIborLegEngineBuilder>(cfBuilder);
        QL_REQUIRE(cappedFlooredIborBuilder != nullptr, "expected CapFlooredIborLegEngineBuilder");
        QuantLib::ext::shared_ptr<FloatingRateCouponPricer> couponPricer =
            cappedFlooredIborBuilder->engine(IndexNameTranslator::instance().oreName(index->name()));
        QuantLib::setCouponPricer(fltLeg, couponPricer);
    }

    // determine expiries and strikes for calibration basket (simple approach, a la summit)

    std::vector<Date> expiryDates;
    std::vector<Real> strikes;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < fltLeg.size(); ++i) {
        auto fltcpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(fltLeg[i]);
        if (fltcpn != nullptr && fltcpn->fixingDate() > today && i % legRatio == 0) {
            expiryDates.push_back(fltcpn->fixingDate());
            auto fixcpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(fixLeg[i]);
            QL_REQUIRE(fixcpn != nullptr, "BalanceGuaranteedSwap Builder: expected fixed rate coupon");
            strikes.push_back(fixcpn->rate() - fltcpn->spread());
        }
    }

    // set pricing engine, init instrument and other trade members

    bgSwap->setPricingEngine(
        builder->engine(id(), referenceSecurity(), ccy_str, expiryDates, bgSwap->maturityDate(), strikes));
    setSensitivityTemplate(*builder);

    // add required fixings
    addToRequiredFixings(fltLeg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings_));

    // FIXME this won't work for exposure, currently not supported
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(bgSwap);

    npvCurrency_ = ccy_str;
    notional_ = std::max(currentNotional(fixLeg), currentNotional(fltLeg));
    notionalCurrency_ = ccy_str;
    legCurrencies_ = vector<string>(2, ccy_str);
    legs_ = {fixLeg, fltLeg};
    legPayers_ = {swap_[fixedLegIndex].isPayer(), swap_[floatingLegIndex].isPayer()};
    maturity_ = bgSwap->maturityDate();
}

void BalanceGuaranteedSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "BalanceGuaranteedSwapData");
    QL_REQUIRE(swapNode, "BalanceGuaranteedSwap::fromXML(): BalanceGuaranteedSwapData not found");

    referenceSecurity_ = XMLUtils::getChildValue(swapNode, "ReferenceSecurity");

    XMLNode* tranchesNode = XMLUtils::getChildNode(swapNode, "Tranches");
    QL_REQUIRE(tranchesNode, "BalanceGuaranteedSwap::fromXML(): Tranches node not found");
    tranches_.clear();
    vector<XMLNode*> trancheNodes = XMLUtils::getChildrenNodes(tranchesNode, "Tranche");
    for (Size i = 0; i < trancheNodes.size(); ++i) {
        BGSTrancheData td;
        td.fromXML(trancheNodes[i]);
        tranches_.push_back(td);
    }

    XMLNode* scheduleNode = XMLUtils::getChildNode(tranchesNode, "ScheduleData");
    schedule_.fromXML(scheduleNode);

    swap_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); ++i) {
        LegData ld; // we do not allow ORE+ leg types anyway
        ld.fromXML(nodes[i]);
        swap_.push_back(ld);
    }
}

XMLNode* BalanceGuaranteedSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::addChild(doc, node, "ReferenceSecurity", referenceSecurity_);

    XMLNode* tranchesNode = doc.allocNode("Tranches");
    XMLUtils::appendNode(node, tranchesNode);
    for (Size i = 0; i < tranches_.size(); ++i) {
        XMLUtils::appendNode(tranchesNode, tranches_[i].toXML(doc));
    }

    XMLUtils::appendNode(tranchesNode, schedule_.toXML(doc));

    XMLNode* swapNode = doc.allocNode("BalanceGuaranteedSwapData");
    XMLUtils::appendNode(node, swapNode);
    for (Size i = 0; i < swap_.size(); ++i)
        XMLUtils::appendNode(swapNode, swap_[i].toXML(doc));
    return node;
}

} // namespace data
} // namespace ore
