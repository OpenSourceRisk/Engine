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
            for (auto const& [field, value] : ComputeEnvironment::instance().context().deviceInfo()) {
                BOOST_TEST_MESSAGE("      " << std::left << std::setw(30) << field << ": '" << value << "'");
            }
            BOOST_TEST_MESSAGE("      " << std::left << std::setw(30) << "supportsDoublePrecision"
                                        << ": " << std::boolalpha
                                        << ComputeEnvironment::instance().context().supportsDoublePrecision());
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

BOOST_AUTO_TEST_CASE(testSimpleCalcWithDoublePrecision) {
    ComputeEnvironmentFixture fixture;
    const std::size_t n = 1024;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing simple calc (double precision) on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();

        if (!c.supportsDoublePrecision()) {
            BOOST_TEST_MESSAGE("device does not support double precision - skipping the test for this device.");
            continue;
        }

        BOOST_TEST_MESSAGE("  do first calc");

        double dblPrecNumber = 1.29382757483823819;

        ComputeContext::Settings settings;
        settings.useDoublePrecision = true;
        auto [id, _] = c.initiateCalculation(n, 0, 0, settings);
        std::vector<double> rx(n, dblPrecNumber);
        auto x = c.createInputVariable(&rx[0]);
        auto y = c.createInputVariable(dblPrecNumber);
        auto z = c.applyOperation(RandomVariableOpCode::Add, {x, y});
        auto w = c.applyOperation(RandomVariableOpCode::Mult, {z, z});
        c.declareOutputVariable(w);
        std::vector<std::vector<double>> output(1, std::vector<double>(n));
        c.finalizeCalculation(output);
        for (auto const& v : output.front()) {
            BOOST_CHECK_CLOSE(v, (dblPrecNumber + dblPrecNumber) * (dblPrecNumber + dblPrecNumber), 1.0E-15);
        }

        BOOST_TEST_MESSAGE("  do second calc using same kernel");

        c.initiateCalculation(n, id, 0, settings);
        std::vector<double> rx2(n, dblPrecNumber);
        c.createInputVariable(&rx2[0]);
        c.createInputVariable(dblPrecNumber);
        std::vector<std::vector<double>> output2(1, std::vector<double>(n));
        c.finalizeCalculation(output2);
        for (auto const& v : output2.front()) {
            BOOST_CHECK_CLOSE(v, (dblPrecNumber + dblPrecNumber) * (dblPrecNumber + dblPrecNumber), 1.0E-15);
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

        ComputeContext::Settings settings;
        settings.debug = true;
        auto [id, _] = c.initiateCalculation(n, 0, 0, settings);
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

        c.initiateCalculation(n, id, 0);
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
        BOOST_CHECK_CLOSE(boost::accumulators::variance(acc), 1.0, 2.0);
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
        auto [id, _] = c.initiateCalculation(n, 0, 0);
        auto vs = c.createInputVariates(3, 2);
        for (auto const& d : vs) {
            for (auto const& r : d) {
                c.declareOutputVariable(r);
            }
        }
        std::vector<std::vector<double>> output(6, std::vector<double>(n));
        c.finalizeCalculation(output);
        outputTimings(c);
        checkRngOutput(output);

        BOOST_TEST_MESSAGE("test to replay same calc");
        auto [id2, newCalc2] = c.initiateCalculation(n, id, 0);
        BOOST_CHECK(!newCalc2);
        BOOST_CHECK_EQUAL(id, id2);
        c.finalizeCalculation(output);
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
        auto [id, newCalc] = c.initiateCalculation(n, 0, 0);
        BOOST_CHECK(newCalc);
        BOOST_CHECK(id > 0);
        auto v1 = c.createInputVariable(1.0);
        auto v2 = c.createInputVariable(1.0);
        c.finalizeCalculation(output);
        auto [id2, newCalc2] = c.initiateCalculation(n, id, 0);
        BOOST_CHECK(!newCalc2);
        BOOST_CHECK_EQUAL(id, id2);
        c.createInputVariable(1.0);
        c.createInputVariable(1.0);
        BOOST_CHECK_THROW(c.createInputVariates(1, 1), std::exception);
        BOOST_CHECK_THROW(c.applyOperation(RandomVariableOpCode::Add, {v1, v2}), std::exception);
        BOOST_CHECK_THROW(c.freeVariable(v1), std::exception);
        BOOST_CHECK_THROW(c.declareOutputVariable(v1), std::exception);
        BOOST_CHECK_NO_THROW(c.finalizeCalculation(output));
    }
}

BOOST_AUTO_TEST_CASE(testRngGenerationMt19937) {
    ComputeEnvironmentFixture fixture;
    const std::size_t n = 1500;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing rng generation mt19937 against QL on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();
        ComputeContext::Settings settings;
        settings.rngSequenceType = QuantExt::SequenceType::MersenneTwister;
        settings.useDoublePrecision = c.supportsDoublePrecision();
        BOOST_TEST_MESSAGE("using double precision = " << std::boolalpha << settings.useDoublePrecision);
        c.initiateCalculation(n, 0, 0, settings);
        auto vs = c.createInputVariates(1, 1);
        auto vs2 = c.createInputVariates(1, 1);
        for (auto const& d : vs) {
            for (auto const& r : d) {
                c.declareOutputVariable(r);
            }
        }
        for (auto const& d : vs2) {
            for (auto const& r : d) {
                c.declareOutputVariable(r);
            }
        }
        std::vector<std::vector<double>> output(2, std::vector<double>(n));
        c.finalizeCalculation(output);

        auto sg = GenericPseudoRandom<MersenneTwisterUniformRng, InverseCumulativeNormal>::make_sequence_generator(
            1, settings.rngSeed);
        MersenneTwisterUniformRng mt(settings.rngSeed);

        double tol = settings.useDoublePrecision ? 1E-12 : 1E-4;

        Size noErrors = 0, errorThreshold = 10;
        for (Size j = 0; j < 2; ++j) {
            for (Size i = 0; i < n; ++i) {
                Real ref = sg.nextSequence().value[0];
                Real err = std::abs(output[j][i] - ref);
                if (std::abs(ref) > 1E-10)
                    err /= std::abs(ref);
                if (err > tol && noErrors < errorThreshold) {
                    BOOST_ERROR("gpu value (" << output[j][i] << ") at j=" << j << ", i=" << i
                                              << " does not match cpu value (" << ref << "), error " << err << ", tol "
                                              << tol);
                    noErrors++;
                }
            }
        }
    }
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(testConditionalExpectation) {
    ComputeEnvironmentFixture fixture;
    const std::size_t n = 100;
    for (auto const& d : ComputeEnvironment::instance().getAvailableDevices()) {
        BOOST_TEST_MESSAGE("testing conditional expectation on device '" << d << "'.");
        ComputeEnvironment::instance().selectContext(d);
        auto& c = ComputeEnvironment::instance().context();
        ComputeContext::Settings settings;
        settings.useDoublePrecision = c.supportsDoublePrecision();
        BOOST_TEST_MESSAGE("using double precision = " << std::boolalpha << settings.useDoublePrecision);

        c.initiateCalculation(n, 0, 0, settings);

        auto one = c.createInputVariable(1.0);
        auto vs = c.createInputVariates(1, 2);
        auto ce = c.applyOperation(RandomVariableOpCode::ConditionalExpectation, {vs[0][0], one, vs[0][1]});

        for (auto const& d : vs) {
            for (auto const& r : d) {
                c.declareOutputVariable(r);
            }
        }
        c.declareOutputVariable(ce);

        std::vector<std::vector<double>> output(3, std::vector<double>(n));
        c.finalizeCalculation(output);

        RandomVariable y(output[0]);
        RandomVariable x(output[1]);
        RandomVariable z = conditionalExpectation(
            y, {&x}, multiPathBasisSystem(1, settings.regressionOrder, QuantLib::LsmBasisSystem::Monomial, x.size()));

        double tol = settings.useDoublePrecision ? 1E-12 : 1E-4;
        Size noErrors = 0, errorThreshold = 10;

        for (Size i = 0; i < n; ++i) {
            Real err = std::abs(output[2][i] - z[i]);
            if (std::abs(z[i]) > 1E-10)
                err /= std::abs(z[i]);
            if (err > tol && noErrors < errorThreshold) {
                BOOST_ERROR("gpu value (" << output[2][i] << ") at i=" << i << " does not match reference cpu value ("
                                          << z[i] << "), error " << err << ", tol " << tol);
                noErrors++;
            }
        }
    }
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
