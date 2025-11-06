/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_parsers_i
#define ored_parsers_i

%include indexes.i
%include inflation.i
%include termstructures.i
%include qle_indexes.i
%include ored_conventions.i
%include std_string.i
%include std_map.i

%{
using std::string;
using std::map;

using ore::data::CalendarParser;

using QuantLib::YieldTermStructure;
using QuantLib::ZeroInflationTermStructure;
using QuantLib::Handle;
using QuantLib::Index;
using QuantLib::Option;
using QuantLib::Exercise;
using QuantLib::Settlement;
using QuantLib::Position;
using QuantLib::Compounding;
using QuantLib::Frequency;
using QuantLib::DateGeneration;
using QuantLib::Currency;
using QuantLib::DayCounter;
using QuantLib::BusinessDayConvention;
using QuantLib::Period;
using QuantLib::Calendar;
using QuantLib::Date;

using ore::data::Conventions;
using ore::data::IRSwapConvention;

using ore::data::parseIborIndex;
using ore::data::parseSwapIndex;
using ore::data::parseZeroInflationIndex;
using ore::data::parseFxIndex;
using ore::data::parseIndex;
using ore::data::parseCalendar;
using ore::data::parsePeriod;
using ore::data::parseOptionType;
using ore::data::parseExerciseType;
using ore::data::parseSettlementType;
using ore::data::parsePositionType;
using ore::data::parseCompounding;
using ore::data::parseFrequency;
using ore::data::parseDateGenerationRule;
using ore::data::parseCurrency;
using ore::data::parseDayCounter;
using ore::data::parseBusinessDayConvention;
using ore::data::parseDate;
using ore::data::fxDominance;
using ore::data::isGenericIborIndex;
%}
bool isGenericIborIndex(const std::string& indexName);
ext::shared_ptr<IborIndex> parseIborIndex(const std::string& s,
                                          const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>());
    
ext::shared_ptr<SwapIndex> parseSwapIndex(const std::string& s,
                                          const Handle<YieldTermStructure>& forwarding = Handle<YieldTermStructure>(),
                                          const Handle<YieldTermStructure>& discounting = Handle<YieldTermStructure>());

ext::shared_ptr<Index> parseIndex(const std::string& s);
    
ext::shared_ptr<ZeroInflationIndex> parseZeroInflationIndex(const std::string& s,
                                                            const Handle<ZeroInflationTermStructure>& h = Handle<ZeroInflationTermStructure>());
    
ext::shared_ptr<FxIndex> parseFxIndex(const std::string& s);

QuantLib::Calendar parseCalendar(const std::string& s);
QuantLib::Period parsePeriod(const std::string& s);
QuantLib::BusinessDayConvention parseBusinessDayConvention(const std::string& s);
QuantLib::DayCounter parseDayCounter(const std::string& s);
QuantLib::Currency parseCurrency(const std::string& s);
QuantLib::DateGeneration::Rule parseDateGenerationRule(const std::string& s);
QuantLib::Frequency parseFrequency(const std::string& s);
QuantLib::Compounding parseCompounding(const std::string& s);
QuantLib::Position::Type parsePositionType(const std::string& s);
QuantLib::Settlement::Type parseSettlementType(const std::string& s);
QuantLib::Exercise::Type parseExerciseType(const std::string& s);
QuantLib::Option::Type parseOptionType(const std::string& s);
QuantLib::Date parseDate(const std::string& s);

%template(StringCalMap) std::map<std::string, Calendar>;

%shared_ptr(CalendarParser)
class CalendarParser {
public:
    CalendarParser();
    QuantLib::Calendar parseCalendar(const std::string& name) const;
    QuantLib::Calendar addCalendar(const std::string baseName, std::string& newName);
    void reset();
    void resetAddedAndRemovedHolidays();
    const std::map<std::string, QuantLib::Calendar> getCalendars() const;
};

std::string fxDominance(const std::string& s1, const std::string& s2);

#endif
