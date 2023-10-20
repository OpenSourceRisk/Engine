/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/math/computeenvironment.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_io.hpp>
#include <qle/math/randomvariable_opcodes.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

#include "toplevelfixture.hpp"

using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ComputeEnvironmentTest)

struct ComputeEnvironmentCleanUp {
    ComputeEnvironmentCleanUp() {}
    ~ComputeEnvironmentCleanUp() { ComputeEnvironment::instance().reset(); }
};

namespace {
void outputTimings(const ComputeContext& c) {
    BOOST_TEST_MESSAGE("  " << (double)c.debugInfo().numberOfOperations / c.debugInfo().nanoSecondsCalculation * 1.0E3
                            << " MFLOPS (raw)");
    BOOST_TEST_MESSAGE("  " << (double)c.debugInfo().numberOfOperations /
                                   (c.debugInfo().nanoSecondsCalculation + c.debugInfo().nanoSecondsDataCopy) * 1.0E3
                            << " MFLOPS (raw + data copy)");
    BOOST_TEST_MESSAGE("  " << (double)c.debugInfo().numberOfOperations /
                                   (c.debugInfo().nanoSecondsCalculation + c.debugInfo().nanoSecondsDataCopy +
                                    c.debugInfo().nanoSecondsProgramBuild) *
                                   1.0E3
                            << " MFLOPS (raw + data copy + program build)");
}
} // namespace

BOOST_AUTO_TEST_CASE(testEnvironmentInit) {
    BOOST_TEST_MESSAGE("testing enviroment initialization");
    ComputeEnvironmentCleanUp cleanUp;
    auto init = []() {
        for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
            ComputeEnvironment::instance().selectContext(d);
            ComputeEnvironment::instance().context().init();
            BOOST_TEST_MESSAGE("  device '" << d << "' initialized.");
        }
    };
    BOOST_CHECK_NO_THROW(init());
}

BOOST_AUTO_TEST_CASE(testSimpleCalc) {
    ComputeEnvironmentCleanUp cleanUp;
    const std::size_t n = 1024;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing simple calc on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();

        BOOST_TEST_MESSAGE("  do first calc");

        auto [id, _] = c.initiateCalculation(n);
        std::vector<double> rx(n, 4.0);
        auto x = c.createInputVariable(&rx[0]);
        auto y = c.createInputVariable(3.0);
        auto z = c.applyOperation(RandomVariableOpCode::Add, {x, y});
        auto w = c.applyOperation(RandomVariableOpCode::Mult, {z, z});
        c.declareOutputVariable(w);
        std::vector<std::vector<double>> output(1, std::vector<double>(n));
        c.finalizeCalculation(output);
        for (auto const& v : output.front()) {
            BOOST_CHECK_CLOSE(v, 49.0, 1.0E-8);
        }

        BOOST_TEST_MESSAGE("  do second calc using same kernel");

        c.initiateCalculation(n, id, 0);
        std::vector<double> rx2(n, 5.0);
        c.createInputVariable(&rx2[0]);
        c.createInputVariable(1.0);
        std::vector<std::vector<double>> output2(1, std::vector<double>(n));
        c.finalizeCalculation(output2);
        for (auto const& v : output2.front()) {
            BOOST_CHECK_CLOSE(v, 36.0, 1.0E-8);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLargeCalc) {
    ComputeEnvironmentCleanUp cleanUp;

    const std::size_t n = 65536;
    const std::size_t m = 1024;

    std::vector<double> results;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing large calc on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();
        std::vector<std::size_t> values(m);
        std::vector<double> data(n, 0.9f);
        std::vector<std::vector<double>> output(1, std::vector<double>(n));

        // first calc

        auto [id, _] = c.initiateCalculation(n, 0, 0, true);
        values[0] = c.createInputVariable(&data[0]);
        std::size_t val = values[0];
        for (std::size_t i = 0; i < m; ++i) {
            std::size_t val2 = c.applyOperation(RandomVariableOpCode::Add, {val, values[0]});
            std::size_t val3 = c.applyOperation(RandomVariableOpCode::Mult, {val2, values[0]});
            // c.freeVariable(val);
            // c.freeVariable(val2);
            val = val3;
        }
        c.declareOutputVariable(val);
        c.finalizeCalculation(output);
        BOOST_TEST_MESSAGE("  first calculation result = " << output.front()[0]);
        results.push_back(output.front()[0]);

        // second calculation

        c.initiateCalculation(n, id, 0, true);
        values[0] = c.createInputVariable(&data[0]);
        c.finalizeCalculation(output);
        BOOST_TEST_MESSAGE("  second calculation result = " << output.front()[0]);
        results.push_back(output.front()[0]);

        outputTimings(c);
    }

    std::vector<RandomVariable> values(m);
    for (std::size_t i = 0; i < m; ++i) {
        values[i] = RandomVariable(n, 0.9);
        values[i].expand();
    }
    RandomVariable res = values[0];
    boost::timer::nanosecond_type t1;
    boost::timer::cpu_timer timer;
    for (std::size_t i = 0; i < m; ++i) {
        res += values[0];
        res *= values[0];
    }
    t1 = timer.elapsed().wall;
    BOOST_TEST_MESSAGE("  testing large calc locally (benchmark)");
    BOOST_TEST_MESSAGE("  result = " << res.at(0));
    BOOST_TEST_MESSAGE("  " << 2.0 * (double)m * (double)n / (double)(t1)*1.0E3 << " MFlops");

    for (auto const& r : results) {
        BOOST_CHECK_CLOSE(res.at(0), r, 1E-3);
    }
}

BOOST_AUTO_TEST_CASE(testRngGeneration) {
    ComputeEnvironmentCleanUp cleanUp;
    const std::size_t n = 65536;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing rng generation on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();
        c.initiateCalculation(n, 0, 0, true);
        auto vs = c.createInputVariates(3, 2, 42);
        for (auto const& d : vs) {
            for (auto const& r : d) {
                c.declareOutputVariable(r);
            }
        }
        std::vector<std::vector<double>> output(6, std::vector<double>(n));
        c.finalizeCalculation(output);
        outputTimings(c);
        for (auto const& o : output) {
            boost::accumulators::accumulator_set<
                double, boost::accumulators::stats<boost::accumulators::tag::mean, boost::accumulators::tag::variance>>
                acc;
            for (auto const& v : o) {
                acc(v);
            }
            BOOST_TEST_MESSAGE("  mean = " << boost::accumulators::mean(acc)
                                           << ", variance = " << boost::accumulators::variance(acc));
            BOOST_CHECK_SMALL(boost::accumulators::mean(acc), 0.05);
            BOOST_CHECK_CLOSE(boost::accumulators::variance(acc), 1.0, 1.0);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
