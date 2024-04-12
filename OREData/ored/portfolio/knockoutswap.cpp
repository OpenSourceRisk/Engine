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

#include <ored/portfolio/knockoutswap.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

void KnockOutSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    clear();

    QL_REQUIRE(legData_.size() == 2, "Expected exactly two legs, got " << legData_.size());

    std::set<std::string> legTypes;
    for (auto const& ld : legData_) {
	legTypes.insert(ld.legType());
    }

    QL_REQUIRE(legTypes.size() == 2 && *legTypes.begin() == "Fixed" && *std::next(legTypes.begin(), 1) == "Floating",
	       "Expected one 'Floating' and one 'Fixed' type");

    const LegData& fixedLegData = legData_[0].legType() == "Fixed" ? legData_[0] : legData_[1];
    const LegData& floatLegData = legData_[0].legType() == "Fixed" ? legData_[1] : legData_[0];

    auto floatAddData = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(floatLegData.concreteLegData());
    auto fixedAddData = QuantLib::ext::dynamic_pointer_cast<FixedLegData>(fixedLegData.concreteLegData());

    QL_REQUIRE(floatAddData, "Internal error: could not cast to float additional data");
    QL_REQUIRE(fixedAddData, "Internal error: could not cast to fixed additional data");

    QL_REQUIRE(fixedLegData.isPayer() != floatLegData.isPayer(), "Expected one payer and one receiver leg");

    if (fixedLegData.isPayer())
	numbers_.emplace_back("Number", "Payer", "-1");
    else
	numbers_.emplace_back("Number", "Payer", "1");

    QL_REQUIRE(fixedLegData.notionals().size() == 1,
	       "Expected one notional on fixed leg, got " << fixedLegData.notionals().size());
    QL_REQUIRE(floatLegData.notionals().size() == 1,
	       "Expected one notional on floating leg, got " << floatLegData.notionals().size());
    QL_REQUIRE(QuantLib::close_enough(fixedLegData.notionals().front(), floatLegData.notionals().front()),
	       "Expected same notional on fixed and floating leg, got " << fixedLegData.notionals().front() << " and "
									<< floatLegData.notionals().front());

    QL_REQUIRE(fixedAddData->rates().size() == 1,
	       "Expected one rate on fixed leg, got " << fixedAddData->rates().size());
    QL_REQUIRE(floatAddData->spreads().size() <= 1,
	       "Expected at most one spread on floating leg, got " << floatAddData->spreads().size());
    QL_REQUIRE(floatAddData->gearings().size() <= 1,
	       "Expected at most one gearing on floating leg, got " << floatAddData->gearings().size());

    numbers_.emplace_back("Number", "Notional", std::to_string(fixedLegData.notionals().front()));
    numbers_.emplace_back("Number", "FixedRate", std::to_string(fixedAddData->rates().front()));
    numbers_.emplace_back("Number", "FloatMargin",
			  floatAddData->spreads().empty() ? "0.0" : std::to_string(floatAddData->spreads().front()));
    numbers_.emplace_back("Number", "FloatGearing",
			  floatAddData->gearings().empty() ? "1.0" : std::to_string(floatAddData->gearings().front()));

    auto index = parseIborIndex(floatAddData->index());
    Size fixingShift = floatAddData->fixingDays() == Null<Size>() ? index->fixingDays() : floatAddData->fixingDays();
    std::string fixingCalendar = index->fixingCalendar().name();
    events_.emplace_back("FloatFixingSchedule", "FloatSchedule", "-" + std::to_string(fixingShift) + "D", fixingCalendar, "P");

    indices_.emplace_back("Index", "FloatIndex", floatAddData->index());

    std::string floatDayCounter =
	floatLegData.dayCounter().empty() ? index->dayCounter().name() : floatLegData.dayCounter();
    std::string fixedDayCounter = fixedLegData.dayCounter().empty() ? floatDayCounter : fixedLegData.dayCounter();

    daycounters_.emplace_back("Daycounter", "FixedDayCounter", fixedDayCounter);
    daycounters_.emplace_back("Daycounter", "FloatDayCounter", floatDayCounter);

    events_.emplace_back("FixedSchedule", fixedLegData.schedule());
    events_.emplace_back("FloatSchedule", floatLegData.schedule());

    QL_REQUIRE(!fixedLegData.currency().empty() && fixedLegData.currency() == floatLegData.currency(),
	       "Both legs must have the same currency, got '" << fixedLegData.currency() << "' on the fixed leg and '"
							      << floatLegData.currency() << "' on the floating leg.");
    QL_REQUIRE(fixedLegData.currency() == index->currency().code(),
	       "Leg currency '" << fixedLegData.currency() << "' must match float index currency '"
				<< index->currency().code() << "' of index '" << index->name() << "'");

    currencies_.emplace_back("Currency", "PayCurrency", fixedLegData.currency());

    QL_REQUIRE(barrierData_.style().empty() || barrierData_.style() == "European",
	       "Expected European barrier style, got '" << barrierData_.style() << "'");

    auto barrierType = parseBarrierType(barrierData_.type());
    if (barrierType == QuantLib::Barrier::DownOut)
	numbers_.emplace_back("Number", "KnockOutType", "3");
    else if (barrierType == QuantLib::Barrier::UpOut)
	numbers_.emplace_back("Number", "KnockOutType", "4");
    else {
	QL_FAIL("Expected BarrierType 'DownAndOut' or 'UpAndOut', got '" << barrierData_.type());
    }

    QL_REQUIRE(barrierData_.levels().size() == 1, "Expected exactly one barrier level");
    QL_REQUIRE(barrierData_.levels().front().value() != Null<Real>(), "No barrier level specified.");

    numbers_.emplace_back("Number", "KnockOutLevel", std::to_string(barrierData_.levels().front().value()));

    events_.emplace_back("BarrierStartDate", barrierStartDate_);

    // set product tag

    productTag_ = "SingleUnderlyingIrOption";

    // set script

    bool isIborBased = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndex>(index) == nullptr;

    // clang-format off

    // FloatFixingSchedule has one date more than needed at the back because it is derived from FloatSchedule
    // there are two variants of the float payoff scripting depending on isIborBased

    std::string mc_script = std::string(
      "REQUIRE KnockOutType == 3 OR KnockOutType == 4;\n"
      "NUMBER Alive[SIZE(FloatFixingSchedule)], aliveInd, lastFixedIndex, lastFloatIndex, d, j, fix;\n"

      "aliveInd = 1;\n"

      "FOR d IN (1, SIZE(FloatFixingSchedule), 1) DO\n"

      "   FOR j IN (lastFixedIndex + 1, SIZE(FixedSchedule) - 1, 1) DO\n"
      "     IF FixedSchedule[j] < FloatFixingSchedule[d] OR d == SIZE(FloatFixingSchedule) THEN\n"
      "        value = value + LOGPAY( Payer * aliveInd * Notional * FixedRate * dcf( FixedDayCounter, FixedSchedule[j], FixedSchedule[j+1]),\n"
      "                             FixedSchedule[j], FixedSchedule[j+1], PayCurrency, 1, FixedLegCoupon );\n"
      "        lastFixedIndex = j;\n"
      "      END;\n"
      "    END;\n"

      "    FOR j IN (lastFloatIndex + 1, SIZE(FloatSchedule) - 1, 1) DO\n"
      "      IF FloatSchedule[j] < FloatFixingSchedule[d] OR d == SIZE(FloatFixingSchedule) THEN\n"
      "        value = value + LOGPAY( (-Payer) * aliveInd * Notional *\n") + std::string(
	isIborBased ?
      "                             ( FloatGearing * FloatIndex(FloatFixingSchedule[j]) + FloatMargin)\n"
	:
      "                             FWDCOMP(FloatIndex, FloatFixingSchedule[j], FloatSchedule[j], FloatSchedule[j+1], FloatMargin, FloatGearing)\n") + std::string(

      "                             * dcf( FloatDayCounter, FloatSchedule[j], FloatSchedule[j+1]),\n"
      "                             FloatFixingSchedule[j], FloatSchedule[j+1], PayCurrency, 2, FloatingLegCoupon );\n"
      "        lastFloatIndex = j;\n"
      "      END;\n"
      "    END;\n"

      "   IF d < SIZE(FloatFixingSchedule) THEN\n"
      "     fix = FloatIndex(FloatFixingSchedule[d]);\n"
      "     IF FloatFixingSchedule[d] >= BarrierStartDate AND\n"
      "        {{KnockOutType == 3 AND fix <= KnockOutLevel} OR\n"
      "         {KnockOutType == 4 AND fix >= KnockOutLevel}} THEN\n"
      "       aliveInd = 0;\n"
      "     END;\n"
      "     Alive[d] = aliveInd;\n"
      "   END;\n"

      "END;\n");

    // clang-format on

    script_[""] = ScriptedTradeScriptData(
	mc_script, "value", {{"currentNotional", "Notional"}, {"notionalCurrency", "PayCurrency"}, {"Alive", "Alive"}},
	{}, {}, {}, {});

    // build trade

    ScriptedTrade::build(factory);
}

void KnockOutSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");

    barrierData_.fromXML(XMLUtils::getChildNode(dataNode, "BarrierData"));
    barrierStartDate_ = XMLUtils::getChildValue(dataNode, "BarrierStartDate", true);

    legData_.clear();
    for (auto const n : XMLUtils::getChildrenNodes(dataNode, "LegData")) {
	legData_.push_back(ore::data::LegData());
	legData_.back().fromXML(n);
    }
}

XMLNode* KnockOutSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(dataNode, barrierData_.toXML(doc));
    XMLUtils::addChild(doc, dataNode, "BarrierStartDate", barrierStartDate_);
    for (auto& ld : legData_) {
	XMLUtils::appendNode(dataNode, ld.toXML(doc));
    }
    XMLUtils::appendNode(node, dataNode);
    return node;
}

} // namespace data
} // namespace ore
