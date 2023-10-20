/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#pragma once

#include <ql/patterns/observable.hpp>

namespace QuantExt {

// helper class to temporarily and safely change the settings
class SavedObservableSettings {
public:
    SavedObservableSettings() {
        updatesEnabled_ = QuantLib::ObservableSettings::instance().updatesEnabled();
        updatesDeferred_ = QuantLib::ObservableSettings::instance().updatesDeferred();
    }
    ~SavedObservableSettings() {
        // Restore observable settings
        if (updatesEnabled_)
            QuantLib::ObservableSettings::instance().enableUpdates();
        else
            QuantLib::ObservableSettings::instance().disableUpdates(updatesDeferred_);
    }

    bool updatesEnabled() const { return updatesEnabled_; }
    bool updatesDeferred() const { return updatesDeferred_; }

private:
    bool updatesEnabled_;
    bool updatesDeferred_;
};

} // namespace QuantExt
