/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/settings.hpp>
#include <qle/utilities/savedobservablesettings.hpp>

namespace ore::analytics {

class CleanUpThreadLocalSingletons {
public:
    QuantLib::SavedSettings savedSettings;
    QuantExt::SavedObservableSettings savedObservableSettings;
    ~CleanUpThreadLocalSingletons();
};

class CleanUpThreadGlobalSingletons {
public:
    ~CleanUpThreadGlobalSingletons();
};

class CleanUpLogSingleton {
public:
    CleanUpLogSingleton(const bool removeLoggers = false, const bool clearIndependentLoggers = true);
    ~CleanUpLogSingleton();

private:
    bool removeLoggers_, clearIndependentLoggers_;
};

} // namespace ore::analytics
