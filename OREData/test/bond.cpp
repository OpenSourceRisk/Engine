/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/indexparser.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using std::string;

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore::data;

// Bond test

namespace {

class TestMarket : public MarketImpl {
public:
    TestMarket() : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);
        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BANK_EUR_LEND")] =
            flatRateYts(0.02);
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = flatRateYts(0.02);
        defaultCurves_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] = flatRateDcs(0.0);
        // recoveryRates_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] =
        //     Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.00));
        securitySpreads_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.00));
        recoveryRates_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.00));
        // build ibor index
        Handle<IborIndex> hEUR(ore::data::parseIborIndex(
            "EUR-EURIBOR-6M", yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")]));
        iborIndices_[make_pair(Market::defaultConfiguration, "EUR-EURIBOR-6M")] = hEUR;

        // add Eurib 6M fixing
        hEUR->addFixing(Date(1, Feb, 2016), -0.00191);
        hEUR->addFixing(Date(1, Feb, 2017), -0.00191);
        hEUR->addFixing(Date(1, Feb, 2018), -0.00191);
        hEUR->addFixing(Date(1, Feb, 2019), -0.00191);
        hEUR->addFixing(Date(31, Jan, 2019), -0.00191);
        hEUR->addFixing(Date(30, Jan, 2020), -0.00191);
    }

    TestMarket(Real defaultFlatRate) : MarketImpl(false) {
        asof_ = Date(3, Feb, 2016);
        // build discount
        yieldCurves_[make_tuple(Market::defaultConfiguration, YieldCurveType::Yield, "BANK_EUR_LEND")] =
            flatRateYts(0.02);
        defaultCurves_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] = flatRateDcs(defaultFlatRate);
        // recoveryRates_[make_pair(Market::defaultConfiguration, "CreditCurve_A")] =
        //     Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.00));
        securitySpreads_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.00));
        recoveryRates_[make_pair(Market::defaultConfiguration, "Security1")] =
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.00));
    }

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        QuantLib::ext::shared_ptr<YieldTermStructure> yts(new FlatForward(0, NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        yts->enableExtrapolation();
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantExt::CreditCurve> flatRateDcs(Real forward) {
        QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> dcs(
            new FlatHazardRate(asof_, forward, ActualActual(ActualActual::ISDA)));
        return Handle<QuantExt::CreditCurve>(
            QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(dcs)));
    }
};

struct CommonVars {
    // global data
    string ccy;
    string securityId;
    string creditCurveId;
    string issuerId;
    string referenceCurveId;
    bool isPayer;
    string start;
    string end;
    string issue;
    string fixtenor;
    Calendar cal;
    string calStr;
    string conv;
    string rule;
    Size days;
    string fixDC;
    Real fixedRate;
    string settledays;
    bool isinarrears;
    Real notional;
    vector<Real> notionals;
    vector<Real> spread;

    // utilities
    QuantLib::ext::shared_ptr<ore::data::Bond> makeBond() {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        // build CMSSwap
        LegData fixedLegData(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy,
                             fixedSchedule, fixDC, notionals);

        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::Bond> bond(
            new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr,
                                              issue, fixedLegData)));
        return bond;
    }

    QuantLib::ext::shared_ptr<ore::data::Bond> makeAmortizingFixedBond(string amortType, Real value, bool underflow) {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        AmortizationData amortizationData(amortType, value, start, end, fixtenor, underflow);
        LegData fixedLegData(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy,
                             fixedSchedule, fixDC, notionals, vector<string>(), conv, false, false, false, true, "", 0,
                             "", {amortizationData});

        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::Bond> bond(
            new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr,
                                              issue, fixedLegData)));
        return bond;
    }

    QuantLib::ext::shared_ptr<ore::data::Bond> makeAmortizingFloatingBond(string amortType, Real value, bool underflow) {
        ScheduleData floatingSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        AmortizationData amortizationData(amortType, value, start, end, fixtenor, underflow);
        LegData floatingLegData(QuantLib::ext::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, spread), isPayer, ccy,
                                floatingSchedule, fixDC, notionals, vector<string>(), conv, false, false, false, true,
                                "", 0, "", {amortizationData});

        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::Bond> bond(
            new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr,
                                              issue, floatingLegData)));
        return bond;
    }

    QuantLib::ext::shared_ptr<ore::data::Bond> makeAmortizingFixedBondWithChangingAmortisation(string amortType1, Real value1,
                                                                                       bool underflow1, string end1,
                                                                                       string amortType2, Real value2,
                                                                                       bool underflow2) {
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        AmortizationData amortizationData1(amortType1, value1, start, end1, fixtenor, underflow1);
        AmortizationData amortizationData2(amortType2, value2, end1, end, fixtenor, underflow2);
        LegData fixedLegData(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy,
                             fixedSchedule, fixDC, notionals, vector<string>(), conv, false, false, false, true, "", 0,
                             "", {amortizationData1, amortizationData2});

        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::Bond> bond(
            new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr,
                                              issue, fixedLegData)));
        return bond;
    }

    QuantLib::ext::shared_ptr<ore::data::Bond>
    makeAmortizingFloatingBondWithChangingAmortisation(string amortType1, Real value1, bool underflow1, string end1,
                                                       string amortType2, Real value2, bool underflow2) {
        ScheduleData floatingSchedule(ScheduleRules(start, end, fixtenor, calStr, conv, conv, rule));

        FloatingLegData floatingLegRateData;
        AmortizationData amortizationData1(amortType1, value1, start, end1, fixtenor, underflow1);
        AmortizationData amortizationData2(amortType2, value2, end1, end, fixtenor, underflow2);
        LegData floatingLegData(QuantLib::ext::make_shared<FloatingLegData>("EUR-EURIBOR-6M", 2, false, spread), isPayer, ccy,
                                floatingSchedule, fixDC, notionals, vector<string>(), conv, false, false, false, true,
                                "", 0, "", {amortizationData1, amortizationData2});

        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::Bond> bond(
            new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr,
                                              issue, floatingLegData)));
        return bond;
    }

    QuantLib::ext::shared_ptr<ore::data::Bond> makeZeroBond() {
        Envelope env("CP1");

        QuantLib::ext::shared_ptr<ore::data::Bond> bond(
            new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settledays, calStr,
                                              notional, end, ccy, issue)));
        return bond;
    }

    CommonVars()
        : ccy("EUR"), securityId("Security1"), creditCurveId("CreditCurve_A"), issuerId("CPTY_A"),
          referenceCurveId("BANK_EUR_LEND"), isPayer(false), start("20160203"), end("20210203"), issue("20160203"),
          fixtenor("1Y") {
        cal = TARGET();
        calStr = "TARGET";
        conv = "MF";
        rule = "Forward";
        fixDC = "ACT/ACT";
        fixedRate = 0.05;
        settledays = "2";
        isinarrears = false;
        notional = 10000000;
        notionals.push_back(10000000);
        spread.push_back(0.0);
    }
};

// print details of bond cashflows
void printBondSchedule(const QuantLib::ext::shared_ptr<ore::data::Bond>& b) {
    auto qlInstr = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(b->instrument()->qlInstrument());
    BOOST_REQUIRE(qlInstr != nullptr);
    BOOST_TEST_MESSAGE("Bond NPV=" << qlInstr->NPV() << ", Schedule:");
    Leg l = qlInstr->cashflows();
    BOOST_TEST_MESSAGE(" StartDate    EndDate     Nominal        Rate      Amount");
    for (auto const& c : l) {
        auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c);
        if (cpn != nullptr) {
            BOOST_TEST_MESSAGE(QuantLib::io::iso_date(cpn->accrualStartDate())
                               << " " << QuantLib::io::iso_date(cpn->accrualEndDate()) << std::setw(12)
                               << cpn->nominal() << std::setw(12) << cpn->rate() << std::setw(12) << cpn->amount());
        } else {
            BOOST_TEST_MESSAGE("           " << QuantLib::io::iso_date(c->date()) << std::setw(12) << " "
                                             << std::setw(12) << " " << std::setw(12) << c->amount());
        }
    }
    BOOST_TEST_MESSAGE("");
}

// check nominal schedule of bond
void checkNominalSchedule(const QuantLib::ext::shared_ptr<ore::data::Bond>& b, const std::vector<Real> notionals) {
    auto qlInstr = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(b->instrument()->qlInstrument());
    BOOST_REQUIRE(qlInstr != nullptr);
    Leg l = qlInstr->cashflows();
    std::vector<Real> bondNotionals;
    for (auto const& c : l) {
        auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c);
        if (cpn != nullptr) {
            bondNotionals.push_back(cpn->nominal());
        }
    }
    BOOST_REQUIRE_EQUAL(bondNotionals.size(), notionals.size());
    for (Size i = 0; i < notionals.size(); ++i)
        BOOST_CHECK_CLOSE(bondNotionals[i], notionals[i], 1E-4);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BondTests)

BOOST_AUTO_TEST_CASE(testZeroBond) {
    BOOST_TEST_MESSAGE("Testing Zero Bond...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::Bond> bond = vars.makeZeroBond();

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    bond->build(engineFactory);

    Real npv = bond->instrument()->NPV();
    Real expectedNpv = 9048374.18;

    BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);
}

BOOST_AUTO_TEST_CASE(testAmortizingBond) {
    BOOST_TEST_MESSAGE("Testing Amortising Bonds...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Date today = Date(30, Jan, 2021);
    Settings::instance().evaluationDate() = today; // market->asofDate();

    CommonVars vars;
    vector<QuantLib::ext::shared_ptr<ore::data::Bond>> bonds;
    QuantLib::ext::shared_ptr<ore::data::Bond> bondFixedAmount = vars.makeAmortizingFixedBond("FixedAmount", 2500000, true);
    bonds.push_back(bondFixedAmount);

    QuantLib::ext::shared_ptr<ore::data::Bond> bondRelativeInitial =
        vars.makeAmortizingFixedBond("RelativeToInitialNotional", 0.25, true);
    bonds.push_back(bondRelativeInitial);

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    Real npvTol = 0.5;

    for (auto& b : bonds) {
        b->build(engineFactory);

        // output schedule
        printBondSchedule(b);

        Real npv = b->instrument()->NPV();
        Real expectedNpv = 0.0;

        BOOST_CHECK(std::fabs(npv - expectedNpv) < npvTol);
    }

    QuantLib::ext::shared_ptr<ore::data::Bond> bondRelativePrevious =
        vars.makeAmortizingFixedBond("RelativeToPreviousNotional", 0.25, true);
    bondRelativePrevious->build(engineFactory);
    printBondSchedule(bondRelativePrevious);

    QuantLib::ext::shared_ptr<QuantLib::Instrument> inst1 = bondRelativePrevious->instrument()->qlInstrument();
    QuantLib::ext::shared_ptr<QuantLib::Bond> qlBond1 = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(inst1);
    Real expectedNotional = 3164062.5;

    Real notional = qlBond1->notionals()[qlBond1->notionals().size() - 2];

    BOOST_CHECK_CLOSE(notional, expectedNotional, 1);

    QuantLib::ext::shared_ptr<ore::data::Bond> bondFixedAnnuity = vars.makeAmortizingFixedBond("Annuity", 2500000, true);
    bondFixedAnnuity->build(engineFactory);
    printBondSchedule(bondFixedAnnuity);

    QuantLib::ext::shared_ptr<QuantLib::Instrument> inst2 = bondFixedAnnuity->instrument()->qlInstrument();
    QuantLib::ext::shared_ptr<QuantLib::Bond> qlBond2 = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(inst2);
    expectedNotional = 1380908.447;

    notional = qlBond2->notionals()[qlBond2->notionals().size() - 2];

    BOOST_CHECK(std::fabs(notional - expectedNotional) < npvTol);

    QuantLib::ext::shared_ptr<ore::data::Bond> bondFloatingAnnuity = vars.makeAmortizingFloatingBond("Annuity", 2500000, true);
    bondFloatingAnnuity->build(engineFactory);
    printBondSchedule(bondFloatingAnnuity);

    QuantLib::ext::shared_ptr<QuantLib::Instrument> inst3 = bondFloatingAnnuity->instrument()->qlInstrument();
    QuantLib::ext::shared_ptr<QuantLib::Bond> qlBond3 = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(inst3);
    Real expectedAmount = 93.41;

    Real amount = qlBond3->cashflows()[qlBond3->cashflows().size() - 2]->amount();

    BOOST_CHECK(std::fabs(amount - expectedAmount) < npvTol);
}

BOOST_AUTO_TEST_CASE(testAmortizingBondWithChangingAmortisation) {
    BOOST_TEST_MESSAGE("Testing Amortising Bonds with changing amortisation...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Date today = Date(30, Jan, 2021);
    Settings::instance().evaluationDate() = today; // market->asofDate();

    // build engine factory
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // test different amortization combinations
    CommonVars vars;

    // fixed rate bond test cases
    QuantLib::ext::shared_ptr<ore::data::Bond> bond1 = vars.makeAmortizingFixedBondWithChangingAmortisation(
        "FixedAmount", 2500000, true, "05-02-2018", "FixedAmount", 1250000, true);
    bond1->build(engineFactory);
    printBondSchedule(bond1);
    checkNominalSchedule(bond1, {1.0E7, 7.5E6, 6.25E6, 5.0E6, 3.75E6});

    QuantLib::ext::shared_ptr<ore::data::Bond> bond2 = vars.makeAmortizingFixedBondWithChangingAmortisation(
        "FixedAmount", 2500000, true, "05-02-2018", "RelativeToInitialNotional", 0.1, true);
    bond2->build(engineFactory);
    printBondSchedule(bond2);
    checkNominalSchedule(bond2, {1.0E7, 7.5E6, 6.5E6, 5.5E6, 4.5E6});

    QuantLib::ext::shared_ptr<ore::data::Bond> bond3 = vars.makeAmortizingFixedBondWithChangingAmortisation(
        "RelativeToPreviousNotional", 0.1, true, "05-02-2018", "Annuity", 1E6, true);
    bond3->build(engineFactory);
    printBondSchedule(bond3);
    checkNominalSchedule(bond3, {1.0E7, 9.0E6, 8.45247E6, 7.87393E6, 7.26645E6});

    QuantLib::ext::shared_ptr<ore::data::Bond> bond4 = vars.makeAmortizingFixedBondWithChangingAmortisation(
        "Annuity", 1E6, true, "05-02-2018", "RelativeToPreviousNotional", 0.1, true);
    bond4->build(engineFactory);
    printBondSchedule(bond4);
    checkNominalSchedule(bond4, {1.0E7, 9.50012E6, 8.55011E6, 7.6951E6, 6.92559E6});

    // floating rate bond test cases
    QuantLib::ext::shared_ptr<ore::data::Bond> bond5 = vars.makeAmortizingFloatingBondWithChangingAmortisation(
        "FixedAmount", 2500000, true, "05-02-2018", "FixedAmount", 1250000, true);
    bond5->build(engineFactory);
    printBondSchedule(bond5);
    checkNominalSchedule(bond5, {1.0E7, 7.5E6, 6.25E6, 5.0E6, 3.75E6});

    QuantLib::ext::shared_ptr<ore::data::Bond> bond6 = vars.makeAmortizingFloatingBondWithChangingAmortisation(
        "FixedAmount", 2500000, true, "05-02-2018", "RelativeToInitialNotional", 0.1, true);
    bond6->build(engineFactory);
    printBondSchedule(bond6);
    checkNominalSchedule(bond6, {1.0E7, 7.5E6, 6.5E6, 5.5E6, 4.5E6});

    // annuity only allowed in single block setup
    QuantLib::ext::shared_ptr<ore::data::Bond> bond7 = vars.makeAmortizingFloatingBondWithChangingAmortisation(
        "RelativeToPreviousNotional", 0.1, true, "05-02-2018", "Annuity", 1E6, true);
    BOOST_CHECK_THROW(bond7->build(engineFactory), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testMultiPhaseBond) {
    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Date today = Date(30, Jan, 2021);
    Settings::instance().evaluationDate() = today; // market->asofDate();

    // build engine factory
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    // test multi phase bond
    CommonVars vars;
    ScheduleData schedule1(ScheduleRules("05-02-2016", "05-02-2018", "1Y", "TARGET", "F", "F", "Forward"));
    ScheduleData schedule2(ScheduleRules("05-02-2018", "05-02-2020", "6M", "TARGET", "F", "F", "Forward"));
    auto fixedLegRateData = QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, 0.01));
    LegData legdata1(fixedLegRateData, vars.isPayer, vars.ccy, schedule1, vars.fixDC, vars.notionals);
    LegData legdata2(fixedLegRateData, vars.isPayer, vars.ccy, schedule2, vars.fixDC, vars.notionals);
    Envelope env("CP1");
    QuantLib::ext::shared_ptr<ore::data::Bond> bond(
        new ore::data::Bond(env, BondData(vars.issuerId, vars.creditCurveId, vars.securityId, vars.referenceCurveId,
                                          vars.settledays, vars.calStr, vars.issue, {legdata1, legdata2})));
    bond->build(engineFactory);
    printBondSchedule(bond);
    auto qlInstr = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(bond->instrument()->qlInstrument());
    BOOST_REQUIRE(qlInstr != nullptr);
    // annually
    BOOST_REQUIRE_EQUAL(qlInstr->cashflows().size(), 7);
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[0]->date(), Date(6, Feb, 2017));
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[1]->date(), Date(5, Feb, 2018));
    // semi annually
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[2]->date(), Date(6, Aug, 2018));
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[3]->date(), Date(5, Feb, 2019));
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[4]->date(), Date(5, Aug, 2019));
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[5]->date(), Date(5, Feb, 2020));
    BOOST_CHECK_EQUAL(qlInstr->cashflows()[6]->date(), Date(5, Feb, 2020));
}

BOOST_AUTO_TEST_CASE(testBondZeroSpreadDefault) {
    BOOST_TEST_MESSAGE("Testing Bond price...");

    // build market
    QuantLib::ext::shared_ptr<Market> market = QuantLib::ext::make_shared<TestMarket>();
    Settings::instance().evaluationDate() = market->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::Bond> bond = vars.makeBond();

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";

    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    bond->build(engineFactory);

    Real npv = bond->instrument()->NPV();
    Real expectedNpv = 11403727.39;

    BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);
}

BOOST_AUTO_TEST_CASE(testBondCompareDefault) {
    BOOST_TEST_MESSAGE("Testing Bond price...");

    // build market
    QuantLib::ext::shared_ptr<Market> market1 = QuantLib::ext::make_shared<TestMarket>(0.0);
    QuantLib::ext::shared_ptr<Market> market2 = QuantLib::ext::make_shared<TestMarket>(0.5);
    QuantLib::ext::shared_ptr<Market> market3 = QuantLib::ext::make_shared<TestMarket>(0.99);
    Settings::instance().evaluationDate() = market1->asofDate();

    CommonVars vars;
    QuantLib::ext::shared_ptr<ore::data::Bond> bond = vars.makeBond();

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    map<string, string> engineparams;
    engineparams["TimestepPeriod"] = "6M";
    engineData->engineParameters("Bond") = engineparams;

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory1 = QuantLib::ext::make_shared<EngineFactory>(engineData, market1);
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory2 = QuantLib::ext::make_shared<EngineFactory>(engineData, market2);
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory3 = QuantLib::ext::make_shared<EngineFactory>(engineData, market3);

    bond->build(engineFactory1);
    Real npv1 = bond->instrument()->NPV();
    bond->build(engineFactory2);
    Real npv2 = bond->instrument()->NPV();
    bond->build(engineFactory3);
    Real npv3 = bond->instrument()->NPV();

    BOOST_CHECK((npv1 > npv2) && (npv2 > npv3));

    //   BOOST_CHECK_CLOSE(npv, expectedNpv, 1.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
