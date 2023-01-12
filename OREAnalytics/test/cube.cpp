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
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>

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

template <class T> void testCubeFileIO(NPVCube& cube, const std::string& cubeName, Real tolerance) {

    initCube(cube);

    // get a random filename
    string filename = boost::filesystem::unique_path().string();
    BOOST_TEST_MESSAGE("Saving cube " << cubeName << " to file " << filename);
    cube.save(filename);

    // Create a new Cube and load it
    T* cube2 = new T();
    BOOST_TEST_MESSAGE("Loading from file " << filename);
    cube2->load(filename);
    BOOST_TEST_MESSAGE("Cube " << cubeName << " loaded from file.");

    // Delete the file to make sure all reads are from memory
    boost::filesystem::remove(filename);

    // Check dimensions match
    BOOST_CHECK_EQUAL(cube.numIds(), cube2->numIds());
    BOOST_CHECK_EQUAL(cube.numDates(), cube2->numDates());
    BOOST_CHECK_EQUAL(cube.samples(), cube2->samples());
    BOOST_CHECK_EQUAL(cube.depth(), cube2->depth());

    // check all values
    checkCube(*cube2, tolerance);
    // All done
    delete cube2;
}

void testCubeGetSetbyDateID(NPVCube& cube, Real tolerance) {
    std::map<string, Size> ids = cube.idAndIndex();
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

} // namespace

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
    DoublePrecisionInMemoryCube c(d, ids, dates, samples);
    testCubeFileIO<DoublePrecisionInMemoryCube>(c, "DoublePrecisionInMemoryCube", 1e-14);
}

BOOST_AUTO_TEST_CASE(testDoublePrecisionInMemoryCubeFileNIO) {
    std::set<string> ids{string("id")}; // the overlap doesn't matter
    Date d(1, QuantLib::Jan, 2016);        // need a real date here
    vector<Date> dates(50, d);
    Size samples = 200;
    Size depth = 6;
    DoublePrecisionInMemoryCubeN c(d, ids, dates, samples, depth);
    testCubeFileIO<DoublePrecisionInMemoryCubeN>(c, "DoublePrecisionInMemoryCubeN", 1e-14);
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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
