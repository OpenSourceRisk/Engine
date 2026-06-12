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

#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/utilities.hpp>

#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/utilities/inflation.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/models/marketmodels/browniangenerators/mtbrowniangenerator.hpp>
#include <ql/settings.hpp>
#include <ql/optional.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <set>

namespace ore {
namespace data {
using namespace QuantLib;
using namespace QuantExt;
    
std::pair<std::vector<Date>, std::vector<Date>> coarsenDateGrid(const std::vector<Date>& dates, const std::string& rule,
    const Date& referenceDate, const std::vector<Date>& unadjDates) {

    // if rule is empty return original grid

    if (rule.empty())
        return {dates, unadjDates};

    // An initial check that if `unadjDates` is non-empty, it is the same size as `dates`.
    QL_REQUIRE(unadjDates.empty() || unadjDates.size() == dates.size(), "coarsenDateGrid: if unadjusted dates are "
        "provided, they should be the same size as the original date grid");

    // get ref date and prepare result vector

    Date refDate = referenceDate == Null<Date>() ? Settings::instance().evaluationDate() : referenceDate;

    std::vector<Date> result;
    std::vector<Date> unadjResult;

    // parse the rule

    std::vector<std::pair<Period, Period>> grid;
    std::vector<std::string> tokens;
    boost::split(tokens, rule, boost::is_any_of(","));
    for (auto const& t : tokens) {
        std::vector<std::string> tmp;
        boost::split(tmp, t, boost::is_any_of("()"));
        QL_REQUIRE(tmp.size() == 3, "coarsenGrid: invalid rule token '" << t << "', expected e.g. '10Y(1M)'");
        grid.push_back(std::make_pair(parsePeriod(tmp[0]), parsePeriod(tmp[1])));
    }

    // keep all dates <= refDate (and corresponding unadjusted dates if provided).
    auto d = dates.begin();
    if (!unadjDates.empty()) {
        for (; d != dates.end() && *d <= refDate; ++d)
            result.push_back(*d);
    } else {
        auto u = unadjDates.begin();
        for (; d != dates.end() && *d <= refDate; ++d, ++u) {
            result.push_back(*d);
            unadjResult.push_back(*u);
        }
    }

    // step through the rule grid...

    Date start = refDate;
    for (auto const& p : grid) {
        Date end = refDate + p.first;

        do {

            // look at subperiods defined by the second tenor in the rule

            start = std::min(end, start + p.second);

            // avoid too short stubs at the end
            if (static_cast<double>(end - start) / static_cast<double>(end - (end - p.second)) < 0.2)
                start = end;

            // for each subperiod keep at most one date, if there are several in the subperiod, keep the latest one

            std::vector<Date> candidates;
            while (d != dates.end() && *d <= start)
                candidates.push_back(*d++);

            if (!candidates.empty()) {
                result.push_back(candidates.back());
                if (!unadjDates.empty()) {
                    unadjResult.push_back(unadjDates[std::distance(dates.begin(), d) - 1]);
                }
            }

        } while (start < end);

        start = end;
    }

    return {result, unadjResult};
}

std::pair<std::string, ScriptedTradeScriptData> getScript(const ScriptedTrade& scriptedTrade,
                                                          const ScriptLibraryData& scriptLibrary,
                                                          const std::string& purpose,
                                                          const bool fallBackOnEmptyPurpose) {
    if (!scriptedTrade.scriptName().empty()) {
        DLOG("get script '" << scriptedTrade.scriptName() << "' for purpose '" << purpose
                            << "' (fallBackOnEmptyPurpose=" << std::boolalpha << fallBackOnEmptyPurpose
                            << ") from script library");
        return scriptLibrary.get(scriptedTrade.scriptName(), purpose, fallBackOnEmptyPurpose);
    } else {
        DLOG("get script for purpose '" << purpose << "' (fallBackOnEmptyPurpose=" << std::boolalpha
                                        << fallBackOnEmptyPurpose << ") from inline script in scripted trade");
        return std::make_pair(scriptedTrade.productTag(), scriptedTrade.script(purpose, fallBackOnEmptyPurpose));
    }
}

ASTNodePtr parseScript(const std::string& code) {
    ScriptParser parser(code);
    DLOG("parsing script (size " << code.size() << ")");
    if (parser.success()) {
        DLOG("successfully parsed the script");
    } else {
        ALOG("an error occured during script parsing:");
        LOGGERSTREAM(parser.error());
        LOG("full script is:");
        LOG("<<<<<<<<<<");
        LOGGERSTREAM(code);
        LOG(">>>>>>>>>>");
        QL_FAIL("scripted trade could not be built due to parser errors, see log for more details.");
    }
    return parser.ast();
}

void checkDuplicateName(const QuantLib::ext::shared_ptr<Context> context, const std::string& name) {
    auto scalar = context->scalars.find(name);
    auto array = context->arrays.find(name);
    QL_REQUIRE(scalar == context->scalars.end() && array == context->arrays.end(),
               "variable '" << name << "' already declared.");
}

namespace {

using std::map;
using std::set;
using std::string;
using std::vector;

// Vertex is simply a schedule name.
struct VertexData {
    std::string scheduleName;
};

// Want to have a directed graph where vertices are schedules and there is an edge from A to B if schedule A depends on
// schedule B. We will then do a topological sort of this graph to get the order in which to build the schedules.
using Graph = boost::adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::directedS,
    VertexData>;

using Vertex = boost::graph_traits<Graph>::vertex_descriptor;

// A small helper that uses boost graph to order the building of derived scehdules below.
vector<string> derivedScheduleOrder(map<string, ScriptedTradeEventData> derivedSchedules,
    const set<string>& builtSchedules) {

    Graph graph;

    // Mapping from schedule name to vertex descriptor for vertices in the graph.
    std::unordered_map<std::string, Vertex> mpVertices;

    // Create vertices for all derived schedule names.
    for (const auto& entry : derivedSchedules) {
        const auto& schedName = entry.first;
        Vertex v = boost::add_vertex(graph);
        graph[v].scheduleName = schedName;
        mpVertices.emplace(schedName, v);
    }

    // Add an edge from derived schedule to base schedule. Fail if base schedule is not available.
    for (const auto& [schedName, schedData] : derivedSchedules) {
        if (builtSchedules.contains(schedData.baseSchedule()))
            continue;
        Vertex schedVertex = mpVertices.at(schedName);
        auto itDep = mpVertices.find(schedData.baseSchedule());
        QL_REQUIRE(itDep != mpVertices.end(), "makeContext: base schedule '" << schedData.baseSchedule() <<
            "' not found for derived schedule '" << schedName << "'");
        boost::add_edge(itDep->second, schedVertex, graph);
    }

    // Topological sort with check for cycles.
    std::vector<Vertex> schedulesSorted;
    try {
        boost::topological_sort(graph, std::back_inserter(schedulesSorted));
    } catch (const boost::not_a_dag&) {
        QL_FAIL("makeContext: circular dependency detected among derived schedules.");
    }

    // Reverse the order to get the correct order for building the schedules and return the result.
    vector<string> result;
    result.reserve(schedulesSorted.size());
    for (auto it = schedulesSorted.rbegin(); it != schedulesSorted.rend(); ++it) {
        result.push_back(graph[*it].scheduleName);
    }
    return result;
}

} // namespace

QuantLib::ext::shared_ptr<Context> makeContext(Size nPaths, const std::string& gridCoarsening,
                                       const std::vector<std::string>& schedulesEligibleForCoarsening,
                                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                       const std::vector<ScriptedTradeEventData>& events,
                                       const std::vector<ScriptedTradeValueTypeData>& numbers,
                                       const std::vector<ScriptedTradeValueTypeData>& indices,
                                       const std::vector<ScriptedTradeValueTypeData>& currencies,
                                       const std::vector<ScriptedTradeValueTypeData>& daycounters) {

    // In make context below, if we hit a derived schedule that has `ShiftAnchor` set to `Unadjusted`, we need to have 
    // an unadjusted version of the base schedule available. We do a first pass here over the events to identify the 
    // names of the base schedules that appear in a derived schedule with `ShiftAnchor` set to `Unadjusted`. We store 
    // the base schedule name as a key in the `unadjustedBaseSchedules` map and populate the vector of dates below in 
    // the main pass if necessary.
    map<string, vector<Date>> unadjustedBaseSchedules;
    for (const auto& event : events) {
        if (event.type() == ScriptedTradeEventData::Type::Derived) {
            const ext::optional<DateDeltaAnchor>& anchor = event.shiftAnchor();
            if (anchor && *anchor == DateDeltaAnchor::Unadjusted) {
                unadjustedBaseSchedules.try_emplace(event.baseSchedule());
            }
        }
    }

    TLOG("make context");
    auto context = QuantLib::ext::make_shared<Context>();
    map<string, ScriptedTradeEventData> derivedSchedules;
    // keep track of schedules we have built so far
    set<string> builtSchedules;
    for (auto const& x : events) {
        TLOG("adding event " << x.name());
        if (x.type() == ScriptedTradeEventData::Type::Value) {
            checkDuplicateName(context, x.name());
            Date d = parseDate(x.value());
            context->scalars[x.name()] = EventVec{nPaths, d};
            builtSchedules.insert(x.name());
        } else if (x.type() == ScriptedTradeEventData::Type::Array) {
            checkDuplicateName(context, x.name());
            QuantLib::Schedule s;
            auto itUnadj = unadjustedBaseSchedules.find(x.name());
            try {
                s = makeSchedule(x.schedule());
                if (itUnadj != unadjustedBaseSchedules.end()) {
                    auto tmpUnadj = makeSchedule(x.schedule(), Null<Date>(), {}, true);
                    itUnadj->second = tmpUnadj.dates();
                }
            } catch (const std::exception& e) {
                QL_FAIL("failed building schedule '" << x.name() << "': " << e.what());
            }
            std::vector<Date> c;
            if (std::find(schedulesEligibleForCoarsening.begin(), schedulesEligibleForCoarsening.end(), x.name()) !=
                schedulesEligibleForCoarsening.end()) {

                if (itUnadj != unadjustedBaseSchedules.end()) {
                    std::tie(c, itUnadj->second) = coarsenDateGrid(s.dates(), gridCoarsening,
                        Null<Date>(), itUnadj->second);
                } else {
                    std::tie(c, std::ignore) = coarsenDateGrid(s.dates(), gridCoarsening);
                }

                if (!gridCoarsening.empty()) {
                    TLOG("apply grid coarsening rule = " << gridCoarsening << " to '" << x.name()
                                                         << "', resulting grid:")
                    for (auto const& d : c) {
                        TLOG("date " << ore::data::to_string(d));
                    }
                }
            } else {
                c = s.dates();
            }
            std::vector<ValueType> tmp;
            for (auto const& d : c)
                tmp.push_back(EventVec{nPaths, d});
            context->arrays[x.name()] = tmp;
            QL_REQUIRE(!tmp.empty(), "empty event array '" << x.name() << "' not allowed");
            builtSchedules.insert(x.name());
        } else if (x.type() == ScriptedTradeEventData::Type::Derived) {
            derivedSchedules[x.name()] = x;
        } else {
            QL_FAIL("unexpected ScriptedTradeEventData::Type");
        }
        context->constants.insert(x.name());
    }

    // Build the derived schedules, if there are any.
    if (!derivedSchedules.empty()) {
        vector<string> orderedSchedules = derivedScheduleOrder(derivedSchedules, builtSchedules);
        for (const auto& schedName : orderedSchedules) {
            const auto& evData = derivedSchedules.at(schedName);
            checkDuplicateName(context, evData.name());

            // Populate base set of dates to be shifted.
            vector<Date> anchorDates;
            const ext::optional<DateDeltaAnchor>& anchor = evData.shiftAnchor();
            if (anchor && *anchor == DateDeltaAnchor::Unadjusted) {
                auto itUnadj = unadjustedBaseSchedules.find(evData.baseSchedule());
                QL_REQUIRE(itUnadj != unadjustedBaseSchedules.end() && !itUnadj->second.empty(),
                    "makeContext: unadjusted version of base schedule '" << evData.baseSchedule() <<
                    "' not found for derived schedule '" << evData.name() << "'");
                anchorDates = itUnadj->second;
            } else {
                const auto& ctxArrs = context->arrays;
                auto itBase = ctxArrs.find(evData.baseSchedule());
                QL_REQUIRE(itBase != ctxArrs.end(), "makeContext: base schedule '" << evData.baseSchedule() <<
                    "' not found for derived schedule '" << evData.name() << "'");
                anchorDates.reserve(itBase->second.size());
                for (auto const& d : itBase->second) {
                    QL_REQUIRE(d.which() == ValueTypeWhich::Event, "makeContext: expected event in base "
                        "schedule, but got " << valueTypeLabels.at(d.which()));
                    anchorDates.push_back(boost::get<EventVec>(d).value);
                }
            }

            // Create the shifted schedule.
            Period shift;
            ext::optional<DateDeltaUnit> shiftUnit = evData.shiftUnit();
            bool suIsCalDays = shiftUnit && *shiftUnit == QuantExt::DateDeltaUnit::CalendarDays;
            vector<ValueType> thisBuiltSched;
            try {
                Calendar cal = parseCalendar(evData.calendar());
                BusinessDayConvention conv = parseBusinessDayConvention(evData.convention());
                shift = parsePeriod(evData.shift());
                if (suIsCalDays) {
                    QL_REQUIRE(shift.units() == Days, "makeContext: when making derived schedule, the shift unit "
                        "is calendar days but the shift does not have day units, it has " << shift.units() << ".");
                }

                for (auto const& d : anchorDates) {
                    if (suIsCalDays) {
                        thisBuiltSched.push_back(EventVec{ nPaths, cal.adjust(d + shift, conv) });
                    } else {
                        thisBuiltSched.push_back(EventVec{ nPaths, cal.advance(d, shift, conv) });
                    }
                }

                context->arrays[evData.name()] = thisBuiltSched;

            } catch (const std::exception& e) {
                QL_FAIL("makeContext: failed building derived schedule '" << evData.name() << "': " << e.what());
            }

            // We may want an unadjusted version of this schedule also if it is a base schedule for another 
            // derived schedule with `ShiftAnchor` set to `Unadjusted`, so we store the unadjusted version in
            // the map if needed.
            auto itUnadjThis = unadjustedBaseSchedules.find(evData.name());
            if (itUnadjThis != unadjustedBaseSchedules.end()) {
                vector<Date> unadjDatesThis;
                // If in this derived schedule, the shift period unit is days and the shift unit is business 
                // days, then it is not clear what the unadjusted version of the derived schedule should be. In 
                // this case, we log a warning and just use the possibly adjusted version above.
                if (shift.length() != 0 && shift.units() == Days && !suIsCalDays) {
                    WLOG("makeContext: cannot create an unadjusted version of the derived schedule '"
                            << evData.name() << "', using the adjusted version instead. Any derived schedule "
                            << "depending on this unadjusted version may be affected.");
                    unadjDatesThis.reserve(thisBuiltSched.size());
                    for (const auto& d : thisBuiltSched)
                        unadjDatesThis.push_back(boost::get<EventVec>(d).value);
                } else {
                    NullCalendar nullCal;
                    unadjDatesThis.reserve(anchorDates.size());
                    for (auto const& d : anchorDates) {
                        if (suIsCalDays) {
                            unadjDatesThis.push_back(d + shift);
                        } else {
                            unadjDatesThis.push_back(nullCal.advance(d, shift, Unadjusted));
                        }
                    }
                }
                itUnadjThis->second = unadjDatesThis;
            }
        }
    }

    for (auto const& x : numbers) {
        TLOG("adding number " << x.name());
        checkDuplicateName(context, x.name());
        if (!x.isArray()) {
            double d = parseReal(x.value());
            context->scalars[x.name()] = RandomVariable(nPaths, d);
        } else {
            std::vector<ValueType> tmp;
            for (auto const& d : x.values())
                tmp.push_back(RandomVariable(nPaths, parseReal(d)));
            context->arrays[x.name()] = tmp;
            QL_REQUIRE(!tmp.empty(), "empty number array '" << x.name() << "' not allowed");
        }
        context->constants.insert(x.name());
    }

    for (auto const& x : indices) {
        TLOG("adding index " << x.name());
        checkDuplicateName(context, x.name());
        if (!x.isArray()) {
            context->scalars[x.name()] = IndexVec{nPaths, x.value()};
        } else {
            std::vector<ValueType> tmp;
            for (auto const& d : x.values())
                tmp.push_back(IndexVec{nPaths, d});
            context->arrays[x.name()] = tmp;
            QL_REQUIRE(!tmp.empty(), "empty index array '" << x.name() << "' not allowed");
        }
        context->constants.insert(x.name());
    }

    for (auto const& x : currencies) {
        TLOG("adding currency " << x.name());
        checkDuplicateName(context, x.name());
        if (!x.isArray()) {
            context->scalars[x.name()] = CurrencyVec{nPaths, x.value()};
        } else {
            std::vector<ValueType> tmp;
            for (auto const& d : x.values())
                tmp.push_back(CurrencyVec{nPaths, d});
            context->arrays[x.name()] = tmp;
            QL_REQUIRE(!tmp.empty(), "empty currency array '" << x.name() << "' not allowed");
        }
        context->constants.insert(x.name());
    }

    for (auto const& x : daycounters) {
        TLOG("adding daycounter " << x.name());
        checkDuplicateName(context, x.name());
        if (!x.isArray()) {
            context->scalars[x.name()] = DaycounterVec{nPaths, x.value()};
        } else {
            std::vector<ValueType> tmp;
            for (auto const& d : x.values())
                tmp.push_back(DaycounterVec{nPaths, d});
            context->arrays[x.name()] = tmp;
            QL_REQUIRE(!tmp.empty(), "empty currency array '" << x.name() << "' not allowed");
        }
        context->constants.insert(x.name());
    }

    DLOG("context built with " << context->scalars.size() << " scalars and " << context->arrays.size() << " arrays.");
    return context;
}

void addNewSchedulesToContext(QuantLib::ext::shared_ptr<Context> context,
                              const std::vector<ScriptedTradeScriptData::NewScheduleData>& newSchedules) {
    for (auto const& x : newSchedules) {
        DLOG("adding new schedule " << x.name());
        checkDuplicateName(context, x.name());
        std::vector<std::vector<ValueType>> sources;
        for (auto const& s : x.sourceSchedules()) {
            auto d = context->arrays.find(s);
            QL_REQUIRE(d != context->arrays.end(),
                       "ScriptedTradeGenericEngineBuilder::engineBuilder(): did not find source schedule '"
                           << s << "' when building new schedule '" << x.name() << "'");
            sources.push_back(d->second);
        }
        std::vector<ValueType> result;
        if (x.operation() == "Join") {
            std::set<QuantLib::Date> tmp;
            Size n = 0;
            for (auto const& s : sources) {
                for (auto const& d : s) {
                    tmp.insert(boost::get<EventVec>(d).value);
                    n = boost::get<EventVec>(d).size;
                }
            }
            for (auto const& d : tmp) {
                result.push_back(EventVec{n, d});
            }
            context->arrays[x.name()] = result;
            context->constants.insert(x.name());
        } else {
            QL_FAIL("new schedule operation '" << x.operation() << "' not supported");
        }
    }
}

namespace {

struct SizeSetter : public boost::static_visitor<void> {
    explicit SizeSetter(const Size newSize) : newSize_(newSize) {}
    void operator()(RandomVariable& v) const {
        QL_REQUIRE(v.deterministic(), "can only change size of determinstic random variables");
        v = RandomVariable(newSize_, v.at(0));
    }
    void operator()(Filter& v) const {
        QL_REQUIRE(v.deterministic(), "can only change size of determinstic filters");
        v = Filter(newSize_, v.at(0));
    }
    void operator()(EventVec& c) const { c.size = newSize_; }
    void operator()(CurrencyVec& c) const { c.size = newSize_; }
    void operator()(IndexVec& c) const { c.size = newSize_; }
    void operator()(DaycounterVec& c) const { c.size = newSize_; }

private:
    Size newSize_;
};

} // namespace

void amendContextVariablesSizes(QuantLib::ext::shared_ptr<Context> context, const Size newSize) {
    SizeSetter setter(newSize);
    for (auto& x : context->scalars)
        boost::apply_visitor(setter, x.second);
    for (auto& v : context->arrays)
        for (auto& x : v.second)
            boost::apply_visitor(setter, x);
}

std::string scriptedIndexName(const QuantLib::ext::shared_ptr<Underlying>& underlying) {
    if (underlying->type() == "Equity") {
        return "EQ-" + underlying->name();
    } else if (underlying->type() == "FX") {
        return "FX-" + underlying->name();
    } else if (underlying->type() == "Commodity") {
        QuantLib::ext::shared_ptr<CommodityUnderlying> comUnderlying =
            QuantLib::ext::dynamic_pointer_cast<CommodityUnderlying>(underlying);
        std::string tmp = "COMM-" + comUnderlying->name();
        if (comUnderlying->priceType().empty() || comUnderlying->priceType() == "Spot") {
            return tmp;
        } else if (comUnderlying->priceType() == "FutureSettlement") {
            tmp += "#" + std::to_string(comUnderlying->futureMonthOffset() == Null<Size>()
                                            ? 0
                                            : comUnderlying->futureMonthOffset());
            if (comUnderlying->deliveryRollDays() != Null<Size>()) {
                tmp += "#" + std::to_string(comUnderlying->deliveryRollDays());
                if (!comUnderlying->deliveryRollCalendar().empty()) {
                    tmp += "#" + comUnderlying->deliveryRollCalendar();
                }
            }
            return tmp;
        } else {
            QL_FAIL("underlying price type '" << comUnderlying->priceType() << "' for commodity underlying '"
                                              << comUnderlying->name() << "' not handled.");
        }
    } else if (underlying->type() == "InterestRate") {
        return underlying->name();
    } else if (underlying->type() == "Inflation") {
        QuantLib::ext::shared_ptr<InflationUnderlying> infUnderlying =
            QuantLib::ext::dynamic_pointer_cast<InflationUnderlying>(underlying);
        if (infUnderlying->interpolation() == QuantLib::CPI::InterpolationType::Linear)
            return underlying->name() + "#L";
        else if (infUnderlying->interpolation() == QuantLib::CPI::InterpolationType::Flat)
            return underlying->name() + "#F";
        else {
            QL_FAIL("observation interpolation " << infUnderlying->interpolation()
                                                 << " not covered in scripted inflation indexes");
        }
    } else if (underlying->type() == "Basic") {
        return underlying->name();
    } else {
        QL_FAIL("underlying type '" << underlying->type() << "' not handled.");
    }
}

std::map<std::string, std::vector<Real>>
getCalibrationStrikes(const std::vector<ScriptedTradeScriptData::CalibrationData>& calibrationSpec,
                      const QuantLib::ext::shared_ptr<Context>& context) {
    std::map<std::string, std::vector<Real>> result;
    for (auto const& c : calibrationSpec) {

        std::vector<std::string> indexNames;
        // set up index
        auto index = context->scalars.find(c.index());
        if (index != context->scalars.end()) {
            QL_REQUIRE(index->second.which() == ValueTypeWhich::Index,
                "calibration index variable '" << c.index() << "' must evaluate to an index");
            std::string indexName = boost::get<IndexVec>(index->second).value;
            // replace fixing source tag in FX indices by GENERIC, since this is what is passed to the model
            // TODO FX indices might be reorganised vs. a base ccy != their original target ccy, is there anything
            // we can do to get an effective calibration at the specified deal strike?
            IndexInfo info(indexName);
            if (info.isFx())
                indexName =
                    "FX-GENERIC-" + info.fx()->sourceCurrency().code() + "-" + info.fx()->targetCurrency().code();            
            indexNames.push_back(indexName);
        } else {
            auto indexes = context->arrays.find(c.index());
            if (indexes != context->arrays.end()) {
                for (Size i = 0; i < indexes->second.size(); ++i) {
                    QL_REQUIRE(indexes->second[i].which() == ValueTypeWhich::Index,
                               "calibration strike variable '" << c.index() << "[" << i
                                                               << "]' must evaluate to an index");
                    auto indexName = boost::get<IndexVec>(indexes->second[i]).value;
                    IndexInfo info(indexName);
                    if (info.isFx())
                        indexName = "FX-GENERIC-" + info.fx()->sourceCurrency().code() + "-" +
                                    info.fx()->targetCurrency().code();
                    indexNames.push_back(indexName);
                }
            } else
                QL_FAIL("did not find calibration index variable '" << c.index()
                    << "' (as scalar or array) in context");
        }
        
        
        // loop over calibration strikes for index
        for (auto const& strikeStr : c.strikes()) {
            auto strike = context->scalars.find(strikeStr);
            if (strike != context->scalars.end()) {
                QL_REQUIRE(strike->second.which() == ValueTypeWhich::Number,
                            "calibration strike variable '" << strikeStr << "' must evaluate to a number");
                auto strikeNum = boost::get<RandomVariable>(strike->second);
                QL_REQUIRE(strikeNum.deterministic(), "calibration strike variable '"
                                                            << strikeStr << "' must be deterministic, got "
                                                            << strikeNum);
                QL_REQUIRE(indexNames.size() == 1, "Can only have one index if one strike provided");
                result[indexNames.at(0)].push_back(strikeNum.at(0));
                DLOG("add calibration strike for index '" << indexNames.at(0) << "': " << strikeNum.at(0));
            } else {
                auto strikeVec = context->arrays.find(strikeStr);
                if (strikeVec != context->arrays.end()) {
                    QL_REQUIRE(strikeVec->second.size() % indexNames.size() == 0, 
                        "StrikeVec must contain the same number of strikes for each index");
                    auto strikeSize = strikeVec->second.size() / indexNames.size();
                    Size ind = 0;
                    for (Size j = 0; j < indexNames.size(); j++) {
                        for (Size i = 0; i < strikeSize; ++i) {
                            QL_REQUIRE(strikeVec->second[ind].which() == ValueTypeWhich::Number,
                                        "calibration strike variable '" << strikeStr << "[" << i
                                                                        << "]' must evaluate to a number");
                            auto strikeNum = boost::get<RandomVariable>(strikeVec->second[ind]);
                            QL_REQUIRE(strikeNum.deterministic(), "calibration strike variable '"
                                                                    "calibration strike variable '"
                                                                        << strikeStr << "[" << i
                                                                        << "]' must be deterministic, got "
                                                                        << strikeNum);
                            result[indexNames[j]].push_back(strikeNum.at(0));
                            DLOG("add calibration strike for index '" << indexNames[j] << "' from : '" << strikeStr
                                                                        << "[" << i << "]'  " << strikeNum.at(0));
                            ind++;
                        }
                    }
                } else {
                    WLOG("getCalibrationStrikes: did not find calibration strike variable '" << strikeStr
                        << "' (as scalar or array) in context forcalibration index variable '" << c.index());
                }
            }
        }
    }
    return result;
}

} // namespace data
} // namespace ore
