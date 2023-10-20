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

#include <boost/algorithm/string/join.hpp>

#include <ql/errors.hpp>

namespace QuantExt {

ComputeEnvironment::ComputeEnvironment() { reset(); }

ComputeEnvironment::~ComputeEnvironment() { releaseFrameworks(); }

void ComputeEnvironment::releaseFrameworks() {
    for (auto& f : frameworks_)
        delete f;
    frameworks_.clear();
}

void ComputeEnvironment::reset() {
    currentContext_ = nullptr;
    currentContextDeviceName_.clear();
    releaseFrameworks();
    for (auto& c : ComputeFrameworkRegistry::instance().getAll())
        frameworks_.push_back(c());
}

std::set<std::string> ComputeEnvironment::getAvailableDevices() const {
    std::set<std::string> result;
    for (auto const& f : frameworks_) {
        auto tmp = f->getAvailableDevices();
        result.insert(tmp.begin(), tmp.end());
    }
    return result;
}

bool ComputeEnvironment::hasContext() const { return currentContext_ != nullptr; }

void ComputeEnvironment::selectContext(const std::string& deviceName) {
    if (currentContextDeviceName_ == deviceName)
        return;
    for (auto& f : frameworks_) {
        if (auto tmp = f->getAvailableDevices(); tmp.find(deviceName) != tmp.end()) {
            currentContext_ = f->getContext(deviceName);
            currentContext_->init();
            currentContextDeviceName_ = deviceName;
            return;
        }
    }
    QL_FAIL("ComputeEnvironment::selectContext(): device '"
            << deviceName << "' not found. Available devices: " << boost::join(getAvailableDevices(), ","));
}

ComputeContext& ComputeEnvironment::context() { return *currentContext_; }

void ComputeContext::finalizeCalculation(std::vector<std::vector<double>>& output) {
    std::vector<double*> outputPtr(output.size());
    std::transform(output.begin(), output.end(), outputPtr.begin(),
                   [](std::vector<double>& v) -> double* { return &v[0]; });
    finalizeCalculation(outputPtr);
}

void ComputeFrameworkRegistry::add(const std::string& name, std::function<ComputeFramework*(void)> creator,
                                   const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(allowOverwrite || std::find(names_.begin(), names_.end(), name) == names_.end(),
               "FrameworkRegistry::add(): creator for '"
                   << name << "' already exists and allowOverwrite is false, can't add it.");
    names_.push_back(name);
    creators_.push_back(creator);
}

const std::vector<std::function<ComputeFramework*(void)>>& ComputeFrameworkRegistry::getAll() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return creators_;
}

}; // namespace QuantExt
