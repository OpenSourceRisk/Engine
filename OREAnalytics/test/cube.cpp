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

#include "cube.hpp"
#include <boost/filesystem.hpp>
#include <orea/cube/inmemorycube.hpp>

using namespace ore::analytics;
using namespace boost::unit_test_framework;
using std::vector;
using std::string;

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

    // Check we can't get anything out of bounds
    BOOST_CHECK_THROW(cube.get(cube.numIds(), 0, 0), std::exception);
    BOOST_CHECK_THROW(cube.get(0, cube.numDates(), 0), std::exception);
    BOOST_CHECK_THROW(cube.get(0, 0, cube.samples()), std::exception);

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
}

namespace testsuite {

void CubeTest::testSinglePrecisionInMemoryCube() {
    // trades, dates, samples
    vector<string> ids(100, string("id")); // the overlap doesn't matter
    vector<Date> dates(100, Date());
    Size samples = 1000;
    SinglePrecisionInMemoryCube c(Date(), ids, dates, samples);
    testCube(c, "SinglePrecisionInMemoryCube", 1e-5);
}

void CubeTest::testDoublePrecisionInMemoryCube() {
    vector<string> ids(100, string("id")); // the overlap doesn't matter
    vector<Date> dates(100, Date());
    Size samples = 1000;
    DoublePrecisionInMemoryCube c(Date(), ids, dates, samples);
    testCube(c, "DoublePrecisionInMemoryCube", 1e-14);
}

void CubeTest::testSinglePrecisionInMemoryCubeN() {
    vector<string> ids(100, string("id"));
    vector<Date> dates(50, Date());
    Size samples = 200;
    Size depth = 6;
    SinglePrecisionInMemoryCubeN c(Date(), ids, dates, samples, depth);
    testCube(c, "SinglePrecisionInMemoryCubeN", 1e-5);
}

void CubeTest::testDoublePrecisionInMemoryCubeN() {
    vector<string> ids(100, string("id"));
    vector<Date> dates(50, Date());
    Size samples = 200;
    Size depth = 6;
    DoublePrecisionInMemoryCubeN c(Date(), ids, dates, samples, depth);
    testCube(c, "DoublePrecisionInMemoryCubeN", 1e-14);
}

void CubeTest::testDoublePrecisionInMemoryCubeFileIO() {
    vector<string> ids(100, string("id")); // the overlap doesn't matter
    Date d(1, QuantLib::Jan, 2016);        // need a real date here
    vector<Date> dates(100, d);
    Size samples = 1000;
    DoublePrecisionInMemoryCube c(d, ids, dates, samples);
    testCubeFileIO<DoublePrecisionInMemoryCube>(c, "DoublePrecisionInMemoryCube", 1e-14);
}

void CubeTest::testDoublePrecisionInMemoryCubeFileNIO() {
    vector<string> ids(100, string("id")); // the overlap doesn't matter
    Date d(1, QuantLib::Jan, 2016);        // need a real date here
    vector<Date> dates(50, d);
    Size samples = 200;
    Size depth = 6;
    DoublePrecisionInMemoryCubeN c(d, ids, dates, samples, depth);
    testCubeFileIO<DoublePrecisionInMemoryCubeN>(c, "DoublePrecisionInMemoryCubeN", 1e-14);
}

test_suite* CubeTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Cube tests");

    suite->add(BOOST_TEST_CASE(&CubeTest::testSinglePrecisionInMemoryCube));
    suite->add(BOOST_TEST_CASE(&CubeTest::testDoublePrecisionInMemoryCube));
    suite->add(BOOST_TEST_CASE(&CubeTest::testSinglePrecisionInMemoryCubeN));
    suite->add(BOOST_TEST_CASE(&CubeTest::testDoublePrecisionInMemoryCubeN));
    suite->add(BOOST_TEST_CASE(&CubeTest::testDoublePrecisionInMemoryCubeFileIO));
    suite->add(BOOST_TEST_CASE(&CubeTest::testDoublePrecisionInMemoryCubeFileNIO));

    return suite;
}
}
