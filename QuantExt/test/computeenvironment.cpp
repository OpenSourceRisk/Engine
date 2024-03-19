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

#include <qle/math/basiccpuenvironment.hpp>
#include <qle/math/computeenvironment.hpp>
#include <qle/math/openclenvironment.hpp>
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

struct ComputeEnvironmentFixture {
    ComputeEnvironmentFixture() {
        QuantExt::ComputeFrameworkRegistry::instance().add(
            "OpenCL", &QuantExt::createComputeFrameworkCreator<QuantExt::OpenClFramework>, true);
        QuantExt::ComputeFrameworkRegistry::instance().add(
            "BasicCpu", &QuantExt::createComputeFrameworkCreator<QuantExt::BasicCpuFramework>, true);
    }
    ~ComputeEnvironmentFixture() { ComputeEnvironment::instance().reset(); }
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
    ComputeEnvironmentFixture fixture;
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
    ComputeEnvironmentFixture fixture;
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
        c.finalizeCalculation(output, {});
        for (auto const& v : output.front()) {
            BOOST_CHECK_CLOSE(v, 49.0, 1.0E-8);
        }

        BOOST_TEST_MESSAGE("  do second calc using same kernel");

        c.initiateCalculation(n, id, 0);
        std::vector<double> rx2(n, 5.0);
        c.createInputVariable(&rx2[0]);
        c.createInputVariable(1.0);
        std::vector<std::vector<double>> output2(1, std::vector<double>(n));
        c.finalizeCalculation(output2, {});
        for (auto const& v : output2.front()) {
            BOOST_CHECK_CLOSE(v, 36.0, 1.0E-8);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLargeCalc) {
    ComputeEnvironmentFixture fixture;

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
        c.finalizeCalculation(output, {});
        BOOST_TEST_MESSAGE("  first calculation result = " << output.front()[0]);
        results.push_back(output.front()[0]);

        // second calculation

        c.initiateCalculation(n, id, 0, true);
        values[0] = c.createInputVariable(&data[0]);
        c.finalizeCalculation(output, {});
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
    BOOST_TEST_MESSAGE("  " << 2.0 * (double)m * (double)n / (double)(t1) * 1.0E3 << " MFlops");

    for (auto const& r : results) {
        BOOST_CHECK_CLOSE(res.at(0), r, 1E-3);
    }
}

namespace {
void checkRngOutput(const std::vector<std::vector<double>>& output) {
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
} // namespace

BOOST_AUTO_TEST_CASE(testRngGeneration) {
    ComputeEnvironmentFixture fixture;
    const std::size_t n = 65536;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing rng generation on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();
        auto [id, _] = c.initiateCalculation(n, 0, 0, true);
        auto vs = c.createInputVariates(3, 2, 42);
        for (auto const& d : vs) {
            for (auto const& r : d) {
                c.declareOutputVariable(r);
            }
        }
        std::vector<std::vector<double>> output(6, std::vector<double>(n));
        c.finalizeCalculation(output, {});
        outputTimings(c);
        checkRngOutput(output);

        BOOST_TEST_MESSAGE("test to replay same calc");
        auto [id2, newCalc2] = c.initiateCalculation(n, id, 0, false);
        BOOST_CHECK(!newCalc2);
        BOOST_CHECK_EQUAL(id, id2);
        c.finalizeCalculation(output, {});
        outputTimings(c);
        checkRngOutput(output);
    }
}

BOOST_AUTO_TEST_CASE(testReplayFlowError) {
    ComputeEnvironmentFixture fixture;
    const std::size_t n = 42;
    std::vector<std::vector<double>> output;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing replay flow error on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();
        auto [id, newCalc] = c.initiateCalculation(n, 0, 0, true);
        BOOST_CHECK(newCalc);
        BOOST_CHECK(id > 0);
        auto v1 = c.createInputVariable(1.0);
        auto v2 = c.createInputVariable(1.0);
        c.finalizeCalculation(output, {});
        auto [id2, newCalc2] = c.initiateCalculation(n, id, 0, true);
        BOOST_CHECK(!newCalc2);
        BOOST_CHECK_EQUAL(id, id2);
        c.createInputVariable(1.0);
        c.createInputVariable(1.0);
        BOOST_CHECK_THROW(c.createInputVariates(1, 1, 42), std::exception);
        BOOST_CHECK_THROW(c.applyOperation(RandomVariableOpCode::Add, {v1, v2}), std::exception);
        BOOST_CHECK_THROW(c.freeVariable(v1), std::exception);
        BOOST_CHECK_THROW(c.declareOutputVariable(v1), std::exception);
        BOOST_CHECK_NO_THROW(c.finalizeCalculation(output, {}));
    }
}

// The code below replicates the behavior of one iteration of one of our static backtests
// when executed on our GPGPU Framework.
namespace {
double externalAverage(const std::vector<double>& v) {
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::mean>> acc;
    for (auto const& x : v)
        acc(x);
    return boost::accumulators::mean(acc);
}
struct S0 {
    std::size_t randomVariableOpCode;
    std::vector<std::size_t> args;
    std::vector<bool> vb;
};
void runBacktest(const std::string &d, const std::size_t id, const std::vector<S0>& v0, double expected) {

    BOOST_TEST_MESSAGE("testing backtest iteration on device '" << d << "'.");
    ComputeEnvironment::instance().selectContext(d);

    const std::size_t n = 10000;
    std::size_t externalCalculationId_ = 0;
    std::size_t cgVersion_ = 1;
    bool newExternalCalc = false;
    std::tie(externalCalculationId_, newExternalCalc) =
        ComputeEnvironment::instance().context().initiateCalculation(n, externalCalculationId_, cgVersion_);
    constexpr double max = std::numeric_limits<float>::max();

    std::vector<double> v1 = {
        -56340,   -39240,    -37560,  -37380,  -35460,  -30960,  -26220,  -26160,  -24920,    -24060,      -22820,
        -22680,   -20640,    -18780,  -18540,  -17480,  -16680,  -15960,  -15380,  -15120,    -14100,      -13380,
        -13140,   -13080,    -13020,  -12460,  -11120,  -10640,  -10320,  -9480,   -9400,     -9020,       -8920,
        -8760,    -8740,     -8540,   -7560,   -7300,   -7260,   -6820,   -6320,   -5560,     -5320,       -4820,
        -4700,    -4460,     -4380,   -3160,   -2,      -1,      -0.0939, -0.0654, -0.0623,   -0.0516,     -0.0437,
        -0.0378,  -0.0278,   -0.0266, -0.0235, -0.0223, -0.0219, -0.0158, 0,       0.003,     0.0075,      0.5,
        0.8434,   0.8719,    0.875,   0.8857,  0.8936,  0.8995,  0.9095,  0.9107,  0.9138,    0.915,       0.9154,
        0.9215,   0.9373,    0.9403,  0.9448,  1,       2,       3,       15,      30,        600,         1200,
        1500,     2100,      3000,    30000,   187460,  200000,  400000,  max,     0.0242777, 0.000589407, 0.99513,
        0.998575, -0.172332, 0.998575};

    for (const auto& x : v1)
        ComputeEnvironment::instance().context().createInputVariable(x);

    std::vector<std::vector<std::size_t>> rv = {{11}};
    auto gen = ComputeEnvironment::instance().context().createInputVariates(rv.size(), rv.front().size(), 42);

    for (const auto& s : v0) {
        ComputeEnvironment::instance().context().applyOperation(s.randomVariableOpCode, s.args);
        int i = 0;
        for (const auto& b : s.vb) {
            if (b)
                ComputeEnvironment::instance().context().freeVariable(s.args[i]);
            i++;
        }
    }
    ComputeEnvironment::instance().context().declareOutputVariable(id);

    std::vector<std::vector<double>> externalOutput_ = std::vector<std::vector<double>>(1, std::vector<double>(n));
    std::vector<double*> externalOutputPtr_ = std::vector<double*>(1, &externalOutput_.front()[0]);
    std::size_t regressionOrder = 2;
    ComputeEnvironment::instance().context().finalizeCalculation(externalOutputPtr_, {regressionOrder});
    auto baseNpv_ = externalAverage(externalOutput_[0]);
    BOOST_CHECK_CLOSE(baseNpv_, expected, 1E-3);
}
} // namespace

BOOST_AUTO_TEST_CASE(testBacktest) {

    ComputeEnvironmentFixture fixture;
    std::size_t id;
    double expected;
    std::vector<S0> v;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {

        if (d == "BasicCpu/Default/Default") {
            id = 78;
            expected = -39400;
            v = {
                {5, {99, 98}, {false, false}},
                {4, {65, 97}, {true, false}},
                {15, {103}, {true}},
                {3, {65}, {true}},
                {2, {103, 104}, {true, true}},
                {13, {100}, {false}},
                {4, {96, 102}, {false, true}},
                {1, {100, 103}, {false, true}},
                {1, {105, 65}, {true, true}},
                {13, {103}, {true}},
                {8, {65, 62}, {false, false}},
                {8, {65, 78}, {false, false}},
                {2, {81, 105}, {false, true}},
                {4, {103, 106}, {true, true}},
                {2, {65, 78}, {false, false}},
                {4, {94, 106}, {true, true}},
                {4, {105, 103}, {false, true}},
                {2, {81, 105}, {false, true}},
                {8, {65, 78}, {false, false}},
                {8, {65, 95}, {false, true}},
                {2, {81, 94}, {false, true}},
                {4, {105, 95}, {true, true}},
                {2, {65, 78}, {true, true}},
                {4, {93, 95}, {true, true}},
                {1, {106, 78}, {false, true}},
                {4, {94, 95}, {false, true}},
                {2, {81, 94}, {false, true}},
                {4, {95, 106}, {true, true}},
                {1, {78, 94}, {true, true}},
                {9, {106, 62}, {false, true}},
                {1, {89, 106}, {false, false}},
                {4, {94, 62}, {false, true}},
                {2, {81, 94}, {false, false}},
                {4, {62, 89}, {true, true}},
                {1, {78, 95}, {true, true}},
                {4, {94, 83}, {false, true}},
                {2, {81, 94}, {false, true}},
                {4, {83, 82}, {true, true}},
                {1, {95, 94}, {true, true}},
                {9, {89, 91}, {true, true}},
                {2, {81, 94}, {false, false}},
                {2, {81, 94}, {false, false}},
                {5, {81, 101}, {false, false}},
                {5, {81, 95}, {false, true}},
                {4, {106, 83}, {false, false}},
                {4, {94, 95}, {false, true}},
                {2, {81, 94}, {false, false}},
                {2, {81, 94}, {false, true}},
                {4, {106, 83}, {true, true}},
                {1, {78, 94}, {false, true}},
                {4, {62, 83}, {false, true}},
                {2, {81, 62}, {true, true}},
                {4, {83, 78}, {true, true}},
                {1, {94, 62}, {true, true}},
            };
        } else {
            id = 111;
            expected = -39438;
            v = {
                 {5, {99, 98}, {false, false}},
                 {4, {65, 97}, {true, false}},
                 {15, {103}, {true}},
                 {3, {105}, {true}},
                 {2, {103, 104}, {true, true}},
                 {13, {100}, {false}},
                 {4, {96, 102}, {false, true}},
                 {1, {100, 103}, {false, true}},
                 {1, {102, 105}, {true, true}},
                 {13, {103}, {true}},
                 {8, {105, 62}, {false, false}},
                 {8, {105, 78}, {false, false}},
                 {2, {81, 102}, {false, true}},
                 {4, {103, 106}, {true, true}},
                 {2, {105, 78}, {false, false}},
                 {4, {94, 106}, {true, true}},
                 {4, {102, 103}, {false, true}},
                 {2, {81, 102}, {false, true}},
                 {8, {105, 78}, {false, false}},
                 {8, {105, 95}, {false, true}},
                 {2, {81, 107}, {false, true}},
                 {4, {102, 108}, {true, true}},
                 {2, {105, 78}, {true, true}},
                 {4, {93, 108}, {true, true}},
                 {1, {106, 105}, {false, true}},
                 {4, {107, 108}, {false, true}},
                 {2, {81, 107}, {false, true}},
                 {4, {108, 106}, {true, true}},
                 {1, {105, 107}, {true, true}},
                 {9, {106, 62}, {false, true}},
                 {1, {89, 106}, {false, false}},
                 {4, {107, 105}, {false, true}},
                 {2, {81, 107}, {false, false}},
                 {4, {105, 89}, {true, true}},
                 {1, {108, 102}, {true, true}},
                 {4, {107, 83}, {false, true}},
                 {2, {81, 107}, {false, true}},
                 {4, {108, 82}, {true, true}},
                 {1, {102, 107}, {true, true}},
                 {9, {105, 91}, {true, true}},
                 {2, {81, 107}, {false, false}},
                 {2, {81, 107}, {false, false}},
                 {5, {81, 101}, {false, false}},
                 {5, {81, 109}, {false, true}},
                 {4, {106, 110}, {false, false}},
                 {4, {107, 109}, {false, true}},
                 {2, {81, 107}, {false, false}},
                 {2, {81, 107}, {false, true}},
                 {4, {106, 110}, {true, true}},
                 {1, {111, 107}, {false, true}},
                 {4, {112, 110}, {false, true}},
                 {2, {81, 112}, {true, true}},
                 {4, {110, 111}, {true, true}},
                 {1, {107, 112}, {true, true}},
             };        
        }
        runBacktest(d, id, v, expected);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
