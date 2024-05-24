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

#include <orea/app/cleanupsingletons.hpp>
#include <orea/engine/observationmode.hpp>

#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/utilities/calendarparser.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>

#include <qle/math/computeenvironment.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/utilities/savedobservablesettings.hpp>

namespace ore::analytics {

CleanUpThreadLocalSingletons::~CleanUpThreadLocalSingletons() {
    QuantLib::IndexManager::instance().clearHistories();
    QuantExt::DividendManager::instance().clearHistories();
    ore::analytics::ObservationMode::instance().setMode(ore::analytics::ObservationMode::Mode::None);
    QuantExt::ComputeEnvironment::instance().reset();
    QuantExt::RandomVariableStats::instance().reset();
    QuantExt::McEngineStats::instance().reset();
}

CleanUpThreadGlobalSingletons::~CleanUpThreadGlobalSingletons() {
    ore::data::InstrumentConventions::instance().clear();
    ore::data::IndexNameTranslator::instance().clear();
    ore::data::CalendarParser::instance().reset();
    ore::data::CurrencyParser::instance().reset();
    ore::data::ScriptLibraryStorage::instance().clear();
}

CleanUpLogSingleton::CleanUpLogSingleton(const bool removeLoggers, const bool clearIndependentLoggers)
    : removeLoggers_(removeLoggers), clearIndependentLoggers_(clearIndependentLoggers) {}

CleanUpLogSingleton::~CleanUpLogSingleton() {
    if (clearIndependentLoggers_)
        ore::data::Log::instance().clearAllIndependentLoggers();
    if (removeLoggers_)
        ore::data::Log::instance().removeAllLoggers();
}

} // namespace ore::data
