/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Klaus Spanderen

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
 Examples:
  1. Start with 12 worker processes
     ./quantlib-test-suite --nProc=12 --log_level=message --report_level=short
                           --build_info=yes
  2. If parameter "--nProc" is omitted then the number
     of worker processes will be equal to the number of CPU cores.
 */


#pragma once

#include <ql/errors.hpp>
#include <ql/types.hpp>

#ifdef VERSION
/* This comes from ./configure, and for some reason it interferes with
   the internals of the unit test library in Boost 1.63. */
#undef VERSION
#endif

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/thread/thread.hpp>

#define BOOST_TEST_NO_MAIN 1
#include <boost/algorithm/string.hpp>
#include <boost/test/included/unit_test.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE "TestSuite"
#endif
using boost::unit_test::test_results;
using namespace boost::interprocess;
using namespace boost::unit_test_framework;

namespace {
int worker(std::string cmd) {
    std::cout << cmd << "\n";
    return std::system(cmd.c_str());
}

class TestCaseCollector : public test_tree_visitor {
public:
    typedef std::map<test_unit_id, std::list<test_unit_id>> id_map_t;

    const id_map_t& map() const { return idMap_; }
    test_unit_id testSuiteId() const { return testSuiteId_; }

    bool visit(test_unit const& tu) override {
        if (tu.p_parent_id == framework::master_test_suite().p_id) {
            testSuiteId_ = tu.p_id;
        } else if (tu.p_type == test_unit_type::TUT_SUITE && tu.p_parent_id == testSuiteId_) {
            idMap_[tu.p_parent_id].push_back(tu.p_id);
        }
        return test_tree_visitor::visit(tu);
    }

    std::list<test_unit_id>::size_type numberOfTests() {
        std::list<test_unit_id>::size_type n = 0;
        for (id_map_t::const_iterator p_it = idMap_.begin(); p_it != idMap_.end(); ++p_it)
            n += p_it->second.size();

        return n;
    }

private:
    id_map_t idMap_;
    test_unit_id testSuiteId_;
};

class TestCaseReportAggregator : public test_tree_visitor {
public:
    void test_suite_finish(test_suite const& ts) override {
        results_collect_helper ch(s_rc_impl().m_results_store[ts.p_id], ts);
        traverse_test_tree(ts, ch);
    }
};

struct TestCaseId {
    test_unit_id id;
    bool terminate;
};

struct RuntimeLog {
    QuantLib::Time time;
    char testCaseName[256];
};

struct QualifiedTestResults {
    test_unit_id id;
    test_results results;
};

} // namespace

test_suite* init_unit_test_suite(int, char*[]);

int main(int argc, char* argv[]) {
    typedef QuantLib::Time Time;
    boost::timer::cpu_timer testTimer;

    std::string moduleName = BOOST_TEST_MODULE;
    std::string profileFileNameStr = moduleName + "_unit_test_profile.txt";
    const char* const profileFileName = profileFileNameStr.c_str();
    std::string testUnitIdQueueNameStr = moduleName + "_test_unit_queue";
    const char* const testUnitIdQueueName = testUnitIdQueueNameStr.c_str();
    std::string testResultQueueNameStr = moduleName + "_test_result_queue";
    const char* const testResultQueueName = testResultQueueNameStr.c_str();
    std::string testRuntimeLogNameStr = moduleName + "_test_runtime_log_queue";
    const char* const testRuntimeLogName = testRuntimeLogNameStr.c_str();

    const std::string clientModeStr = "--client_mode=true";
    const bool clientMode = (std::string(argv[argc - 1]) == clientModeStr);

    message_queue::size_type recvd_size;

    try {
        unsigned int priority;
        if (!clientMode) {
            std::map<std::string, Time> runTimeLog;

            std::ifstream in(profileFileName);
            if (in.good()) {
                // NOLINTNEXTLINE(readability-implicit-bool-conversion)
                for (std::string line; std::getline(in, line);) {
                    std::vector<std::string> tok;
                    boost::split(tok, line, boost::is_any_of(":"));

                    QL_REQUIRE(tok.size() == 2, "every line should consists of two entries");
                    runTimeLog[tok[0]] = std::stod(tok[1]);
                }
            }
            in.close();

            auto nProc = std::thread::hardware_concurrency();

            std::stringstream cmd;
            cmd << "\"" << argv[0] << "\" ";

            std::vector<char*> localArgs(1, argv[0]);

            std::vector<std::string> logSink;
            for (int i = 1; i < argc; ++i) {
                const std::string arg(argv[i]);

                // check for number of processes
                std::vector<std::string> tok;
                boost::split(tok, arg, boost::is_any_of("="));
                if (tok.size() == 2 && tok[0] == "--nProc") {
                    nProc = std::stoul(tok[1]);
                } else if (tok[0] == "--log_sink") {
                    boost::split(logSink, tok[1], boost::is_any_of("."));
                    localArgs.push_back(argv[i]);
                } else if (arg != "--build_info=yes") {
                    cmd << arg << " ";
                    localArgs.push_back(argv[i]);
                }
            }

            std::cout << "nProc = " << nProc << "\n ";

            framework::init(init_unit_test_suite, localArgs.size(), &localArgs[0]);
            framework::finalize_setup_phase();

            framework::impl::s_frk_state().deduce_run_status(framework::master_test_suite().p_id);

            TestCaseCollector tcc;
            traverse_test_tree(framework::master_test_suite(), tcc, true);

            message_queue::remove(testUnitIdQueueName);
            message_queue mq(create_only, testUnitIdQueueName, tcc.numberOfTests() + nProc, sizeof(TestCaseId));

            message_queue::remove(testResultQueueName);
            message_queue rq(create_only, testResultQueueName, nProc, sizeof(QualifiedTestResults));

            message_queue::remove(testRuntimeLogName);
            message_queue lq(create_only, testRuntimeLogName, nProc, sizeof(RuntimeLog));

            // fork worker processes
            std::string cmdStr = cmd.str();
            std::vector<std::thread> threadGroup;
            std::vector<std::string> cmdStrs(nProc, cmdStr);
            for (unsigned i = 0; i < nProc; ++i) {
                std::stringstream newCmd;
                if (logSink.size() == 2) {
                    newCmd << "--log_sink=" << logSink[0] << "_" << i << "." << logSink[1] << " ";
                }
                newCmd << clientModeStr;
                cmdStrs[i] += newCmd.str();
            }

            for (unsigned i = 0; i < nProc; ++i) {
                threadGroup.push_back(std::thread(worker, cmdStrs[i]));
            }

            struct queue_remove {
                explicit queue_remove(const char* name) : name_(name) {}
                ~queue_remove() { message_queue::remove(name_); }

            private:
                const char* const name_;
            } queue_remover1(testUnitIdQueueName), queue_remover2(testResultQueueName),
                queue_remover3(testRuntimeLogName);

            std::multimap<Time, test_unit_id> testsSortedByRunTime;

            for (TestCaseCollector::id_map_t::const_iterator p_it = tcc.map().begin(); p_it != tcc.map().end();
                 ++p_it) {
               
                for (std::list<test_unit_id>::const_iterator it = p_it->second.begin(); it != p_it->second.end();
                        ++it) {

                    const std::string name = framework::get(*it, TUT_ANY).p_name;

                    if (runTimeLog.count(name) != 0u) {
                        testsSortedByRunTime.insert(std::make_pair(runTimeLog[name], *it));
                    } else {
                        testsSortedByRunTime.insert(std::make_pair((std::numeric_limits<Time>::max)(), *it));
                    }
                }
            }

            std::list<test_unit_id> ids;
            for (std::multimap<Time, test_unit_id>::const_iterator iter = testsSortedByRunTime.begin();
                 iter != testsSortedByRunTime.end(); ++iter) {

                ids.push_front(iter->second);
            }
            
            testsSortedByRunTime.clear();

            for (std::list<test_unit_id>::const_iterator iter = ids.begin(); iter != ids.end(); ++iter) {
                const TestCaseId id = {*iter, false};
                mq.send(&id, sizeof(TestCaseId), 0);
            }

            const TestCaseId id = {0, true};
            for (unsigned i = 0; i < nProc; ++i) {
                mq.send(&id, sizeof(TestCaseId), 0);
            }

            for (unsigned i = 0; i < ids.size(); ++i) {
                QualifiedTestResults remoteResults;

                rq.receive(&remoteResults, sizeof(QualifiedTestResults), recvd_size, priority);

                boost::unit_test::s_rc_impl().m_results_store[remoteResults.id] = remoteResults.results;
            }

            TestCaseReportAggregator tca;
            traverse_test_tree(framework::master_test_suite(), tca, true);

            results_reporter::make_report();

            RuntimeLog log;
            for (unsigned i = 0; i < ids.size(); ++i) {
                lq.receive(&log, sizeof(RuntimeLog), recvd_size, priority);
                runTimeLog[std::string(log.testCaseName)] = log.time;
            }

            std::ofstream out(profileFileName, std::ios::out | std::ios::trunc);
            out << std::setprecision(6);
            for (std::map<std::string, QuantLib::Time>::const_iterator iter = runTimeLog.begin();
                 iter != runTimeLog.end(); ++iter) {
                out << iter->first << ":" << iter->second << std::endl;
            }
            out.close();
            
            for (auto& thread : threadGroup) {
                thread.join();
            }

            testTimer.stop();
            double seconds = testTimer.elapsed().wall * 1e-9;
            int hours = int(seconds / 3600);
            seconds -= hours * 3600;
            int minutes = int(seconds / 60);
            seconds -= minutes * 60;
            std::cout << std::endl << BOOST_TEST_MODULE << " tests completed in ";
            if (hours > 0)
                std::cout << hours << " h ";
            if (hours > 0 || minutes > 0)
                std::cout << minutes << " m ";
            std::cout << std::fixed << std::setprecision(0) << seconds << " s" << std::endl;

        } else {
            framework::init(init_unit_test_suite, argc - 1, argv);
            framework::finalize_setup_phase();

            framework::impl::s_frk_state().deduce_run_status(framework::master_test_suite().p_id);

            message_queue mq(open_only, testUnitIdQueueName);

            TestCaseId id;
            mq.receive(&id, sizeof(TestCaseId), recvd_size, priority);

            typedef std::list<std::pair<std::string, QuantLib::Time>> run_time_list_type;
            run_time_list_type runTimeLogs;

            message_queue rq(open_only, testResultQueueName);

            while (!id.terminate) {
                auto startTime = std::chrono::steady_clock::now();

#if BOOST_VERSION < 106200
                BOOST_TEST_FOREACH(test_observer*, to, framework::impl::s_frk_state().m_observers)
                framework::impl::s_frk_state().m_aux_em.vexecute([&]() { to->test_start(1); });

                framework::impl::s_frk_state().execute_test_tree(id.id);

                BOOST_TEST_REVERSE_FOREACH(test_observer*, to, framework::impl::s_frk_state().m_observers)
                to->test_finish();
#else
                // works for BOOST_VERSION > 106100, needed for >106500
                framework::run(id.id, false);
#endif

                auto stopTime = std::chrono::steady_clock::now();
                double T = std::chrono::duration_cast<std::chrono::microseconds>(stopTime - startTime).count() * 1e-6;
                runTimeLogs.push_back(std::make_pair(framework::get(id.id, TUT_ANY).p_name, T));

                QualifiedTestResults results = {id.id, boost::unit_test::results_collector.results(id.id)};

                rq.send(&results, sizeof(QualifiedTestResults), 0);

                mq.receive(&id, sizeof(TestCaseId), recvd_size, priority);
            }

            RuntimeLog log;
            log.testCaseName[sizeof(log.testCaseName) - 1] = '\0';

            message_queue lq(open_only, testRuntimeLogName);
            for (run_time_list_type::const_iterator iter = runTimeLogs.begin(); iter != runTimeLogs.end(); ++iter) {
                log.time = iter->second;

                std::strncpy(log.testCaseName, iter->first.c_str(), sizeof(log.testCaseName) - 1);

                lq.send(&log, sizeof(RuntimeLog), 0);
            }
        }
    } catch (QuantLib::Error& ex) {
        std::cerr << "QuantLib exception: " << ex.what() << std::endl;
        return boost::exit_exception_failure;
    } catch (interprocess_exception& ex) {
        std::cerr << "interprocess exception: " << ex.what() << std::endl;
        return boost::exit_exception_failure;
    } catch (framework::nothing_to_test const&) {
        return boost::exit_success;
    } catch (framework::internal_error const& ex) {
        results_reporter::get_stream() << "Boost.Test framework internal error: " << ex.what() << std::endl;

        return boost::exit_exception_failure;
    } catch (framework::setup_error const& ex) {
        results_reporter::get_stream() << "Test setup error: " << ex.what() << std::endl;

        return boost::exit_exception_failure;
    } catch (...) {
        results_reporter::get_stream() << "Boost.Test framework internal error: unknown reason" << std::endl;

        return boost::exit_exception_failure;
    }

    framework::shutdown();

#if BOOST_VERSION < 106000
    return runtime_config::no_result_code()
#elif BOOST_VERSION < 106400
    // changed in Boost 1.60
    return !runtime_config::get<bool>(runtime_config::RESULT_CODE)
#else
    // changed again in Boost 1.64
    return !runtime_config::get<bool>(runtime_config::btrt_result_code)
#endif
               ? boost::exit_success
               : results_collector.results(framework::master_test_suite().p_id).result_code();
}
