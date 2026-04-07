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

/*! \file ored/scripting/utilities.hpp
    \brief some utility functions
    \ingroup utilities
*/
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/model/utilities.hpp>

#include <qle/time/futureexpirycalculator.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>

#include <tuple>

#pragma once

namespace QuantLib {
class GeneralizedBlackScholesProcess;
}

namespace ore {
namespace data {

/*! coarsens given date grid starting at eval date using the given rule, which is of the form
    3M(1W),1Y(1M),5Y(3M),10Y(1Y),50Y(5Y)
    the rough idea is out to 3M at least a 1W spacing is used, output 1Y a 1M spacing etc.
    for the exact algorithm that generates the coarsened grid, see the code */
std::vector<Date> coarsenDateGrid(const std::vector<Date>& date, const std::string& rule,
                                  const Date& referenceDate = Null<Date>());

/*! get product tag + script, if a name is defined in the scriptTrade, get the script from the library, otherwise
  from the trade itself; use the give purpose and fall back on an empty purpose if specified */
std::pair<std::string, ScriptedTradeScriptData> getScript(const ScriptedTrade& scriptedTrade,
                                                          const ScriptLibraryData& scriptLibrary,
                                                          const std::string& purpose,
                                                          const bool fallBackOnEmptyPurpose);

/*! parse script and return ast */
ASTNodePtr parseScript(const std::string& code);

/*! check whether variable name is already present in given context, if yes throw an exception */
void checkDuplicateName(const QuantLib::ext::shared_ptr<Context> context, const std::string& name);

/*! build a context from the given data and apply the given gridCoarsening rule, if required */
QuantLib::ext::shared_ptr<Context> makeContext(const Size nPaths, const std::string& gridCoarsening,
                                       const std::vector<std::string>& schedulesEligibleForCoarsening,
                                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                       const std::vector<ScriptedTradeEventData>& events,
                                       const std::vector<ScriptedTradeValueTypeData>& numbers,
                                       const std::vector<ScriptedTradeValueTypeData>& indices,
                                       const std::vector<ScriptedTradeValueTypeData>& currencies,
                                       const std::vector<ScriptedTradeValueTypeData>& daycounters);

/*! add new schedules (as specified in the script node) to schedules */
void addNewSchedulesToContext(QuantLib::ext::shared_ptr<Context> context,
                              const std::vector<ScriptedTradeScriptData::NewScheduleData>& newSchedules);

/*! maend the variables sizes in a context to a new size, this is only possible for deterministic variables */
void amendContextVariablesSizes(QuantLib::ext::shared_ptr<Context> context, const Size newSize);

/*! Builds an index (EQ-SP5-EUR, FX-ECB-EUR-USD, ...) that can be used in scripted trades, from an underlying */
std::string scriptedIndexName(const QuantLib::ext::shared_ptr<Underlying>& underlying);

/*! Get map index => calibration strikes as vector<Real> from calibration spec and context */
std::map<std::string, std::vector<Real>>
getCalibrationStrikes(const std::vector<ScriptedTradeScriptData::CalibrationData>& calibrationSpec,
                      const QuantLib::ext::shared_ptr<Context>& context);

} // namespace data
} // namespace ore
