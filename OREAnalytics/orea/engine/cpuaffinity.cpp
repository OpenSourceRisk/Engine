/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/engine/cpuaffinity.hpp>

#include <ored/utilities/log.hpp>

#include <algorithm>
#include <numeric>
#include <random>
#include <thread>

#ifdef ORE_MULTITHREADING_CPU_AFFINITY
#include <pthread.h>
#include <sched.h>
#endif

namespace ore {
namespace analytics {

#ifdef ORE_MULTITHREADING_CPU_AFFINITY

std::vector<std::size_t> getCpuIds(std::size_t nThreads, const std::string& logPrefix) {

    std::size_t nCPU = std::max(1U, std::thread::hardware_concurrency());
    WLOG(logPrefix << " Number of CPUs found: " << nCPU);

    std::vector<std::size_t> result(nThreads);

    std::mt19937 gen{std::random_device{}()};
    std::vector<std::size_t> availableCpus;

    for (std::size_t i = 0; i < nThreads; ++i) {
        if (availableCpus.empty()) {
            availableCpus.resize(nCPU);
            std::iota(availableCpus.begin(), availableCpus.end(), 0);
        }
        std::uniform_int_distribution<> distrib(0, availableCpus.size() - 1);
        auto pos = std::next(availableCpus.begin(), distrib(gen));
        result[i] = *pos;
        availableCpus.erase(pos);
    }

    for (std::size_t i = 0; i < nThreads; ++i) {
        WLOG(logPrefix << " Assigning thread " << i << " to CPU #" << result[i]);
    }
    return result;
}

void setThreadCpuAffinity(int id, const std::vector<std::size_t>& cpuIds, const std::string& logPrefix) {
    pthread_t self = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuIds[id], &cpuset);
    if (int rc = pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)) {
        WLOG(logPrefix << " Error while setting cpu affinity for thread " << id << " to cpu id " << cpuIds[id]
                       << ": got return code " << rc);
    } else {
        WLOG(logPrefix << " Setting cpu affinity for thread " << id << " to cpu id " << cpuIds[id]
                       << ", running on cpu " << sched_getcpu());
    }
}

#else

std::vector<std::size_t> getCpuIds(std::size_t, const std::string&) { return {}; }

void setThreadCpuAffinity(int, const std::vector<std::size_t>&, const std::string&) {}

#endif

} // namespace analytics
} // namespace ore
