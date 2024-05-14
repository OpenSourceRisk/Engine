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

#include <ored/portfolio/genericbarrieroption.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/to_string.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

// clang-format off

    static const std::string mcscript =
      "        REQUIRE PayoffType == 0 OR PayoffType == 1;\n"
      "        REQUIRE SIZE(Underlyings) == SIZE(TransatlanticBarrierType);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierLevels) / SIZE(Underlyings);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierRebates);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierRebateCurrencies);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierRebatePayTimes);\n"
      "        REQUIRE ExpiryDate >= BarrierMonitoringDates[SIZE(BarrierMonitoringDates)];\n"
      "\n"
      "        NUMBER KnockedIn, KnockedOut, Active, rebate, TransatlanticActive;\n"
      "        NUMBER U, i, k, d, currentNotional, levelIndex;\n"
      "\n"
      "        FOR d IN (1, SIZE(BarrierMonitoringDates), 1) DO\n"
      "\n"
      "          FOR i IN (1, SIZE(BarrierTypes), 1) DO\n"
      "\n"
      "            FOR k IN (1, SIZE(Underlyings), 1) DO\n"
      "              U = Underlyings[k](BarrierMonitoringDates[d]);\n"
      "\n"
      "              levelIndex = ((k - 1) * SIZE(BarrierTypes)) + i;\n"           
      "              IF {BarrierTypes[i] == 1 AND U <= BarrierLevels[levelIndex]} OR\n"
      "                 {BarrierTypes[i] == 2 AND U >= BarrierLevels[levelIndex]} THEN\n"
      "    	           IF KnockedOut == 0 THEN\n"
      "                  KnockedIn = 1;\n"
      "  	           END;\n"
      "              END;\n"
      "\n"  
      "              IF {BarrierTypes[i] == 3 AND U < BarrierLevels[levelIndex]} OR\n"
      "                 {BarrierTypes[i] == 4 AND U > BarrierLevels[levelIndex]} THEN\n"
      "                 IF KikoType == 1 OR { KikoType == 2 AND KnockedIn == 0 } OR { KikoType == 3 AND KnockedIn == 1 } THEN\n"
      "                   IF KnockedOut == 0 THEN\n"
      "                     IF BarrierRebatePayTimes[i] == 0 THEN\n"
      "                       rebate = PAY( LongShort * BarrierRebates[i], BarrierMonitoringDates[d], BarrierMonitoringDates[d], BarrierRebateCurrencies[i] );\n"
      "                     ELSE\n"
      "                       rebate = PAY( LongShort * BarrierRebates[i], BarrierMonitoringDates[d], SettlementDate, BarrierRebateCurrencies[i] );\n"
      "                     END;\n"
      "                   END;\n"
      "                   KnockedOut = 1;\n"
      "                 END;\n"
      "              END;\n"
      "\n"
      "            END;\n"
      "\n"
      "          END;\n"
      "\n"
      "        END;\n"
      "\n"
      "        Active = 1;\n"
      "        FOR i IN (1, SIZE(BarrierTypes),1) DO\n"
      "          IF BarrierTypes[i] == 1 OR BarrierTypes[i] == 2 THEN\n"
      "            Active = 0;\n"
      "          END;\n"
      "        END;\n"
      "\n"
      "        Active = max(Active, KnockedIn) * (1 - KnockedOut);\n"
      "\n"
      "	       IF BarrierRebate != 0 THEN\n"
      "	         rebate = (1 - Active) * PAY( LongShort * BarrierRebate, SettlementDate, SettlementDate, BarrierRebateCurrency );\n"
      "	       END;\n"
      "\n"
      "	       TransatlanticActive = 1;\n"
      "        FOR k IN (1, SIZE(Underlyings), 1) DO\n"
      "          REQUIRE TransatlanticBarrierType[k] >= 0  AND TransatlanticBarrierType[k] <= 4;\n"
      "          IF { TransatlanticBarrierType[k] == 1 AND Underlyings[k](ExpiryDate) >= TransatlanticBarrierLevel[k]  } OR\n"
      "             { TransatlanticBarrierType[k] == 2 AND Underlyings[k](ExpiryDate) <= TransatlanticBarrierLevel[k]  } OR\n"
      "             { TransatlanticBarrierType[k] == 3 AND Underlyings[k](ExpiryDate) < TransatlanticBarrierLevel[k] } OR\n"
      "             { TransatlanticBarrierType[k] == 4 AND Underlyings[k](ExpiryDate) > TransatlanticBarrierLevel[k] } THEN\n"
      "            TransatlanticActive = 0;\n"
      "          END;\n"
      "        END;\n"
      "\n"
      "	       rebate = rebate + Active * (1 - TransatlanticActive) * PAY( TransatlanticBarrierRebate, SettlementDate, SettlementDate, TransatlanticBarrierRebateCurrency );\n"
      "\n"
      "        IF PayoffType == 0 AND SIZE(Underlyings) == 1 THEN\n"
      "	         value = Active * TransatlanticActive * PAY( LongShort * Quantity * max(0, PutCall * (Underlyings[1](ExpiryDate) - Strike)), ExpiryDate, SettlementDate, PayCurrency ) +\n"
      "                  rebate;\n"
      "	       ELSE\n"
      "	         value = Active * TransatlanticActive * PAY( LongShort * Amount, ExpiryDate, SettlementDate, PayCurrency ) +\n"
      "                  rebate;\n"
      "	       END;\n"
      "\n"
      "        IF PayoffType == 0 THEN\n"
      "          currentNotional = Quantity * Strike;\n"
      "        ELSE\n"
      "          currentNotional = Amount;\n"
      "        END;";

    static const std::string fdscript =
      "        REQUIRE PayoffType == 0 OR PayoffType == 1;\n"
      "        REQUIRE SIZE(Underlyings) == SIZE(TransatlanticBarrierType);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierLevels) / SIZE(Underlyings);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierRebates);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierRebateCurrencies);\n"
      "        REQUIRE SIZE(BarrierTypes) == SIZE(BarrierRebatePayTimes);\n"
      "        REQUIRE ExpiryDate >= BarrierMonitoringDates[SIZE(BarrierMonitoringDates)];\n"
      "\n"
      "        NUMBER V, V_V, V_NA, V_KI, V_KO, V_KIKO, V_KOKI;\n"
      "        NUMBER R, R_V, R_NA, R_KI, R_KO, R_KIKO, R_KOKI, rebate;\n"
      "        NUMBER U, i, k, d, currentNotional, TransatlanticActive, IsKnockedIn, IsKnockedOut, levelIndex;\n"
      "\n"
      "        IF PayoffType == 0 AND SIZE(Underlyings) == 1 THEN\n"
      "          V = PAY( LongShort * Quantity * max(0, PutCall * (Underlyings[1](ExpiryDate) - Strike)), ExpiryDate, SettlementDate, PayCurrency );\n"
      "        ELSE\n"
      "          V = PAY( LongShort * Amount, ExpiryDate, SettlementDate, PayCurrency );\n"
      "        END;\n"
      "\n"
      "        TransatlanticActive = 1;\n"
      "        FOR k IN (1, SIZE(Underlyings), 1) DO\n"
      "          REQUIRE TransatlanticBarrierType[k] >= 0  AND TransatlanticBarrierType[k] <= 4;\n"
      "          IF { TransatlanticBarrierType[k] == 1 AND Underlyings[k](ExpiryDate) >= TransatlanticBarrierLevel[k]  } OR\n"
      "             { TransatlanticBarrierType[k] == 2 AND Underlyings[k](ExpiryDate) <= TransatlanticBarrierLevel[k]  } OR\n"
      "             { TransatlanticBarrierType[k] == 3 AND Underlyings[k](ExpiryDate) < TransatlanticBarrierLevel[k] } OR\n"
      "             { TransatlanticBarrierType[k] == 4 AND Underlyings[k](ExpiryDate) > TransatlanticBarrierLevel[k] } THEN\n"
      "            TransatlanticActive = 0;\n"
      "          END;\n"
      "        END;\n"
      "\n"
      "        IF TransatlanticActive == 0 THEN\n"
      "          V = PAY( LongShort * TransatlanticBarrierRebate, ExpiryDate, SettlementDate, TransatlanticBarrierRebateCurrency );\n"
      "        END;\n"
      "\n"
      "        V_V = V;\n"
      "        V_NA = V;\n"
      "        V_KI = V * 0;\n"
      "        V_KO = V * 0;\n"
      "        V_KIKO = V * 0;\n"
      "        V_KOKI = V * 0;\n"
      "\n"
      "        R = PAY( LongShort * BarrierRebate, ExpiryDate, SettlementDate, BarrierRebateCurrency);\n"
      "        R_V = R;\n"
      "        R_NA = R;\n"
      "        R_KI = R * 0;\n"
      "        R_KO = R * 0;\n"
      "        R_KIKO = R * 0;\n"
      "        R_KOKI = R * 0;\n"
      "\n"
      "        FOR i IN (1, SIZE(BarrierTypes), 1) DO\n"
      "          IF BarrierTypes[i] == 1 OR BarrierTypes[i] == 2 THEN\n"
      "            V_V = V_V * 0;\n"
      "	           R_V = R_V * 0;\n"
      "          END;\n"
      "        END;\n"
      "\n"
      "        FOR d IN (SIZE(BarrierMonitoringDates), 1, -1) DO\n"
      "\n"
      "          V_V = NPV(V_V, BarrierMonitoringDates[d]);\n"
      "          V_NA = NPV(V_NA, BarrierMonitoringDates[d]);\n"
      "          V_KI = NPV(V_KI, BarrierMonitoringDates[d]);\n"
      "          V_KO = NPV(V_KO, BarrierMonitoringDates[d]);\n"
      "          V_KIKO = NPV(V_KIKO, BarrierMonitoringDates[d]);\n"
      "          V_KOKI = NPV(V_KOKI, BarrierMonitoringDates[d]);\n"
      "          R_V = NPV(R_V, BarrierMonitoringDates[d]);\n"
      "          R_NA = NPV(R_NA, BarrierMonitoringDates[d]);\n"
      "          R_KI = NPV(R_KI, BarrierMonitoringDates[d]);\n"
      "          R_KO = NPV(R_KO, BarrierMonitoringDates[d]);\n"
      "          R_KIKO = NPV(R_KIKO, BarrierMonitoringDates[d]);\n"
      "          R_KOKI = NPV(R_KOKI, BarrierMonitoringDates[d]);\n"
      "	         rebate = NPV(rebate, BarrierMonitoringDates[d]);\n"
      "\n"
      "          FOR i IN (1, SIZE(BarrierTypes), 1) DO\n"
      "\n"
      "            IsKnockedIn = 0;\n"
      "            IsKnockedOut = 0;\n"
      "            FOR k IN (1, SIZE(Underlyings), 1) DO\n"
      "              U = Underlyings[k](BarrierMonitoringDates[d]);\n"
      "              levelIndex = ((k - 1) * SIZE(BarrierTypes)) + i;\n"
      "              IF {BarrierTypes[i] == 1 AND U <= BarrierLevels[levelIndex]} OR\n"
      "                 {BarrierTypes[i] == 2 AND U >= BarrierLevels[levelIndex]} THEN\n"
      "                IsKnockedIn = 1;"
      "              END;\n"
      "              IF {BarrierTypes[i] == 3 AND U < BarrierLevels[levelIndex]} OR\n"
      "                 {BarrierTypes[i] == 4 AND U > BarrierLevels[levelIndex]} THEN\n"
      "                IsKnockedOut = 1;"
      "              END;\n"
      "            END;\n"
      "\n"
      "            IF {IsKnockedIn == 1} THEN\n"
      "              V_KIKO = V_KO + V_KIKO + V_KOKI;\n"
      "              V_KOKI = V_KOKI * 0;\n"
      "              V_KI = V_NA + V_KI;\n"
      "              V_KO = V_KO * 0;\n"
      "              V_NA = V_NA * 0;\n"
      "              V_V = V_KI;\n"
      "              IF KikoType == 2 THEN\n"
      "                V_V = V_V + V_KIKO;\n"
      "              END;\n"
      "              R_KIKO = R_KO + R_KIKO + R_KOKI;\n"
      "              R_KOKI = R_KOKI * 0;\n"
      "              R_KI = R_NA + R_KI;\n"
      "              R_KO = R_KO * 0;\n"
      "              R_NA = R_NA * 0;\n"
      "              R_V = R_KI;\n"
      "              IF KikoType == 2 THEN\n"
      "                R_V = R_V + R_KIKO;\n"
      "              END;\n"
      "            END;\n"
      "\n"
      "            IF { IsKnockedOut == 1 } THEN\n"
      "              V_KOKI = V_KI + V_KOKI + V_KIKO;\n"
      "              V_KIKO = V_KIKO * 0;\n"
      "              V_KO = V_NA + V_KO;\n"
      "              V_KI = V_KI * 0;\n"
      "              V_NA = V_NA * 0;\n"
      "              IF KikoType == 1 OR KikoType == 2 THEN\n"
      "                V_V = V_V * 0;\n"
      "              END;\n"
      "              R_KOKI = R_KI + R_KOKI + R_KIKO;\n"
      "              R_KIKO = R_KIKO * 0;\n"
      "              R_KO = R_NA + R_KO;\n"
      "              R_KI = R_KI * 0;\n"
      "              R_NA = R_NA * 0;\n"
      "              IF KikoType == 1 OR KikoType == 2 THEN\n"
      "                R_V = R_V * 0;\n"
      "              END;\n"
      "              IF BarrierRebatePayTimes[i] == 0 THEN\n"
      "                rebate = PAY( LongShort * BarrierRebates[i], BarrierMonitoringDates[d], BarrierMonitoringDates[d], BarrierRebateCurrencies[i] );\n"
      "              ELSE\n"
      "                rebate = PAY( LongShort * BarrierRebates[i], BarrierMonitoringDates[d], SettlementDate, BarrierRebateCurrencies[i] );\n"
      "              END;\n"
      "            END;\n"
      "\n"
      "          END;\n"
      "\n"
      "        END;\n"
      "\n"
      "        rebate = NPV(rebate, TODAY);"
      "        R_V = NPV(R_V, TODAY);"
      "        V_V = NPV(V_V, TODAY);"
      "\n"
      "\n"
      "	       rebate = rebate + ( PAY( LongShort * BarrierRebate, TODAY, SettlementDate, BarrierRebateCurrency ) - R_V );\n"
      "        value = V_V + rebate;\n"
      "\n"
      "        IF PayoffType == 0 THEN\n"
      "          currentNotional = Quantity * Strike;\n"
      "        ELSE\n"
      "          currentNotional = Amount;\n"
      "        END;";

// clang-format on

void GenericBarrierOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();
        
    if (underlyings_.size() > 1)
        QL_REQUIRE(optionData_.payoffType() == "CashOrNothing",
                   "Only CashOrNothing payoff allowed for mulitple underlyings");

    std::string payoffType;
    if (optionData_.payoffType() == "Vanilla" || optionData_.payoffType() == "AssetOrNothing")
        payoffType = "0";
    else if (optionData_.payoffType() == "CashOrNothing")
        payoffType = "1";
    else {
        QL_FAIL("PayoffTpye (" << optionData_.payoffType() << ") must be Vanilla or CashOrNothing");
    }
    numbers_.emplace_back("Number", "PayoffType", payoffType);

    QL_REQUIRE(optionData_.payoffType() != "Vanilla" || (!quantity_.empty() && !strike_.empty() && amount_.empty()),
               "Need Quantity, Strike, no Amount for PayoffType = Vanilla");
    QL_REQUIRE(optionData_.payoffType() != "AssetOrNothing" ||
                   (!quantity_.empty() && strike_.empty() && amount_.empty()),
               "Need Quantity, no Strike, no Amount for PayoffType = AssetOrNothing");
    QL_REQUIRE(optionData_.payoffType() != "CashOrNothing" ||
                   (quantity_.empty() && strike_.empty() && !amount_.empty()),
               "Need no Quantity, no Strike, Amount for PayoffType = CashOrNothing");

    std::vector<std::string> transatlanticBarrierType(underlyings_.size(), "0");
    std::vector<std::string> transatlanticBarrierLevel(underlyings_.size(), "0");
    std::string transatlanticBarrierRebate = "0.0";
    std::string transatlanticBarrierRebateCurrency = payCurrency_;
    if (!transatlanticBarrier_[0].type().empty()) {
        transatlanticBarrierType.clear();
        for (auto const& n : transatlanticBarrier_) {
            if (n.type() == "DownAndIn")
                transatlanticBarrierType.push_back("1");
            else if (n.type() == "UpAndIn")
                transatlanticBarrierType.push_back("2");
            else if (n.type() == "DownAndOut")
                transatlanticBarrierType.push_back("3");
            else if (n.type() == "UpAndOut")
                transatlanticBarrierType.push_back("4");
            else {
                QL_FAIL("Transatlantic BarrierType (" << n.type()
                                                      << ") must be DownAndIn, UpAndIn, DownAndOut, UpAndOut");
            }
        }
        QL_REQUIRE(transatlanticBarrierType.size() == 1 || transatlanticBarrierType.size() == underlyings_.size(),
                   "Transatlantic Barrier must have only 1 Barrier block or 1 block for each underlyings, got "
                       << transatlanticBarrierType.size());
        if (transatlanticBarrierType.size() == 1 && underlyings_.size() > 1) {
            transatlanticBarrierType.assign(underlyings_.size(), transatlanticBarrierType[0]);
        }
        transatlanticBarrierLevel.clear();
        if (transatlanticBarrier_.size() == 1) {
            QL_REQUIRE(transatlanticBarrier_[0].levels().size() == underlyings_.size(),
                       "Transatlantic Barrier must have exactly 1 level for each underlying, got "
                           << transatlanticBarrier_[0].levels().size());
            for (const auto& l : transatlanticBarrier_[0].levels())
                transatlanticBarrierLevel.push_back(boost::lexical_cast<std::string>(l.value()));
        } else {
            QL_REQUIRE(transatlanticBarrierType.size() == underlyings_.size(),
                       "Transatlantic Barrier must have exactly 1 level for each underlying, got "
                           << transatlanticBarrier_.size());
            for (auto const& n : transatlanticBarrier_) {
                QL_REQUIRE(n.levels().size() == 1, "Number of level in each barrier block in transatlantic barriers "
                                                   "must be exactly 1 if more than 1 barrier blocks are provided, got "
                                                       << transatlanticBarrier_.size());
                transatlanticBarrierLevel.push_back(boost::lexical_cast<std::string>(n.levels()[0].value()));
            }
        }
        if (transatlanticBarrier_.size() > 1) {
            for (Size i = 1; i < transatlanticBarrier_.size(); i++) {
                QL_REQUIRE(transatlanticBarrier_[i].rebateCurrency().empty() ||
                               transatlanticBarrier_[i].rebateCurrency() == transatlanticBarrier_[0].rebateCurrency(),
                           "Rebate currency for transatlantic barriers must be identical or only given in the first "
                           "transatlantic barrier.");
            }       
        }
        transatlanticBarrierRebate = boost::lexical_cast<std::string>(transatlanticBarrier_[0].rebate());
        if (!transatlanticBarrier_[0].rebateCurrency().empty())
            transatlanticBarrierRebateCurrency = transatlanticBarrier_[0].rebateCurrency();
    }
    numbers_.emplace_back("Number", "TransatlanticBarrierType", transatlanticBarrierType);
    numbers_.emplace_back("Number", "TransatlanticBarrierLevel", transatlanticBarrierLevel);
    numbers_.emplace_back("Number", "TransatlanticBarrierRebate", transatlanticBarrierRebate);
    currencies_.emplace_back("Currency", "TransatlanticBarrierRebateCurrency", transatlanticBarrierRebateCurrency);

    auto positionType = parsePositionType(optionData_.longShort());
    numbers_.emplace_back("Number", "LongShort", positionType == Position::Long ? "1" : "-1");

    if (optionData_.callPut().empty()) {
        QL_REQUIRE(optionData_.payoffType() == "CashOrNothing" || optionData_.payoffType() == "AssetOrNothing",
                   "Payoff type must be vanilla if option type is not givien.");
        numbers_.emplace_back("Number", "PutCall", "1.0");
    } else {
        numbers_.emplace_back("Number", "PutCall",
                              parseOptionType(optionData_.callPut()) == Option::Call ? "1.0" : "-1.0");
    }
    numbers_.emplace_back("Number", "Quantity", quantity_.empty() ? "0.0" : quantity_);
    if (!strike_.empty())
        numbers_.emplace_back("Number", "Strike", strike_);
    numbers_.emplace_back("Number", "Amount", amount_.empty() ? "0.0" : amount_);
    currencies_.emplace_back("Currency", "PayCurrency", payCurrency_);

    QL_REQUIRE(optionData_.exerciseDates().size() == 1,
               "OptionData must contain exactly one ExerciseDate, got " << optionData_.exerciseDates().size());
    events_.emplace_back("ExpiryDate", optionData_.exerciseDates().front());

    if (!settlementDate_.empty()) {
        QL_REQUIRE(
            settlementLag_.empty() && settlementCalendar_.empty() && settlementConvention_.empty(),
            "If SettlementDate is given, no SettlementLag, SettlementCalendar or SettlementConvention must be given.");
        events_.emplace_back("SettlementDate", settlementDate_);
    } else {
        Date ref = parseDate(optionData_.exerciseDates().front());
        Period p = settlementLag_.empty() ? 0 * Days : parsePeriod(settlementLag_);
        Calendar cal =
            settlementCalendar_.empty() ? getUnderlyingCalendar(factory) : parseCalendar(settlementCalendar_);
        BusinessDayConvention conv =
            settlementConvention_.empty() ? Following : parseBusinessDayConvention(settlementConvention_);
        events_.emplace_back("SettlementDate", ore::data::to_string(cal.advance(ref, p, conv)));
    }

    if (barrierMonitoringDates_.hasData()) {
        QL_REQUIRE(barrierMonitoringStartDate_.empty() && barrierMonitoringEndDate_.empty(),
                   "If ScheduleData is given, no StartDate or EndDate must be given");
        events_.emplace_back("BarrierMonitoringDates", barrierMonitoringDates_);
    } else {
        /* build a daily schedule from the given start / end dates and deriving the calendar from
           the underlying */
        QL_REQUIRE(!barrierMonitoringStartDate_.empty() && !barrierMonitoringEndDate_.empty(),
                   "If no ScheduleData is given, StartDate and EndDate must be given");
        events_.emplace_back("BarrierMonitoringDates",
                             ScheduleData(ScheduleRules(barrierMonitoringStartDate_, barrierMonitoringEndDate_, "1D",
                                                        getUnderlyingCalendar(factory).name(), "F", "F", "Forward")));
    }

    std::vector<std::string> barrierTypes, barrierLevels, barrierRebates, barrierRebateCurrencies,
        barrierRebatePayTimes;
    bool hasKi = false, hasKo = false;
    for (auto const& b : barriers_) {
        std::string barrierType;
        if (b.type() == "DownAndIn") {
            barrierType = "1";
            hasKi = true;
        } else if (b.type() == "UpAndIn") {
            barrierType = "2";
            hasKi = true;
        } else if (b.type() == "DownAndOut") {
            barrierType = "3";
            hasKo = true;
        } else if (b.type() == "UpAndOut") {
            barrierType = "4";
            hasKo = true;
        } else {
            QL_FAIL("BarrierType (" << b.type() << ") must be DownAndIn, UpAndIn, DownAndOut, UpAndOut");
        }
        barrierTypes.push_back(barrierType);
        QL_REQUIRE(b.levels().size() == underlyings_.size(), "Barrier must have exactly number of levels as underlyings, got " << b.levels().size());
        for (const auto& l : b.levels())
            barrierLevels.push_back(boost::lexical_cast<std::string>(l.value()));
        barrierRebates.push_back(boost::lexical_cast<std::string>(b.rebate()));
        barrierRebateCurrencies.push_back(b.rebateCurrency().empty() ? payCurrency_ : b.rebateCurrency());
        std::string rebatePayTime;
        if (b.rebatePayTime() == "atHit")
            rebatePayTime = "0";
        else if (b.rebatePayTime() == "atExpiry" || b.rebatePayTime().empty())
            rebatePayTime = "1";
        else {
            QL_FAIL("RebatePayTime (" << b.rebatePayTime() << ") must be atHit, atExpiry");
        }
        barrierRebatePayTimes.push_back(rebatePayTime);
    }

    // if there is at least one ki barrier, all rebates must be identical + atExpiry
    // and are set via BarrierRebate, BarrierRebateCurrency

    std::string barrierRebate = "0.0", barrierRebateCurrency = payCurrency_;
    if (hasKi) {
        for (Size i = 1; i < barrierTypes.size(); ++i) {
            QL_REQUIRE(close_enough(parseReal(barrierRebates[i]), parseReal(barrierRebates[0])) &&
                           barrierRebateCurrencies[i] == barrierRebateCurrencies[0],
                       "If Knock-In barrier is present, all rebates must be identical, found "
                           << barrierRebates[0] << " " << barrierRebateCurrencies[0] << " and " << barrierRebates[i]
                           << " " << barrierRebateCurrencies[i]);
        }
        for (Size i = 0; i < barrierTypes.size(); ++i) {
            QL_REQUIRE(barrierRebatePayTimes[i] == "1",
                       "If Knock-In barrier is present, all rebate pay times must be atExpiry");
        }
        barrierRebate = barrierRebates[0];
        barrierRebateCurrency = barrierRebateCurrencies[0];
        for (Size i = 0; i < barrierTypes.size(); ++i) {
            barrierRebates[i] = "0.0";
            barrierRebateCurrencies[i] = payCurrency_;
        }
    }

    numbers_.emplace_back("Number", "BarrierTypes", barrierTypes);
    numbers_.emplace_back("Number", "BarrierLevels", barrierLevels);
    numbers_.emplace_back("Number", "BarrierRebates", barrierRebates);
    currencies_.emplace_back("Currency", "BarrierRebateCurrencies", barrierRebateCurrencies);
    numbers_.emplace_back("Number", "BarrierRebatePayTimes", barrierRebatePayTimes);
    numbers_.emplace_back("Number", "BarrierRebate", barrierRebate);
    currencies_.emplace_back("Currency", "BarrierRebateCurrency", barrierRebateCurrency);

    std::string kikoType;
    if (kikoType_ == "KoAlways" || kikoType_.empty())
        kikoType = "1";
    else if (kikoType_ == "KoBeforeKi")
        kikoType = "2";
    else if (kikoType_ == "KoAfterKi")
        kikoType = "3";
    else {
        QL_FAIL("KikoType (" << kikoType_ << ") must be KoAlways, KoBeforeKi, KoAfterKi");
    }
    numbers_.emplace_back("Number", "KikoType", kikoType);

    QL_REQUIRE((hasKi && hasKo) || kikoType == "1",
               "KikoType (" << kikoType_ << ") must be KoAlways if there are only Ko or only Ki barriers");
    QL_REQUIRE(!(hasKi && hasKo) || !kikoType_.empty(),
               "KikoType must be given (KoAlways, KoBeforeKi, KoAfterKi) if both Ko and Ki barriers are present");

    // set product tag

    productTag_ = (underlyings_.size() < 2) ? "SingleAssetOptionBwd({AssetClass})"
                                            : "MultiAssetOption({AssetClass})";

    // set script

    script_[""] = ScriptedTradeScriptData(
        mcscript, "value",
        {{"currentNotional", "currentNotional"},
         {"notionalCurrency", "PayCurrency"},
         {"Active", "Active"},
         {"TransatlanticActive", "TransatlanticActive"}},
        {}, {}, {ScriptedTradeScriptData::CalibrationData("Underlyings", {"Strike", "BarrierLevels"})});
    script_["FD"] = ScriptedTradeScriptData(
        fdscript, "value", {{"currentNotional", "currentNotional"}, {"notionalCurrency", "PayCurrency"}}, {}, {},
        {ScriptedTradeScriptData::CalibrationData("Underlyings", {"Strike", "BarrierLevels"})});

    // build trade

    ScriptedTrade::build(factory, optionData_.premiumData(), positionType == QuantLib::Position::Long ? -1.0 : 1.0);
}

void GenericBarrierOption::initIndices() {
    std::vector<std::string> underlyings;
    for (auto const& u : underlyings_)
        underlyings.push_back(scriptedIndexName(u));

    indices_.emplace_back("Index", "Underlyings", underlyings);
}

QuantLib::Calendar GenericBarrierOption::getUnderlyingCalendar(const QuantLib::ext::shared_ptr<EngineFactory>& factory) const {
    Calendar calendar = NullCalendar();
    QL_REQUIRE(underlyings_.size() > 0, "No underlyings provided.");
    IndexInfo ind(scriptedIndexName(underlyings_.front()));
    if (ind.isFx()) {
        /* just take the default calendars for the two currencies, this can be improved once we have
           full fx indices available in the t0 market (TODO) */
        calendar = parseCalendar(ind.fx()->sourceCurrency().code() + "," + ind.fx()->targetCurrency().code());
    } else if (ind.isEq()) {
        /* get the equity calendar from the market */
        calendar = factory->market()
                       ->equityCurve(ind.eq()->name(), factory->configuration(MarketContext::pricing))
                       ->fixingCalendar();
    } else if (ind.isComm()) {
        /* again, just take the default calendar for the comm currency until we have the actual calendar
           available via the market interface */
        calendar =
            parseCalendar(factory->market()
                              ->commodityPriceCurve(ind.commName(), factory->configuration(MarketContext::pricing))
                              ->currency()
                              .code());
    }
    return calendar;
}

void GenericBarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");

    if (XMLNode* underlyingsNode = XMLUtils::getChildNode(dataNode, "Underlyings")) {
        QL_REQUIRE(underlyingsNode, "No Underlyings node");
        auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
        for (auto const& n : underlyings) {
            UnderlyingBuilder underlyingBuilder;
            underlyingBuilder.fromXML(n);
            underlyings_.push_back(underlyingBuilder.underlying());
        }
    } else {
        XMLNode* tmp = XMLUtils::getChildNode(dataNode, "Underlying");
        if (!tmp)
            tmp = XMLUtils::getChildNode(dataNode, "Name");
        UnderlyingBuilder underlyingBuilder;
        underlyingBuilder.fromXML(tmp);
        underlyings_.push_back(underlyingBuilder.underlying());
    }

    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));

    auto barriersNode = XMLUtils::getChildNode(dataNode, "Barriers");
    QL_REQUIRE(barriersNode, "No Barriers node found");
    if (auto sd = XMLUtils::getChildNode(barriersNode, "ScheduleData")) {
        barrierMonitoringDates_.fromXML(sd);
    }
    barrierMonitoringStartDate_ = XMLUtils::getChildValue(barriersNode, "StartDate", false);
    barrierMonitoringEndDate_ = XMLUtils::getChildValue(barriersNode, "EndDate", false);
    kikoType_ = XMLUtils::getChildValue(barriersNode, "KikoType", false, "KoAlways");
    auto barriers = XMLUtils::getChildrenNodes(barriersNode, "BarrierData");
    for (auto const& n : barriers) {
        barriers_.push_back(BarrierData());
        barriers_.back().fromXML(n);
    }

    auto transatlanticBarrierNode = XMLUtils::getChildNode(dataNode, "TransatlanticBarrier");
    if (transatlanticBarrierNode) {
        auto b = XMLUtils::getChildrenNodes(transatlanticBarrierNode, "BarrierData");
        for (auto const& n : b) {
            transatlanticBarrier_.push_back(BarrierData());
            transatlanticBarrier_.back().fromXML(n);
        }
    }

    payCurrency_ = XMLUtils::getChildValue(dataNode, "PayCurrency", true);
    settlementDate_ = XMLUtils::getChildValue(dataNode, "SettlementDate", false);
    settlementLag_ = XMLUtils::getChildValue(dataNode, "SettlementLag", false);
    settlementCalendar_ = XMLUtils::getChildValue(dataNode, "SettlementCalendar", false);
    settlementConvention_ = XMLUtils::getChildValue(dataNode, "SettlementConvention", false);
    quantity_ = XMLUtils::getChildValue(dataNode, "Quantity", false);
    strike_ = XMLUtils::getChildValue(dataNode, "Strike", false);
    amount_ = XMLUtils::getChildValue(dataNode, "Amount", false);

    initIndices();
}

XMLNode* GenericBarrierOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, dataNode);

    
    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    for (auto& n : underlyings_) {
        XMLUtils::appendNode(underlyingsNode, n->toXML(doc));
    }
    XMLUtils::appendNode(dataNode, underlyingsNode);
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));

    XMLNode* barriers = doc.allocNode("Barriers");
    if (barrierMonitoringDates_.hasData())
        XMLUtils::appendNode(barriers, barrierMonitoringDates_.toXML(doc));
    if (!barrierMonitoringStartDate_.empty())
        XMLUtils::addChild(doc, barriers, "StartDate", barrierMonitoringStartDate_);
    if (!barrierMonitoringEndDate_.empty())
        XMLUtils::addChild(doc, barriers, "EndDate", barrierMonitoringEndDate_);
    for (auto& n : barriers_) {
        XMLUtils::appendNode(barriers, n.toXML(doc));
    }
    if (!kikoType_.empty())
        XMLUtils::addChild(doc, barriers, "KikoType", kikoType_);
    XMLUtils::appendNode(dataNode, barriers);

    if (!transatlanticBarrier_[0].type().empty()) {
        XMLNode* transatlanticBarrierNode = doc.allocNode("TransatlanticBarrier");
        for (auto& n : transatlanticBarrier_) {
            XMLUtils::appendNode(transatlanticBarrierNode, n.toXML(doc));
        }
        XMLUtils::appendNode(dataNode, transatlanticBarrierNode);
    }

    XMLUtils::addChild(doc, dataNode, "PayCurrency", payCurrency_);
    if (!settlementDate_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementDate", settlementDate_);
    if (!settlementLag_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementLag", settlementLag_);
    if (!settlementCalendar_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementCalendar", settlementCalendar_);
    if (!settlementConvention_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementConvention", settlementConvention_);

    if (!quantity_.empty())
        XMLUtils::addChild(doc, dataNode, "Quantity", quantity_);
    if (!strike_.empty())
        XMLUtils::addChild(doc, dataNode, "Strike", strike_);
    if (!amount_.empty())
        XMLUtils::addChild(doc, dataNode, "Amount", amount_);

    return node;
}

} // namespace data
} // namespace ore
