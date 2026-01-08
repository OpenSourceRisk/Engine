/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/utilities/mcstats.hpp
    \brief rate helper related utilities.
*/

#include <boost/timer/timer.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/shared_ptr.hpp>

#pragma once
namespace QuantExt {
struct McEngineStats : public QuantLib::Singleton<McEngineStats> {
    McEngineStats() {
        other_timer.start();
        other_timer.stop();
        path_timer.stop();
        path_timer.start();
        calc_timer.start();
        calc_timer.stop();
    }

    void reset() {
        other_timer.stop();
        path_timer.stop();
        calc_timer.stop();
    }

    boost::timer::cpu_timer other_timer;
    boost::timer::cpu_timer path_timer;
    boost::timer::cpu_timer calc_timer;
};
} // namespace QuantExt