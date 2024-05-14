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

/*! \file qle/math/openclenvironment.hpp
    \brief opencl compute env implementation
*/

#pragma once

#include <qle/math/computeenvironment.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <map>

#ifdef ORE_ENABLE_OPENCL
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#endif

#define ORE_OPENCL_MAX_N_PLATFORMS 4U
#define ORE_OPENCL_MAX_N_DEVICES 8U

namespace QuantExt {

class OpenClFramework : public ComputeFramework {
public:
    OpenClFramework();
    ~OpenClFramework() override final;
    std::set<std::string> getAvailableDevices() const override final;
    ComputeContext* getContext(const std::string& deviceName) override final;

private:
    static void init();

    std::map<std::string, ComputeContext*> contexts_;

    static boost::shared_mutex mutex_;
    static bool initialized_;
#ifdef ORE_ENABLE_OPENCL
    static cl_uint nPlatforms_;
    static cl_uint nDevices_[ORE_OPENCL_MAX_N_PLATFORMS];
    static cl_device_id devices_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];
    static cl_context context_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];
#endif
    static std::string platformName_[ORE_OPENCL_MAX_N_PLATFORMS];
    static std::string deviceName_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];
    static std::vector<std::pair<std::string, std::string>> deviceInfo_[ORE_OPENCL_MAX_N_PLATFORMS]
                                                                       [ORE_OPENCL_MAX_N_DEVICES];
    static bool supportsDoublePrecision_[ORE_OPENCL_MAX_N_PLATFORMS][ORE_OPENCL_MAX_N_DEVICES];
};

} // namespace QuantExt
