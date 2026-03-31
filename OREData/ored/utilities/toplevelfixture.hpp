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

/*! \file ored/utilities/toplevelfixture.hpp
    \brief Fixture that can be used at top level of test suites
*/
#pragma once

#include <ql/indexes/indexmanager.hpp>
#include <ql/settings.hpp>
#include <qle/utilities/savedobservablesettings.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/calendarparser.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/databuilders.hpp>

namespace ore {
namespace data {

//! Top level fixture
class TopLevelFixture {
public:
    QuantLib::SavedSettings savedSettings;
    QuantExt::SavedObservableSettings savedObservableSettings;

    /*! Constructor: add things here that you want to happen at the start of every test case
    */
    TopLevelFixture() { ore::data::dataBuilders(); }

    /*! Destructor: add things here that you want to happen after every test case
    */
    virtual ~TopLevelFixture() {
        // Clear and fixings that have been added
        QuantLib::IndexManager::instance().clearHistories();
        // Clear conventions that have been set
        ore::data::InstrumentConventions::instance().setConventions(
            QuantLib::ext::make_shared<ore::data::Conventions>());
        // Clear contents of the index name translator
        ore::data::IndexNameTranslator::instance().clear();
        // Clear custom calendars and modified holidays
        ore::data::CalendarParser::instance().reset();
        // Clear custom currencies
        ore::data::CurrencyParser::instance().reset();
    }

    bool updatesEnabled() {
        return savedObservableSettings.updatesEnabled();
    }

    bool updatesDeferred() {
        return savedObservableSettings.updatesDeferred();
    }
};
} // namespace data
} // namespace ore
