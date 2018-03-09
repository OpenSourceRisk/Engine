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

/*! \file engine/observationmode.hpp
    \brief Singleton class to hold global Observation Mode
    \ingroup utilities
*/

#pragma once

#include <ql/patterns/observable.hpp>

namespace ore {
namespace analytics {

//! The Global Observation setting
/*!
  This singleton is used in ORE to control the usage of the QuantLib::ObservableSettings
  \ingroup utilities
 */
class ObservationMode : public QuantLib::Singleton<ObservationMode> {
    friend class QuantLib::Singleton<ObservationMode>;

private:
    ObservationMode() : mode_(Mode::None) {}

public:
    //! Allowable mode mode
    enum class Mode { None, Disable, Defer, Unregister };

    Mode mode() { return mode_; }

    void setMode(Mode s) { mode_ = s; }

    void setMode(const std::string& s) {
        if (s == "None")
            mode_ = Mode::None;
        else if (s == "Disable")
            mode_ = Mode::Disable;
        else if (s == "Defer")
            mode_ = Mode::Defer;
        else if (s == "Unregister")
            mode_ = Mode::Unregister;
        else {
            QL_FAIL("Invalid ObserverMode string " << s);
        }
    }

private:
    Mode mode_;
};
} // namespace analytics
} // namespace ore
