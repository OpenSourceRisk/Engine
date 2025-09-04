/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

    static const std::string commodity_american_fd_script =
    "   NUMBER Payoff, d, currentNotional;\n"
    "   \n"
    "   currentNotional = Quantity * Strike;\n"
    "   FOR d IN(SIZE(ExerciseDates), 1, -1) DO\n"
    "       Option = NPV(Option, ExerciseDates[d]);\n" 
    "       Payoff = NPV(OptionType * (Underlying(ExerciseDates[d]) - Strike), ExerciseDates[d]);\n "
    "       IF Payoff > Option THEN\n"
    "           Option = Payoff;\n"
    "       END;\n"
    "   END;\n"
    "   Option = Option * Quantity * LongShort;\n";

QuantLib::ext::shared_ptr<ore::data::Trade>
    CommodityAmericanFDScriptedEngineBuilder::build(const Trade* trade,
                                                    const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    auto commodityOption = dynamic_cast<const ore::data::CommodityOption*>(trade);

    QL_REQUIRE(commodityOption != nullptr, "CommodityAmericanFDScriptedEngineBuilder: internal error, could not "
                                            "cast to ore::data::CommodityOption. Contact dev.");

    // Build a ScriptedTrade with inline scripts
    Date today = Settings::instance().evaluationDate();
    Date expiry = parseDate(commodityOption->option().exerciseDates()[0]);
    std::vector<ScriptedTradeEventData> events;
    if (today < expiry)
        events.push_back(ScriptedTradeEventData(
            "ExerciseDates", ScheduleData(ScheduleRules(to_string(today), commodityOption->option().exerciseDates()[0],
                                                        "1D", "WeekendsOnly", "F", "F", "Forward"))));
    else
        events.push_back(ScriptedTradeEventData(
            "ExerciseDates", ScheduleData(ScheduleRules(commodityOption->option().exerciseDates()[0],
                                                        commodityOption->option().exerciseDates()[0], "1D",
                                                        "WeekendsOnly", "F", "F", "Forward"))));

    std::vector<ScriptedTradeValueTypeData> numbers = {
        ScriptedTradeValueTypeData("Number", "Strike", std::to_string(commodityOption->strike().value())),
        ScriptedTradeValueTypeData("Number", "Quantity", std::to_string(commodityOption->quantity())),
        ScriptedTradeValueTypeData("Number", "LongShort",
                                   commodityOption->option().longShort() == "Long" ? "1.0" : "-1.0"),
        ScriptedTradeValueTypeData("Number", "OptionType",
                                   commodityOption->option().callPut() == "Call" ? "1.0" : "-1.0"),
    };
    std::string underlyingStr = commodityOption->asset();
    std::replace(underlyingStr.begin(), underlyingStr.end(), '#', '-');
    std::vector<ScriptedTradeValueTypeData> indices = {
        ScriptedTradeValueTypeData("Index", "Underlying", "COMM-" + underlyingStr)
    };

    std::vector<ScriptedTradeValueTypeData> currencies = {
        ScriptedTradeValueTypeData("Currency", "PayCcy", commodityOption->currency())
    };

    std::vector<ScriptedTradeValueTypeData> dayCounter;

    std::vector<ScriptedTradeScriptData::CalibrationData> calibrationSpec = {
        ScriptedTradeScriptData::CalibrationData("Underlying", std::vector<std::string>{"Strike"})};

    std::map<std::string, ScriptedTradeScriptData> script = {
        {"", ScriptedTradeScriptData(commodity_american_fd_script, "Option",
                                     {{"currentNotional", "currentNotional"}, {"notionalCurrency", "PayCcy"}}, {}, {},
                                     calibrationSpec)}};

    auto scriptedTrade =
        QuantLib::ext::make_shared<ScriptedTrade>(commodityOption->envelope(), events, numbers, indices, currencies,
                                                  dayCounter, script, "CommodityOptionAmerican", "ScriptedTrade");

    auto longShortPremium = commodityOption->option().longShort() == "Long" ? -1 : 1;
    scriptedTrade->build(engineFactory, commodityOption->option().premiumData(), longShortPremium);

    return scriptedTrade;

}
} // namespace data
} // namespace ore
