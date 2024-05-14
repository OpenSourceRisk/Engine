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

#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/context.hpp>

#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/utilities/indexparser.hpp>

#include <qle/time/futureexpirycalculator.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>

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

/*! convert a IR / FX / EQ index name to a correlation label that is understood by the cam builder;
    return the tenor of the index too (or 0*Days if not applicable) */
std::pair<std::string, Period>
convertIndexToCamCorrelationEntry(const std::string& i);

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

/*! - helper class that takes and index name string and provides the index type and a parsed version of
      the index with no market data attached
    - commodity indices can be of the extended form for scripting, see parseScriptedCommodityIndex for details 
    - if a market is given, the class attempts to retrieve an eq index from the market, so that it has the 
      correct business day calendar (market curve will be attached to the index too in this case)
*/
class IndexInfo {
public:
    // ctor taking the ORE name of an index and conventions
    explicit IndexInfo(const std::string& name, const QuantLib::ext::shared_ptr<Market>& market = nullptr);
    // the (ORE, i.e. Input) name of the index
    std::string name() const { return name_; }
    // type of index
    bool isFx() const { return isFx_; }
    bool isEq() const { return isEq_; }
    bool isComm() const { return isComm_; }
    bool isIr() const { return isIr_; }
    bool isIrIbor() const { return isIrIbor_; }
    bool isIrSwap() const { return isIrSwap_; }
    bool isInf() const { return isInf_; }
    bool isGeneric() const { return isGeneric_; }
    // get parsed version of index, or null if na
    QuantLib::ext::shared_ptr<FxIndex> fx() const { return fx_; }
    QuantLib::ext::shared_ptr<EquityIndex2> eq() const { return eq_; }
    QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>
    // requires obsDate + conventions for forms 3-6 below, throws otherwise
    comm(const Date& obsDate = Date()) const;
    QuantLib::ext::shared_ptr<InterestRateIndex> ir() const { return ir_; }
    QuantLib::ext::shared_ptr<IborIndex> irIbor() const { return irIbor_; }
    // nullptr if it is no ibor fallback index
    QuantLib::ext::shared_ptr<FallbackIborIndex> irIborFallback(const IborFallbackConfig& iborFallbackConfig,
                                                        const Date& asof = QuantLib::Date::maxDate()) const;
    // nullptr if it is no overnight fallback index
    QuantLib::ext::shared_ptr<FallbackOvernightIndex> irOvernightFallback(const IborFallbackConfig& iborFallbackConfig,
								  const Date& asof = QuantLib::Date::maxDate()) const;
    QuantLib::ext::shared_ptr<SwapIndex> irSwap() const { return irSwap_; }
    QuantLib::ext::shared_ptr<ZeroInflationIndex> inf() const { return inf_; }
    QuantLib::ext::shared_ptr<Index> generic() const { return generic_; }
    // get ptr to base class (comm forms 3-6 below require obsDate + conventions, will throw otherwise)
    QuantLib::ext::shared_ptr<Index>
    index(const Date& obsDate = Date()) const;
    // get comm underlying name NYMEX:CL (no COMM- prefix, no suffixes)
    std::string commName() const;
    // get inf name INF-EUHICP (i.e. without the #L, #F suffix)
    std::string infName() const;
    // comparisons (based on the name)
    bool operator==(const IndexInfo& j) const { return name() == j.name(); }
    bool operator!=(const IndexInfo& j) const { return !(*this == j); }
    bool operator<(const IndexInfo& j) const { return name() < j.name(); }
    bool operator>(const IndexInfo& j) const { return j < (*this); }
    bool operator<=(const IndexInfo& j) const { return !((*this) > j); }
    bool operator>=(const IndexInfo& j) const { return !((*this) < j); }

private:
    std::string name_;
    QuantLib::ext::shared_ptr<Market> market_; // might be null
    bool isFx_, isEq_, isComm_, isIr_, isInf_, isIrIbor_, isIrSwap_, isGeneric_;
    QuantLib::ext::shared_ptr<FxIndex> fx_;
    QuantLib::ext::shared_ptr<EquityIndex2> eq_;
    QuantLib::ext::shared_ptr<InterestRateIndex> ir_;
    QuantLib::ext::shared_ptr<IborIndex> irIbor_;
    QuantLib::ext::shared_ptr<SwapIndex> irSwap_;
    QuantLib::ext::shared_ptr<ZeroInflationIndex> inf_;
    QuantLib::ext::shared_ptr<Index> generic_;
    std::string commName_, infName_;
};

std::ostream& operator<<(std::ostream& o, const IndexInfo& i);

/*! This method tries to parse an commodity index name used in the scripting context

    0) COMM-name
    1) COMM-name-YYYY-MM-DD
    2) COMM-name-YYYY-MM
    ===
    3) CMMM-name#N#D#Cal
    4) COMM-name#N#D
    5) COMM-name#N
    ===
    6) COMM-name!N

    Here 0) - 2) are corresponding to the usual ORE conventions while 3) - 6) are specific to the scripting
    module: Expressions of the form 3) - 5) are resolved to one of the forms 1) and 2) using a given commodity
    future expiry calculator as follows:

    3) COMM-name#N#D#Cal is resolved to the (N+1)th future with expiry greater than the given obsDate advanced by D
                         business days w.r.t. Calendar Cal, N >= 0
    4) as 3), Cal is taken as the commodity index's fixing calendar
    5) as 4), D is set to 0 if not given
    6) COMM-name!N is resolved to the future with month / year equal to the obsDate and monthOffst = N, N >=0

    Notice that the forms 1) and 2) can be parsed without an obsDate and a commodity future convention given. If no
    convention is given, the fixing calendar in the index is set to the NullCalendar. In case a commodity future
    convention is given for the name, the fixing calendar is set to the calendar from the convention.

    TODO if the form is COMM-name-YYYY-MM, the day of month of the expiry date will be set to 01, consistently
    with the ORE index parser, even if a convention is present, that would allow us to determine the correct
    expiry date. Should we use that latter date in the returned index?

    Forms 3) to 6) on the other hand require a commodity future convention in any case, and an obsDate.
*/

QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> parseScriptedCommodityIndex(const std::string& indexName,
                                                                        const QuantLib::Date& obsDate = Date());

/*! This method tries to parse an inflation index name used in the scripting context

  1) EUHICPXT
  2) EUHICPXT#F
  3) EUHICPXT#L

  Here 1)   is the original form used in ORE. This represents a non-interpolated index.
       2,3) is the extended form including a flag indicating the interpolation F (flat, =1) or L (linear)

  The function returns a ql inflation index accounting for the interpolation (but without ts attached),
  and the ORE index name without the #F, #L suffix.
*/
std::pair<QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>, std::string>
parseScriptedInflationIndex(const std::string& indexName);

/*! Builds an index (EQ-SP5-EUR, FX-ECB-EUR-USD, ...) that can be used in scripted trades, from an underlying */
std::string scriptedIndexName(const QuantLib::ext::shared_ptr<Underlying>& underlying);

/*! Get inflation simulation lag in calendar days */
Size getInflationSimulationLag(const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index);

/*! Get map index => calibration strikes as vector<Real> from calibration spec and context */
std::map<std::string, std::vector<Real>>
getCalibrationStrikes(const std::vector<ScriptedTradeScriptData::CalibrationData>& calibrationSpec,
                      const QuantLib::ext::shared_ptr<Context>& context);

} // namespace data
} // namespace ore
