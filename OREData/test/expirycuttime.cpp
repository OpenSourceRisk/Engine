/*
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

#include <boost/test/unit_test.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/toplevelfixture.hpp>
#include <ql/settings.hpp>

#include <string>

using namespace QuantLib;
using namespace ore::data;
using std::string;

namespace {

const string singleDate = "<ExerciseDates><ExerciseDate>2016-02-05</ExerciseDate></ExerciseDates>";

string optionData(const string& exercise) {
    return "<OptionData><LongShort>Long</LongShort><OptionType>Call</OptionType><Style>European</Style>"
           "<Settlement>Cash</Settlement><PayOffAtExpiry>false</PayOffAtExpiry>" +
           exercise + "</OptionData>";
}

string envelope(const string& cut) {
    return "<Envelope><CounterParty>CPTY_A</CounterParty><NettingSetId>CPTY_A</NettingSetId><AdditionalFields>" +
           (cut.empty() ? string() : "<ExpiryCutTime>" + cut + "</ExpiryCutTime>") + "</AdditionalFields></Envelope>";
}

string equityOptionXml(const string& cut, const string& exercise = singleDate) {
    return "<Trade id=\"EQ\"><TradeType>EquityOption</TradeType>" + envelope(cut) + "<EquityOptionData>" +
           optionData(exercise) +
           "<Name>RIC:.SPX</Name><Currency>USD</Currency><Strike>100</Strike><Quantity>1</Quantity>"
           "</EquityOptionData></Trade>";
}

string fxOptionXml(const string& cut, const string& exercise = singleDate) {
    return "<Trade id=\"FX\"><TradeType>FxOption</TradeType>" + envelope(cut) + "<FxOptionData>" +
           optionData(exercise) +
           "<BoughtCurrency>EUR</BoughtCurrency><BoughtAmount>1</BoughtAmount>"
           "<SoldCurrency>USD</SoldCurrency><SoldAmount>1.1</SoldAmount></FxOptionData></Trade>";
}

template <class T> T parse(const string& xml) {
    T trade;
    trade.fromXMLString(xml);
    return trade;
}

#ifdef QL_HIGH_RESOLUTION_DATE
bool mentions(const Error& e, const string& what) { return string(e.what()).find(what) != string::npos; }
#endif

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::data::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ExpiryCutTimeTests)

#ifdef QL_HIGH_RESOLUTION_DATE

BOOST_AUTO_TEST_CASE(testCutResolvesExerciseDate) {
    const Date expected(5, Feb, 2016, 21, 0, 0);
    BOOST_CHECK_EQUAL(parseDate(parse<EquityOption>(equityOptionXml("16:00-05:00")).option().exerciseDates()[0]),
                      expected);
    BOOST_CHECK_EQUAL(parseDate(parse<FxOption>(fxOptionXml("16:00-05:00")).option().exerciseDates()[0]), expected);
    BOOST_CHECK_EQUAL(parseDate(parse<FxOption>(fxOptionXml("21:00")).option().exerciseDates()[0]), expected);
}

BOOST_AUTO_TEST_CASE(testNoCutDefaultsOnEvaluationDay) {
    const string twoDates = "<ExerciseDates><ExerciseDate>2016-02-05</ExerciseDate>"
                            "<ExerciseDate>2018-02-05</ExerciseDate></ExerciseDates>";
    auto eq = parse<EquityOption>(equityOptionXml("", twoDates));
    auto fx = parse<FxOption>(fxOptionXml("", twoDates));
    Settings::instance().evaluationDate() = Date(5, Feb, 2016, 15, 0, 0);
    for (const auto& dates : {eq.option().exerciseDates(), fx.option().exerciseDates()}) {
        BOOST_CHECK_EQUAL(parseDate(dates[0]), Date(5, Feb, 2016, 23, 59, 59));
        BOOST_CHECK_EQUAL(dates[1], "2018-02-05");
    }

    Settings::instance().evaluationDate() = Date(4, Feb, 2016);
    BOOST_CHECK_EQUAL(eq.option().exerciseDates()[0], "2016-02-05");
}

BOOST_AUTO_TEST_CASE(testDatetimeExerciseDateUnchanged) {
    Settings::instance().evaluationDate() = Date(5, Feb, 2016, 15, 0, 0);
    auto eq = parse<EquityOption>(
        equityOptionXml("", "<ExerciseDates><ExerciseDate>2016-02-05T16:00Z</ExerciseDate></ExerciseDates>"));
    BOOST_CHECK_EQUAL(eq.option().exerciseDates()[0], "2016-02-05T16:00Z");
}

BOOST_AUTO_TEST_CASE(testInvalidCutThrows) {
    BOOST_CHECK_EXCEPTION(parse<EquityOption>(equityOptionXml("4pm")), Error,
                          [](const Error& e) { return mentions(e, "ExpiryCutTime"); });
    BOOST_CHECK_EXCEPTION(parse<FxOption>(fxOptionXml("25:00Z")), Error,
                          [](const Error& e) { return mentions(e, "ExpiryCutTime"); });
    const string schedule =
        "<ExerciseSchedule><Dates><Dates><Date>2016-02-05</Date></Dates></Dates></ExerciseSchedule>";
    BOOST_CHECK_EXCEPTION(parse<EquityOption>(equityOptionXml("21:00Z", schedule)), Error,
                          [](const Error& e) { return mentions(e, "ExerciseSchedule"); });
}

#else

BOOST_AUTO_TEST_CASE(testCutIgnoredInRegularBuild) {
    BOOST_CHECK_EQUAL(parse<EquityOption>(equityOptionXml("16:00-05:00")).option().exerciseDates()[0], "2016-02-05");
    BOOST_CHECK_EQUAL(parse<FxOption>(fxOptionXml("16:00-05:00")).option().exerciseDates()[0], "2016-02-05");
    BOOST_CHECK_NO_THROW(parse<EquityOption>(equityOptionXml("4pm")));
}

#endif

BOOST_AUTO_TEST_CASE(testRoundTripKeepsCutInEnvelope) {
    auto eq = parse<EquityOption>(equityOptionXml("16:00-05:00"));
    eq.id() = "EQ";
    const string xml = eq.toXMLString();
    BOOST_CHECK(xml.find("<ExpiryCutTime>16:00-05:00</ExpiryCutTime>") != string::npos);
    BOOST_CHECK(xml.find("<ExerciseDate>2016-02-05</ExerciseDate>") != string::npos);
    auto reparsed = parse<EquityOption>(xml);
    BOOST_CHECK_EQUAL(reparsed.envelope().additionalField("ExpiryCutTime"), "16:00-05:00");
    BOOST_CHECK_EQUAL(reparsed.option().exerciseDates()[0], eq.option().exerciseDates()[0]);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
