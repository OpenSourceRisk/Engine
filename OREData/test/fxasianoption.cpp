/*
  Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
 #include <oret/toplevelfixture.hpp>

 #include <boost/make_shared.hpp>

 #include <ql/currencies/america.hpp>
 #include <ql/instruments/asianoption.hpp>
 #include <ql/math/interpolations/linearinterpolation.hpp>
 #include <ql/termstructures/yield/flatforward.hpp>
 #include <ql/time/daycounters/actual360.hpp>

 #include <ored/marketdata/marketimpl.hpp>
 #include <ored/portfolio/builders/fxasianoption.hpp>
 #include <ored/portfolio/portfolio.hpp>
 #include <ored/portfolio/asianoption.hpp>
 #include <ored/utilities/to_string.hpp>

 using namespace std;
 using namespace boost::unit_test_framework;
 using namespace boost::algorithm;
 using namespace QuantLib;
 using namespace QuantExt;
 using namespace ore::data;

 namespace {

 class TestMarket : public MarketImpl {
 public:
     TestMarket(const Real spot, const Date& expiry, const Rate domRate, const Rate forRate,
                const Volatility flatVolatility)
         : MarketImpl(false) {
         // Reference date and common day counter
         asof_ = Date(01, Feb, 2021);
         DayCounter dayCounter = Actual360();

         // Add USD/JPY discount curves
         Handle<YieldTermStructure> domestic(QuantLib::ext::make_shared<FlatForward>(asof_, domRate, dayCounter));
         Handle<YieldTermStructure> foreign(QuantLib::ext::make_shared<FlatForward>(asof_, forRate, dayCounter));
         yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "USD")] = domestic;
         yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "JPY")] = foreign;

         // Add fx spot
	 std::map<std::string, Handle<Quote>> quotes;
	 quotes["JPYUSD"] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot));
	 fx_ = QuantLib::ext::make_shared<FXTriangulation>(quotes);

         // Add USDJPY volatilities
         Handle<BlackVolTermStructure> volatility(
             QuantLib::ext::make_shared<BlackConstantVol>(asof_, TARGET(), flatVolatility, dayCounter));
         fxVols_[make_pair(Market::defaultConfiguration, "JPYUSD")] = volatility;
     }
 };

 struct DiscreteAsianTestData {
     Option::Type type;
     Real spot;
     Real strike;
     Rate foreignRate;
     Rate domesticRate;
     Time firstFixing;
     Time length;
     Size fixings;
     Volatility volatility;
     Real expectedNPV;
 };

 } // namespace

 BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

 BOOST_AUTO_TEST_SUITE(FxAsianOptionTests)

 BOOST_AUTO_TEST_CASE(testFxAsianOptionTradeBuilding) {

     BOOST_TEST_MESSAGE("Testing FX Asian option trade building with constant vol term structure");

     // Data from "Asian Option", Levy, 1997 in "Exotic Options: The State of the Art",
     // edited by Clewlow, Strickland
     // Tests with > 100 fixings are skipped here for speed, QL already tests these
     std::vector<DiscreteAsianTestData> asians = {
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 2, 0.13, 1.3942835683},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 4, 0.13, 1.5852442983},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 8, 0.13, 1.66970673},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 12, 0.13, 1.6980019214},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 26, 0.13, 1.7255070456},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 52, 0.13, 1.7401553533},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 0.0, 11.0 / 12.0, 100, 0.13, 1.7478303712},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 2, 0.13, 1.8496053697},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 4, 0.13, 2.0111495205},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 8, 0.13, 2.0852138818},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 12, 0.13, 2.1105094397},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 26, 0.13, 2.1346526695},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 52, 0.13, 2.147489651},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 1.0 / 12.0, 11.0 / 12.0, 100, 0.13, 2.154728109},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 2, 0.13, 2.63315092584},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 4, 0.13, 2.76723962361},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 8, 0.13, 2.83124836881},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 12, 0.13, 2.84290301412},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 26, 0.13, 2.88179560417},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 52, 0.13, 2.88447044543},
         {Option::Put, 90.0, 87.0, 0.06, 0.025, 3.0 / 12.0, 11.0 / 12.0, 100, 0.13, 2.89985329603}};

     Date asof = Date(01, Feb, 2021);
     Envelope env("CP1");
     QuantLib::ext::shared_ptr<EngineFactory> engineFactory;
     QuantLib::ext::shared_ptr<Market> market;

     for (const auto& a : asians) {
         Time deltaT = a.length / (a.fixings - 1);
         Date expiry;
         vector<Date> fixingDates(a.fixings);
         vector<std::string> strFixingDates(a.fixings);
         for (Size i = 0; i < a.fixings; ++i) {
             fixingDates[i] = (asof + static_cast<Integer>((a.firstFixing + i * deltaT) * 360 + 0.5));
             strFixingDates[i] = to_string(fixingDates[i]);
         }
         expiry = fixingDates[a.fixings - 1];

         ScheduleDates scheduleDates("NullCalendar", "", "", strFixingDates);
         ScheduleData scheduleData(scheduleDates);

         market = QuantLib::ext::make_shared<TestMarket>(a.spot, expiry, a.domesticRate, a.foreignRate, a.volatility);
         QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
         std::string productName = "FxAsianOptionArithmeticPrice";
         engineData->model(productName) = "GarmanKohlhagen";
         engineData->engine(productName) = "MCDiscreteArithmeticAPEngine";
         engineData->engineParameters(productName) = {{"ProcessType", "Discrete"},    {"BrownianBridge", "True"},
                                                      {"AntitheticVariate", "False"}, {"ControlVariate", "True"},
                                                      {"RequiredSamples", "2047"},    {"Seed", "0"}};
         engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

         // Set evaluation date
         Settings::instance().evaluationDate() = market->asofDate();

         // Test the building of a FX Asian option doesn't throw
	 PremiumData premiumData;
         OptionData optionData("Long", to_string(a.type), "European", true, {to_string(expiry)}, "Cash", "",
			       premiumData,
                               vector<Real>(), vector<Real>(), "", "", "", vector<string>(), vector<string>(), "", "",
                               "", "Asian", "Arithmetic", boost::none, boost::none, boost::none);

         QuantLib::ext::shared_ptr<FxAsianOption> asianOption = QuantLib::ext::make_shared<FxAsianOption>(
             env, "FxAsianOption", 1.0, TradeStrike(a.strike, "USD"), optionData, scheduleData,
             QuantLib::ext::make_shared<FXUnderlying>("FX", "ECB-JPY-USD", 1.0), Date(), "USD");
         BOOST_CHECK_NO_THROW(asianOption->build(engineFactory));

         // Check the underlying instrument was built as expected
         QuantLib::ext::shared_ptr<Instrument> qlInstrument = asianOption->instrument()->qlInstrument();

         QuantLib::ext::shared_ptr<DiscreteAveragingAsianOption> discreteAsian =
             QuantLib::ext::dynamic_pointer_cast<DiscreteAveragingAsianOption>(qlInstrument);

         BOOST_CHECK(discreteAsian);
         BOOST_CHECK_EQUAL(discreteAsian->exercise()->type(), Exercise::Type::European);
         BOOST_CHECK_EQUAL(discreteAsian->exercise()->dates().size(), 1);
         BOOST_CHECK_EQUAL(discreteAsian->exercise()->dates()[0], expiry);

         QuantLib::ext::shared_ptr<TypePayoff> payoff = QuantLib::ext::dynamic_pointer_cast<TypePayoff>(discreteAsian->payoff());
         BOOST_CHECK(payoff);
         BOOST_CHECK_EQUAL(payoff->optionType(), a.type);

         Real expectedPrice = a.expectedNPV;

         // Check the price
         BOOST_CHECK_SMALL(asianOption->instrument()->NPV() - expectedPrice, 2e-2);
     }
 }

 BOOST_AUTO_TEST_CASE(testFxAsianOptionFromXml) {

     BOOST_TEST_MESSAGE("Testing parsing of FX Asian option trade from XML");

     // Create an XML string representation of the trade
     string tradeXml;
     tradeXml.append("<Portfolio>");
     tradeXml.append("  <Trade id=\"FxAsianOption_USDJPY\">");
     tradeXml.append("    <TradeType>FxAsianOption</TradeType>");
     tradeXml.append("    <Envelope>");
     tradeXml.append("      <CounterParty>CPTY_A</CounterParty>");
     tradeXml.append("      <NettingSetId>CPTY_A</NettingSetId>");
     tradeXml.append("      <AdditionalFields/>");
     tradeXml.append("    </Envelope>");
     tradeXml.append("    <FxAsianOptionData>");
     tradeXml.append("      <OptionData>");
     tradeXml.append("        <LongShort>Long</LongShort>");
     tradeXml.append("        <OptionType>Call</OptionType>");
     tradeXml.append("        <Style>European</Style>");
     tradeXml.append("        <Settlement>Cash</Settlement>");
     tradeXml.append("        <PayOffAtExpiry>false</PayOffAtExpiry>");
     tradeXml.append("        <PayoffType>Asian</PayoffType>");
     tradeXml.append("        <PayoffType2>Arithmetic</PayoffType2>");
     tradeXml.append("        <ExerciseDates>");
     tradeXml.append("          <ExerciseDate>2021-02-26</ExerciseDate>");
     tradeXml.append("        </ExerciseDates>");
     tradeXml.append("      </OptionData>");
     tradeXml.append("      <ObservationDates>");
     tradeXml.append("        <Dates>");
     tradeXml.append("          <Dates>");
     tradeXml.append("            <Date>2021-02-01</Date>");
     tradeXml.append("            <Date>2021-02-02</Date>");
     tradeXml.append("            <Date>2021-02-03</Date>");
     tradeXml.append("            <Date>2021-02-04</Date>");
     tradeXml.append("            <Date>2021-02-05</Date>");
     tradeXml.append("            <Date>2021-02-08</Date>");
     tradeXml.append("            <Date>2021-02-09</Date>");
     tradeXml.append("            <Date>2021-02-10</Date>");
     tradeXml.append("            <Date>2021-02-11</Date>");
     tradeXml.append("            <Date>2021-02-12</Date>");
     tradeXml.append("            <Date>2021-02-15</Date>");
     tradeXml.append("            <Date>2021-02-16</Date>");
     tradeXml.append("            <Date>2021-02-17</Date>");
     tradeXml.append("            <Date>2021-02-18</Date>");
     tradeXml.append("            <Date>2021-02-19</Date>");
     tradeXml.append("            <Date>2021-02-22</Date>");
     tradeXml.append("            <Date>2021-02-23</Date>");
     tradeXml.append("            <Date>2021-02-24</Date>");
     tradeXml.append("            <Date>2021-02-25</Date>");
     tradeXml.append("            <Date>2021-02-26</Date>");
     tradeXml.append("          </Dates>");
     tradeXml.append("        </Dates>");
     tradeXml.append("      </ObservationDates>");
     tradeXml.append("      <Name>FX-ECB-USD-JPY</Name>");
     tradeXml.append("      <Strike>104.6860</Strike>");
     tradeXml.append("      <Quantity>104.6860</Quantity>");
     tradeXml.append("    </FxAsianOptionData>");
     tradeXml.append("  </Trade>");
     tradeXml.append("</Portfolio>");

     // Load portfolio from XML string
     Portfolio portfolio;
     portfolio.fromXMLString(tradeXml);

     // Extract FxAsianOption trade from portfolio
     QuantLib::ext::shared_ptr<Trade> trade = portfolio.trades().begin()->second;
     QuantLib::ext::shared_ptr<FxAsianOption> option = QuantLib::ext::dynamic_pointer_cast<ore::data::FxAsianOption>(trade);
     BOOST_CHECK(option != nullptr);

     // Check fields after checking that the cast was successful
     BOOST_CHECK(option);
     BOOST_CHECK_EQUAL(option->tradeType(), "FxAsianOption");
     BOOST_CHECK_EQUAL(option->id(), "FxAsianOption_USDJPY");
     // BOOST_CHECK_EQUAL(option->asset(), "USD"); // only available after build
     BOOST_CHECK_EQUAL(option->quantity(), 104.6860);
     BOOST_CHECK_EQUAL(option->strike().value(), 104.6860);
     BOOST_CHECK_EQUAL(option->indexName(), "FX-ECB-USD-JPY");
     BOOST_CHECK_EQUAL(option->option().longShort(), "Long");
     BOOST_CHECK_EQUAL(option->option().callPut(), "Call");
     BOOST_CHECK_EQUAL(option->option().style(), "European");
     BOOST_CHECK_EQUAL(option->option().exerciseDates().size(), 1);
     BOOST_CHECK_EQUAL(option->option().exerciseDates()[0], "2021-02-26");
     BOOST_CHECK(option->observationDates().hasData());

     BOOST_CHECK_EQUAL(option->option().payoffType(), "Asian");
     BOOST_CHECK_EQUAL(option->option().payoffType2(), "Arithmetic");
 }

 BOOST_AUTO_TEST_SUITE_END()

 BOOST_AUTO_TEST_SUITE_END()
