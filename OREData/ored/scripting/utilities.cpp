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

#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/models/marketmodels/browniangenerators/mtbrowniangenerator.hpp>
#include <ql/settings.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

#include <boost/algorithm/string.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using namespace QuantExt;
    
std::vector<Date> coarsenDateGrid(const std::vector<Date>& dates, const std::string& rule, const Date& referenceDate) {

    // if rule is empty return original grid

    if (rule.empty())
        return dates;

    // get ref date and prepare result vector

    Date refDate = referenceDate == Null<Date>() ? Settings::instance().evaluationDate() : referenceDate;

    std::vector<Date> result;

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

    // keep all dates <= refDate

    auto d = dates.begin();
    for (; d != dates.end() && *d <= refDate; ++d) {
        result.push_back(*d);
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

            if (!candidates.empty())
                result.push_back(candidates.back());
        } while (start < end);

        start = end;
    }

    return result;
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

std::pair<std::string, Period> convertIndexToCamCorrelationEntry(const std::string& i) {
    IndexInfo info(i);
    if (info.isIr()) {
        return std::make_pair("IR#" + info.ir()->currency().code(), info.ir()->tenor());
    } else if (info.isInf()) {
        return std::make_pair("INF#" + info.infName(), 0 * Days);
    } else if (info.isFx()) {
        return std::make_pair("FX#" + info.fx()->sourceCurrency().code() + info.fx()->targetCurrency().code(),
                              0 * Days);
    } else if (info.isEq()) {
        return std::make_pair("EQ#" + info.eq()->name(), 0 * Days);
    } else if (info.isComm()) {
        return std::make_pair("COM#" + info.commName(), 0 * Days);
    } else {
        QL_FAIL("convertIndextoCamCorrelationEntry(): index '" << i << "' not recognised");
    }
}

void checkDuplicateName(const QuantLib::ext::shared_ptr<Context> context, const std::string& name) {
    auto scalar = context->scalars.find(name);
    auto array = context->arrays.find(name);
    QL_REQUIRE(scalar == context->scalars.end() && array == context->arrays.end(),
               "variable '" << name << "' already declared.");
}

QuantLib::ext::shared_ptr<Context> makeContext(Size nPaths, const std::string& gridCoarsening,
                                       const std::vector<std::string>& schedulesEligibleForCoarsening,
                                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                       const std::vector<ScriptedTradeEventData>& events,
                                       const std::vector<ScriptedTradeValueTypeData>& numbers,
                                       const std::vector<ScriptedTradeValueTypeData>& indices,
                                       const std::vector<ScriptedTradeValueTypeData>& currencies,
                                       const std::vector<ScriptedTradeValueTypeData>& daycounters) {

    TLOG("make context");

    auto context = QuantLib::ext::make_shared<Context>();


    map<string, ScriptedTradeEventData> derivedSchedules;
    for (auto const& x : events) {
        TLOG("adding event " << x.name());
        if (x.type() == ScriptedTradeEventData::Type::Value) {
            checkDuplicateName(context, x.name());
            Date d = parseDate(x.value());
            context->scalars[x.name()] = EventVec{nPaths, d};
        } else if (x.type() == ScriptedTradeEventData::Type::Array) {
            checkDuplicateName(context, x.name());
            QuantLib::Schedule s;
            try {
                s = makeSchedule(x.schedule());
            } catch (const std::exception& e) {
                QL_FAIL("failed building schedule '" << x.name() << "': " << e.what());
            }
            std::vector<Date> c;
            if (std::find(schedulesEligibleForCoarsening.begin(), schedulesEligibleForCoarsening.end(), x.name()) !=
                schedulesEligibleForCoarsening.end()) {
                c = coarsenDateGrid(s.dates(), gridCoarsening);
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
        } else if (x.type() == ScriptedTradeEventData::Type::Derived) {
            derivedSchedules[x.name()] = x;
        } else {
            QL_FAIL("unexpected ScriptedTradeEventData::Type");
        }
        context->constants.insert(x.name());
    }

    bool calculated;
    while (derivedSchedules.size() > 0) {
        calculated = false;
        for (auto& ds : derivedSchedules) {
            auto base = context->arrays.find(ds.second.baseSchedule());
            checkDuplicateName(context, ds.second.name());
            if (base != context->arrays.end()) {
                try {
                    Calendar cal = parseCalendar(ds.second.calendar());
                    BusinessDayConvention conv = parseBusinessDayConvention(ds.second.convention());
                    Period shift = parsePeriod(ds.second.shift());
                    std::vector<ValueType> tmp;
                    for (auto const& d : base->second) {
                        QL_REQUIRE(d.which() == ValueTypeWhich::Event,
                                   "expected event in base schedule, got " << valueTypeLabels.at(d.which()));
                        EventVec e = QuantLib::ext::get<EventVec>(d);
                        tmp.push_back(EventVec{nPaths, cal.advance(e.value, shift, conv)});
                    }
                    context->arrays[ds.second.name()] = tmp;
                    derivedSchedules.erase(ds.second.name());
                    calculated = true;
                } catch (const std::exception& e) {
                    QL_FAIL("failed building derived schedule '" << ds.second.name() << "': " << e.what());
                }
                break;
            }
        }

        // If, after looping through the full list of derived schedules, we are unable to build any of them.
        if (!calculated) {
            for (const auto& ds : derivedSchedules) {
                ALOG("Failed to build the derived schedule: " << ds.first);
            }
            QL_FAIL("Failed to build at least one derived schedule");
            break;
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
                    tmp.insert(QuantLib::ext::get<EventVec>(d).value);
                    n = QuantLib::ext::get<EventVec>(d).size;
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

IndexInfo::IndexInfo(const std::string& name, const QuantLib::ext::shared_ptr<Market>& market) : name_(name), market_(market) {
    isFx_ = isEq_ = isComm_ = isIr_ = isInf_ = isIrIbor_ = isIrSwap_ = isGeneric_ = false;
    bool done = false;
    // first handle the index types that we can recognise by a prefix
    if (boost::starts_with(name, "COMM-")) {
        // index will be created on the fly, since it depends on the obsDate in general
        isComm_ = done = true;
        std::vector<std::string> tokens;
        boost::split(tokens, name, boost::is_any_of("#!"));
        QL_REQUIRE(!tokens.empty(), "IndexInfo: no commodity name found for '" << name << "'");
        commName_ = parseCommodityIndex(tokens.front())->underlyingName();
    } else if (boost::starts_with(name, "FX-")) {
        // parse fx index using conventions
        fx_ = parseFxIndex(name, Handle<Quote>(), Handle<YieldTermStructure>(), Handle<YieldTermStructure>(), true);
        isFx_ = done = true;
    } else if (boost::starts_with(name, "EQ-")) {
        eq_ = parseEquityIndex(name);
        if (market_ != nullptr) {
            try {
                eq_ = *market_->equityCurve(eq_->name());
            } catch (...) {
            }
        }
        isEq_ = done = true;
    } else if (boost::starts_with(name, "GENERIC-")) {
        generic_ = parseGenericIndex(name);
        isGeneric_ = done = true;
    }
    // no easy way to see if it is an Ibor index, so try and error
    if (!done) {
        try {
            irIbor_ = parseIborIndex(name);
            ir_ = irIbor_;
            isIr_ = isIrIbor_ = done = true;
        } catch (...) {
        }
    }
    // same for swap
    if (!done) {
        try {
            irSwap_ = parseSwapIndex(name, Handle<YieldTermStructure>(), Handle<YieldTermStructure>());
            ir_ = irSwap_;
            isIr_ = isIrSwap_ = done = true;
        } catch (...) {
        }
    }
    // and same for inflation
    if (!done) {
        try {
            auto res = parseScriptedInflationIndex(name);
            inf_ = res.first;
            infName_ = res.second;
            isInf_ = done = true;
        } catch (...) {
        }
    }
    QL_REQUIRE(done, "Could not build index info for '"
                         << name
                         << "', expected a valid COMM, FX, EQ, GENERIC, Ibor, Swap, Inflation index identifier.");
}

QuantLib::ext::shared_ptr<Index> IndexInfo::index(const Date& obsDate) const {
    if (isFx_)
        return fx_;
    else if (isEq_)
        return eq_;
    else if (isIr_)
        return ir_;
    else if (isInf_)
        return inf_;
    else if (isGeneric_)
        return generic_;
    else if (isComm_) {
        return comm(obsDate);
    } else {
        QL_FAIL("could not parse index '" << name_ << "'");
    }
}

QuantLib::ext::shared_ptr<CommodityIndex> IndexInfo::comm(const Date& obsDate) const {
    if (isComm())
        return parseScriptedCommodityIndex(name(), obsDate);
    else
        return nullptr;
}

std::string IndexInfo::commName() const {
    QL_REQUIRE(isComm(), "IndexInfo::commName(): commodity index required, got " << *this);
    return commName_;
}

std::string IndexInfo::infName() const {
    QL_REQUIRE(isInf(), "IndexInfo::infName(): inflation index required, got " << *this);
    return infName_;
}

std::ostream& operator<<(std::ostream& o, const IndexInfo& i) {
    o << "index '" << i.name() << "'";
    if (i.isFx())
        o << ", type FX, index name '" << i.fx()->name() << "'";
    if (i.isEq())
        o << ", type EQ, index name '" << i.eq()->name() << "'";
    if (i.isComm())
        o << ", type COMM";
    if (i.isIrIbor())
        o << ", type IR Ibor, index name '" << i.irIbor()->name() << "'";
    if (i.isIrSwap())
        o << ", type IR Swap, index name '" << i.irSwap()->name() << "'";
    if (i.isGeneric())
        o << ", type Generic, index name '" << i.generic()->name() << "'";
    return o;
}

QuantLib::ext::shared_ptr<FallbackIborIndex> IndexInfo::irIborFallback(const IborFallbackConfig& iborFallbackConfig,
                                                               const Date& asof) const {
    if (isIrIbor_ && iborFallbackConfig.isIndexReplaced(name_, asof)) {
        auto data = iborFallbackConfig.fallbackData(name_);
        // we don't support convention based rfr fallback indices, with ore ticket 1758 this might change
        auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(data.rfrIndex));
        QL_REQUIRE(on, "IndexInfo::irIborFallback(): could not cast rfr index '"
                           << data.rfrIndex << "' for ibor fallback index '" << name_ << "' to an overnight index");
        return QuantLib::ext::make_shared<FallbackIborIndex>(irIbor_, on, data.spread, data.switchDate, false);
    }
    return nullptr;
}

QuantLib::ext::shared_ptr<FallbackOvernightIndex> IndexInfo::irOvernightFallback(const IborFallbackConfig& iborFallbackConfig,
									 const Date& asof) const {
    if (isIrIbor_ && iborFallbackConfig.isIndexReplaced(name_, asof)) {
        auto data = iborFallbackConfig.fallbackData(name_);
        // we don't support convention based rfr fallback indices, with ore ticket 1758 this might change
        auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(data.rfrIndex));
        QL_REQUIRE(on, "IndexInfo::irIborFallback(): could not cast rfr index '"
                           << data.rfrIndex << "' for ibor fallback index '" << name_ << "' to an overnight index");
        if (auto original = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(irIbor_))
	    return QuantLib::ext::make_shared<FallbackOvernightIndex>(original, on, data.spread, data.switchDate, false);
	else
	    return nullptr;
    }
    return nullptr;
}

QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> parseScriptedCommodityIndex(const std::string& indexName,
                                                                        const QuantLib::Date& obsDate) {

    QL_REQUIRE(!indexName.empty(), "parseScriptedCommodityIndex(): empty index name");
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    std::vector<std::string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("#!"));
    std::string plainIndexName = tokens.front();
    std::string commName = parseCommodityIndex(plainIndexName)->underlyingName();

    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention;
    if (conventions->has(commName)) {
        convention = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(commName));
    }
    Calendar fixingCalendar = convention ? convention->calendar() : NullCalendar();

    std::vector<std::string> tokens1;
    boost::split(tokens1, indexName, boost::is_any_of("#"));
    std::vector<std::string> tokens2;
    boost::split(tokens2, indexName, boost::is_any_of("!"));

    QuantLib::ext::shared_ptr<CommodityIndex> res;
    if (tokens1.size() >= 2) {
        // handle form 3)- 5), i.e. COMM-name#N#D#Cal, COMM-name#N#D, COMM-name#N
        QL_REQUIRE(tokens.size() <= 4,
                   "parseScriptedCommodityIndex(): expected COMM-Name#N, Comm-Name#N#D, Comm-Name#N#D#Cal, got '"
                       << indexName << "'");
        QL_REQUIRE(convention,
                   "parseScriptedCommodityIndex(): commodity future convention required for '" << indexName << "'");
        QL_REQUIRE(obsDate != Date(), "parseScriptedCommodityIndex(): obsDate required for '" << indexName << "'");
        int offset = std::stoi(tokens[1]);
        int deliveryRollDays = 0;
        if (tokens.size() >= 3)
            deliveryRollDays = parseInteger(tokens[2]);
        Calendar rollCal = tokens.size() == 4 ? parseCalendar(tokens[3]) : fixingCalendar;
        ConventionsBasedFutureExpiry expiryCalculator(*convention);
        Date adjustedObsDate = deliveryRollDays != 0 ? rollCal.advance(obsDate, deliveryRollDays * Days) : obsDate;
        res = parseCommodityIndex(commName, false, Handle<PriceTermStructure>(), fixingCalendar, true);
        res = res->clone(expiryCalculator.nextExpiry(true, adjustedObsDate, offset));
    } else if (tokens2.size() >= 2) {
        // handle form 6), i.e. COMM-name!N
        QL_REQUIRE(tokens.size() <= 2,
                   "parseScriptedCommodityIndex(): expected COMM-Name!N, got '" << indexName << "'");
        QL_REQUIRE(convention,
                   "parseScriptedCommodityIndex(): commodity future convention required for '" << indexName << "'");
        QL_REQUIRE(obsDate != Date(), "parseScriptedCommodityIndex(): obsDate required for '" << indexName << "'");
        int offset = std::stoi(tokens[1]);
        ConventionsBasedFutureExpiry expiryCalculator(*convention);
        res = parseCommodityIndex(commName, false, Handle<PriceTermStructure>(), fixingCalendar, true);
        res = res->clone(expiryCalculator.expiryDate(obsDate, offset));
    } else {
        // handle 0), 1) and 2)
        res = parseCommodityIndex(indexName, true, QuantLib::Handle<QuantExt::PriceTermStructure>(), fixingCalendar,
                                  false);
    }

    TLOG("parseScriptCommodityIndex(" << indexName << "," << QuantLib::io::iso_date(obsDate) << ") = " << res->name());
    return res;
}

QL_DEPRECATED_DISABLE_WARNING
// Remove in the next release, interpolation has to happen in the coupon (script) and not in the index
std::pair<QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>, std::string>
parseScriptedInflationIndex(const std::string& indexName) {
    QL_REQUIRE(!indexName.empty(), "parseScriptedInflationIndex(): empty index name");
    std::vector<std::string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("#"));
    std::string plainIndexName = tokens.front();
    bool interpolated = false;
    if (tokens.size() == 1) {
        interpolated = false;
    } else if (tokens.size() == 2) {
        QL_REQUIRE(tokens[1] == "F" || tokens[1] == "L", "parseScriptedInflationIndex(): expected ...#[L|F], got ...#"
                                                             << tokens[1] << " in '" << indexName << "'");
        interpolated = tokens[1] == "L";
    } else {
        QL_FAIL("parseScriptedInflationIndex(): expected IndexName or IndexName#[F|L], got '" << indexName << "'");
    }
    return std::make_pair(
        parseZeroInflationIndex(plainIndexName, interpolated, Handle<ZeroInflationTermStructure>()),
        plainIndexName);
}
QL_DEPRECATED_ENABLE_WARNING

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

Size getInflationSimulationLag(const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index) {
    // this is consistent with the lag computation in CrossAssetModel::infDki()
    Date d1 = index->zeroInflationTermStructure()->baseDate();
    Date d2 = index->zeroInflationTermStructure()->referenceDate();
    QL_DEPRECATED_DISABLE_WARNING
    if (!index->interpolated()) {
        d2 = inflationPeriod(d2, index->frequency()).first;
    }
    QL_DEPRECATED_ENABLE_WARNING
    return d2 - d1;
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
            std::string indexName = QuantLib::ext::get<IndexVec>(index->second).value;
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
                    auto indexName = QuantLib::ext::get<IndexVec>(indexes->second[i]).value;
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
                auto strikeNum = QuantLib::ext::get<RandomVariable>(strike->second);
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
                            auto strikeNum = QuantLib::ext::get<RandomVariable>(strikeVec->second[ind]);
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
