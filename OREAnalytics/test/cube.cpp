/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/jaggedcube.hpp>
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/engine/riskfilter.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/engine/sensitivityfilestream.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <orea/engine/sensitivityrecord.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <orea/app/reportwriter.hpp>

#include "testmarket.hpp"

using namespace ore::analytics;
using namespace boost::unit_test_framework;
using std::string;
using std::vector;

namespace {

void initCube(NPVCube& cube) {
    // Set every (i,j,k,d) node to be i*1000000 + j + (k/1000000) + d*3
    for (Size i = 0; i < cube.numIds(); ++i) {
        for (Size j = 0; j < cube.numDates(); ++j) {
            for (Size k = 0; k < cube.samples(); ++k) {
                for (Size d = 0; d < cube.depth(); ++d) {
                    cube.set(i * 1000000.0 + j + k / 1000000.0 + d * 3, i, j, k, d);
                }
            }
        }
    }
}

void checkCube(NPVCube& cube, Real tolerance) {
    // Now check each value
    for (Size i = 0; i < cube.numIds(); ++i) {
        for (Size j = 0; j < cube.numDates(); ++j) {
            for (Size k = 0; k < cube.samples(); ++k) {
                for (Size d = 0; d < cube.depth(); ++d) {
                    Real expected = i * 1000000.0 + j + k / 1000000.0 + d * 3;
                    Real actual = cube.get(i, j, k, d);
                    BOOST_CHECK_CLOSE(expected, actual, tolerance);
                }
            }
        }
    }
}

void testCube(NPVCube& cube, const std::string& cubeName, Real tolerance) {
    BOOST_TEST_MESSAGE("Testing cube " << cubeName);

    initCube(cube);

    // Check we can't set anything out of bounds
    BOOST_CHECK_THROW(cube.set(1.0, cube.numIds(), 0, 0), std::exception);
    BOOST_CHECK_THROW(cube.set(1.0, 0, cube.numDates(), 0), std::exception);
    BOOST_CHECK_THROW(cube.set(1.0, 0, 0, cube.samples()), std::exception);
    BOOST_CHECK_THROW(cube.set(1.0, "test_id", Date::todaysDate(), 0), std::exception);

    // Check we can't get anything out of bounds
    BOOST_CHECK_THROW(cube.get(cube.numIds(), 0, 0), std::exception);
    BOOST_CHECK_THROW(cube.get(0, cube.numDates(), 0), std::exception);
    BOOST_CHECK_THROW(cube.get(0, 0, cube.samples()), std::exception);
    BOOST_CHECK_THROW(cube.get("test_id", Date::todaysDate(), 0), std::exception);

    checkCube(cube, tolerance);
    // All done
}

template <class T>
void testCubeFileIO(QuantLib::ext::shared_ptr<NPVCube> cube, const std::string& cubeName, Real tolerance,
                    bool doublePrecision) {

    initCube(*cube);

    // get a random filename
    string filename = boost::filesystem::unique_path().string();
    BOOST_TEST_MESSAGE("Saving cube " << cubeName << " to file " << filename);
    saveCube(filename, NPVCubeWithMetaData{cube, nullptr, boost::none, boost::none}, doublePrecision);

    // Create a new Cube and load it
    BOOST_TEST_MESSAGE("Loading from file " << filename);
    auto cube2 = loadCube(filename, doublePrecision).cube;
    BOOST_TEST_MESSAGE("Cube " << cubeName << " loaded from file.");

    // Delete the file to make sure all reads are from memory
    boost::filesystem::remove(filename);

    // Check dimensions match
    BOOST_CHECK_EQUAL(cube->numIds(), cube->numIds());
    BOOST_CHECK_EQUAL(cube->numDates(), cube->numDates());
    BOOST_CHECK_EQUAL(cube->samples(), cube->samples());
    BOOST_CHECK_EQUAL(cube->depth(), cube->depth());

    // check all values
    checkCube(*cube2, tolerance);
}

void testCubeGetSetbyDateID(NPVCube& cube, Real tolerance) {
    std::map<string, Size> ids = cube.idsAndIndexes();
    vector<Date> dates = cube.dates();
    // set value for each cube entry
    Real i = 1.0;
    for (auto [id, pos] : ids) {
        for (auto dt : dates) {
            cube.set(i, id, dt, 0);
            i++;
        }
    }
    // check the cube returns as expected
    Real j = 1.0;
    for (const auto& [id, pos]: ids) {
        for (auto dt : dates) {
            Real actual = cube.get(id, dt, 0);
            BOOST_CHECK_CLOSE(j, actual, tolerance);
            j++;
        }
    }
}

void initCube(NPVCube& cube, QuantLib::ext::shared_ptr<Portfolio>& portfolio, QuantLib::ext::shared_ptr<DateGrid>& dg) {
    // Set every (i,j,k,d) node to be i*1000000 + j + (k/1000000) + d*3
    for (const auto& [id, i]: cube.idsAndIndexes()) {
        Size dateLen = 0;
        Date tradeMaturity = portfolio->get(id)->maturity();
        while (dateLen < dg->dates().size() && dg->dates()[dateLen] < tradeMaturity) {
            dateLen++;
        }
        for (Size j = 0; j < dateLen; ++j) {
            for (Size k = 0; k < cube.samples(); ++k) {
                for (Size d = 0; d < cube.depth(); ++d) {
                    cube.set(i * 1000000.0 + j + k / 1000000.0 + d * 3, i, j, d, k);
                }
            }
        }
    }
}

void checkCube(NPVCube& cube, Real tolerance, QuantLib::ext::shared_ptr<Portfolio>& portfolio,
               QuantLib::ext::shared_ptr<DateGrid>& dg) {
    // Now check each value
    for (const auto& [id, i] : cube.idsAndIndexes()) {
        Size dateLen = 0;
        Date tradeMaturity = portfolio->get(id)->maturity();
        while (dateLen < dg->dates().size() && dg->dates()[dateLen] < tradeMaturity) {
            dateLen++;
        }
        for (Size j = 0; j < dateLen; ++j) {
            for (Size k = 0; k < cube.samples(); ++k) {
                for (Size d = 0; d < cube.depth(); ++d) {
                    Real expected = i * 1000000.0 + j + k / 1000000.0 + d * 3;
                    Real actual = cube.get(i, j, d, k);
                    BOOST_CHECK_CLOSE(expected, actual, tolerance);
                }
            }
        }
    }
}

void testCube(NPVCube& cube, const std::string& cubeName, Real tolerance, QuantLib::ext::shared_ptr<Portfolio>& portfolio,
              QuantLib::ext::shared_ptr<DateGrid>& d) {
    BOOST_TEST_MESSAGE("Testing cube " << cubeName);
    initCube(cube, portfolio, d);
    // Check we can't set anything out of bounds
    BOOST_CHECK_THROW(cube.set(1.0, cube.numIds(), 0, 0), std::exception);
    BOOST_CHECK_THROW(cube.set(1.0, 0, cube.numDates(), 0), std::exception);
    BOOST_CHECK_THROW(cube.set(1.0, 0, 0, cube.samples()), std::exception);

    // Check we can't get anything out of bounds
    BOOST_CHECK_THROW(cube.get(cube.numIds(), 0, 0), std::exception);
    BOOST_CHECK_THROW(cube.get(0, cube.numDates(), 0), std::exception);
    BOOST_CHECK_THROW(cube.get(0, 0, cube.samples()), std::exception);
    checkCube(cube, tolerance, portfolio, d);
    // All done
}

template <class T>
void testCubeFileIO(QuantLib::ext::shared_ptr<NPVCube> cube, const std::string& cubeName, Real tolerance,
                    QuantLib::ext::shared_ptr<Portfolio>& portfolio, QuantLib::ext::shared_ptr<DateGrid>& d, bool doublePrecision) {

    initCube(*cube, portfolio, d);

    // get a random filename
    string filename = boost::filesystem::unique_path().string();
    BOOST_TEST_MESSAGE("Saving cube " << cubeName << " to file " << filename);
    saveCube(filename, NPVCubeWithMetaData{cube, nullptr, boost::none, boost::none}, doublePrecision);

    // Create a new Cube and load it
    auto cube2 = QuantLib::ext::make_shared<T>();
    BOOST_TEST_MESSAGE("Loading from file " << filename);
    cube2 = loadCube(filename, doublePrecision).cube;
    BOOST_TEST_MESSAGE("Cube " << cubeName << " loaded from file.");

    // Delete the file to make sure all reads are from memory
    boost::filesystem::remove(filename);

    // Check dimensions match
    BOOST_CHECK_EQUAL(cube->numIds(), cube2->numIds());
    BOOST_CHECK_EQUAL(cube->numDates(), cube2->numDates());
    BOOST_CHECK_EQUAL(cube->samples(), cube2->samples());
    BOOST_CHECK_EQUAL(cube->depth(), cube2->depth());

    // check all values
    checkCube(*cube2, tolerance, portfolio, d);
}

} // namespace

// Returns an int in the interval [min, max]. Inclusive.
inline unsigned long randInt(MersenneTwisterUniformRng& rng, Size min, Size max) {
    return min + (rng.nextInt32() % (max + 1 - min));
}

inline const string& randString(MersenneTwisterUniformRng& rng, const vector<string>& strs) {
    return strs[randInt(rng, 0, strs.size() - 1)];
}
inline bool randBoolean(MersenneTwisterUniformRng& rng) { return randInt(rng, 0, 1) == 1; }

QuantLib::ext::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());

    vector<string> ccys = {"EUR", "USD", "GBP", "JPY", "CHF"};

    map<string, vector<string>> indices = {{"EUR", {"EUR-EURIBOR-6M"}},
                                           {"USD", {"USD-LIBOR-3M"}},
                                           {"GBP", {"GBP-LIBOR-6M"}},
                                           {"CHF", {"CHF-LIBOR-6M"}},
                                           {"JPY", {"JPY-LIBOR-6M"}}};

    vector<string> fixedTenors = {"6M", "1Y"};

    Size minTerm = 2;
    Size maxTerm = 30;

    Size minFixedBps = 10;
    Size maxFixedBps = 400;

    Size seed = 5; // keep this constant to ensure portfolio doesn't change
    MersenneTwisterUniformRng rng(seed);

    Date today = Settings::instance().evaluationDate();
    Calendar cal = TARGET();
    string calStr = "TARGET";
    string conv = "MF";
    string rule = "Forward";
    Size days = 2;
    string fixDC = "30/360";
    string floatDC = "ACT/365";

    vector<double> notional(1, 1000000);
    vector<double> spread(1, 0);

    for (Size i = 0; i < portfolioSize; i++) {
        Size term = portfolioSize == 1 ? 20 : randInt(rng, minTerm, maxTerm);

        // Start today +/- 1 Year
        Date startDate = portfolioSize == 1 ? cal.adjust(today) : cal.adjust(today - 365 + randInt(rng, 0, 730));
        Date endDate = cal.adjust(startDate + term * Years);

        // date 2 string
        std::ostringstream oss;
        oss << io::iso_date(startDate);
        string start(oss.str());
        oss.str("");
        oss.clear();
        oss << io::iso_date(endDate);
        string end(oss.str());

        // ccy + index
        string ccy = portfolioSize == 1 ? "EUR" : randString(rng, ccys);
        string index = portfolioSize == 1 ? "EUR-EURIBOR-6M" : randString(rng, indices[ccy]);
        string floatFreq = portfolioSize == 1 ? "6M" : index.substr(index.find('-', 4) + 1);

        // fixed details
        Real fixedRate = portfolioSize == 1 ? 0.02 : randInt(rng, minFixedBps, maxFixedBps) / 100.0;
        string fixFreq = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);

        // envelope
        Envelope env("CP");

        // Schedules
        ScheduleData floatSchedule(ScheduleRules(start, end, floatFreq, calStr, conv, conv, rule));
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixFreq, calStr, conv, conv, rule));

        bool isPayer = randBoolean(rng);

        // fixed Leg - with dummy rate
        LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy, fixedSchedule,
                         fixDC, notional);

        // float Leg
        vector<double> spreads(1, 0);
        LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spread), !isPayer, ccy,
                            floatSchedule, floatDC, notional);

        QuantLib::ext::shared_ptr<Trade> swap(new ore::data::Swap(env, floatingLeg, fixedLeg));

        // id
        oss.clear();
        oss.str("");
        oss << "Trade_" << i + 1;
        swap->id() = oss.str();

        portfolio->add(swap);
    }
    // portfolio->save("port.xml");

    portfolio->build(factory);

    if (portfolio->size() != portfolioSize)
        BOOST_ERROR("Failed to build portfolio (got " << portfolio->size() << " expected " << portfolioSize << ")");

    // Dump stats about portfolio
    Time maturity = 0;
    DayCounter dc = ActualActual(ActualActual::ISDA);
    map<string, Size> fixedFreqs;
    map<string, Size> floatFreqs;
    for (const auto&  [tradeId, trade] : portfolio->trades()) {
        maturity += dc.yearFraction(today, trade->maturity());

        // fixed Freq
        QuantLib::ext::shared_ptr<ore::data::Swap> swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
        string floatFreq = swap->legData()[0].schedule().rules().front().tenor();
        string fixFreq = swap->legData()[1].schedule().rules().front().tenor();
        QL_REQUIRE(swap->legData()[0].legType() == "Floating" && swap->legData()[1].legType() == "Fixed", "Leg mixup");
        if (fixedFreqs.find(fixFreq) == fixedFreqs.end())
            fixedFreqs[fixFreq] = 1;
        else
            fixedFreqs[fixFreq]++;
        if (floatFreqs.find(floatFreq) == floatFreqs.end())
            floatFreqs[floatFreq] = 1;
        else
            floatFreqs[floatFreq]++;
    }
    maturity /= portfolioSize;
    BOOST_TEST_MESSAGE("Portfolio Size    : " << portfolioSize);
    BOOST_TEST_MESSAGE("Average Maturity  : " << maturity);
    std::ostringstream oss;
    for (Size i = 0; i < ccys.size(); i++)
        oss << ccys[i] << " ";
    BOOST_TEST_MESSAGE("Currencies        : " << oss.str());
    // dump % breakdown of tenors
    map<string, Size>::iterator it;
    BOOST_TEST_MESSAGE("Fixed Tenors      : ");
    for (it = fixedFreqs.begin(); it != fixedFreqs.end(); ++it) {
        Real perc = 100 * it->second / (Real)portfolioSize;
        BOOST_TEST_MESSAGE("  " << it->first << "  " << perc << " %");
    }
    BOOST_TEST_MESSAGE("Floating Tenors   : ");
    for (it = floatFreqs.begin(); it != floatFreqs.end(); ++it) {
        Real perc = 100 * it->second / (Real)portfolioSize;
        BOOST_TEST_MESSAGE("  " << it->first << "  " << perc << " %");
    }

    return portfolio;
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(CubeTest)

BOOST_AUTO_TEST_CASE(testSinglePrecisionInMemoryCube) {
    // trades, dates, samples
    std::set<string> ids {string("id")}; // the overlap doesn't matter
    vector<Date> dates(100, Date());
    Size samples = 1000;
    SinglePrecisionInMemoryCube c(Date(), ids, dates, samples);
    testCube(c, "SinglePrecisionInMemoryCube", 1e-5);
}

BOOST_AUTO_TEST_CASE(testDoublePrecisionInMemoryCube) {
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    vector<Date> dates(100, Date());
    Size samples = 1000;
    DoublePrecisionInMemoryCube c(Date(), ids, dates, samples);
    testCube(c, "DoublePrecisionInMemoryCube", 1e-14);
}

BOOST_AUTO_TEST_CASE(testSinglePrecisionInMemoryCubeN) {
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    vector<Date> dates(50, Date());
    Size samples = 200;
    Size depth = 6;
    SinglePrecisionInMemoryCubeN c(Date(), ids, dates, samples, depth);
    testCube(c, "SinglePrecisionInMemoryCubeN", 1e-5);
}

BOOST_AUTO_TEST_CASE(testDoublePrecisionInMemoryCubeN) {
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    vector<Date> dates(50, Date());
    Size samples = 200;
    Size depth = 6;
    DoublePrecisionInMemoryCubeN c(Date(), ids, dates, samples, depth);
    testCube(c, "DoublePrecisionInMemoryCubeN", 1e-14);
}

BOOST_AUTO_TEST_CASE(testDoublePrecisionInMemoryCubeFileIO) {
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    Date d(1, QuantLib::Jan, 2016);        // need a real date here
    vector<Date> dates(100, d);
    Size samples = 1000;
    auto c = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(d, ids, dates, samples);
    testCubeFileIO<DoublePrecisionInMemoryCube>(c, "DoublePrecisionInMemoryCube", 1e-14, true);
}

BOOST_AUTO_TEST_CASE(testDoublePrecisionInMemoryCubeFileNIO) {
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    Date d(1, QuantLib::Jan, 2016);        // need a real date here
    vector<Date> dates(50, d);
    Size samples = 200;
    Size depth = 6;
    auto c = QuantLib::ext::make_shared<DoublePrecisionInMemoryCubeN>(d, ids, dates, samples, depth);
    testCubeFileIO<DoublePrecisionInMemoryCubeN>(c, "DoublePrecisionInMemoryCubeN", 1e-14, true);
}

BOOST_AUTO_TEST_CASE(testInMemoryCubeGetSetbyDateID) {
    std::set<string> ids = {"id1", "id2", "id3"}; // the overlap doesn't matter
    Date today = Date::todaysDate();
    vector<Date> dates = {today + QuantLib::Period(1, QuantLib::Days), today + QuantLib::Period(2, QuantLib::Days),
                          today + QuantLib::Period(3, QuantLib::Days)};
    Size samples = 1;
    DoublePrecisionInMemoryCube cube(Date(), ids, dates, samples);
    testCubeGetSetbyDateID(cube, 1e-14);
}

BOOST_AUTO_TEST_CASE(testSinglePrecisionJaggedCube) {

    SavedSettings backup;

    Size portfolioSize = 100;
    Size depth = 10;

    Date today = Date(15, December, 2016);
    Settings::instance().evaluationDate() = today;
    string dateGridStr = "270,2W";
    QuantLib::ext::shared_ptr<DateGrid> d = QuantLib::ext::make_shared<DateGrid>(dateGridStr);

    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();

    // Init Market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<testsuite::TestMarket>(today);

    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";

    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, initMarket);

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize, factory);

    Size samples = 10;
    JaggedCube<float> jaggedCube(today, portfolio, d->dates(), samples, depth);
    testCube(jaggedCube, "SinglePrecisionJaggedCube", 1e-5, portfolio, d);
    IndexManager::instance().clearHistories();
}

BOOST_AUTO_TEST_CASE(testDoublePrecisionJaggedCube) {

    SavedSettings backup;

    Size portfolioSize = 100;
    Size depth = 10;

    Date today = Date(15, December, 2016);
    Settings::instance().evaluationDate() = today;
    string dateGridStr = "270,2W";
    QuantLib::ext::shared_ptr<DateGrid> d = QuantLib::ext::make_shared<DateGrid>(dateGridStr);

    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();

    // Init Market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<testsuite::TestMarket>(today);

    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";

    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, initMarket);

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize, factory);

    Size samples = 10;
    JaggedCube<double> jaggedCube(today, portfolio, d->dates(), samples, depth);
    testCube(jaggedCube, "DoublePrecisionJaggedCube", 1e-5, portfolio, d);
    IndexManager::instance().clearHistories();
}

string writeCube(const QuantLib::ext::shared_ptr<NPVCube>& cube, Size bufferSize) {
    auto report = QuantLib::ext::make_shared<InMemoryReport>(bufferSize);
    ReportWriter().writeCube(*report, cube);
    string fileName = boost::filesystem::unique_path().string();
    report->toFile(fileName);
    return fileName;
}

void diffFiles(string filename1, string filename2) {
    std::ifstream ifs1(filename1);
    std::ifstream ifs2(filename2);

    std::istream_iterator<char> b1(ifs1), e1;
    std::istream_iterator<char> b2(ifs2), e2;

    BOOST_CHECK_EQUAL_COLLECTIONS(b1, e1, b2, e2);
}

// Test the functionality of class InMemoryReport to cache data on disk
BOOST_AUTO_TEST_CASE(testInMemoryReportBuffer) {

    // Generate a cube
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    vector<Date> dates(50, Date());
    Size samples = 200;
    Size depth = 6;
    auto c = QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(Date(), ids, dates, samples, depth);

    // From the cube, generate multiple copies of the report, each of which which will have ~60K rows.
    // Specify different values for the buffer size in InMemoryReport:
    string filename_0 = writeCube(c, 0);            // no buffering
    string filename_100 = writeCube(c, 100);
    string filename_1000 = writeCube(c, 1000);
    string filename_10000 = writeCube(c, 10000);
    string filename_100000 = writeCube(c, 100000);  // buffer size > report size, resulting in no buffering

    // Verify that buffering generates the same output as no buffering
    diffFiles(filename_0, filename_100);
    diffFiles(filename_0, filename_1000);
    diffFiles(filename_0, filename_10000);
    diffFiles(filename_0, filename_100000);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
