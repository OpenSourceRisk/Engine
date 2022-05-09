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

/*! \file orea/app/structuredanalyticserror.hpp
    \brief Structured analytics error
    \ingroup app
*/

#pragma once

#include <orea/engine/observationmode.hpp>

#include <ql/currencies/exchangeratemanager.hpp>
#include <ql/experimental/commodities/commoditysettings.hpp>
#include <ql/math/randomnumbers/seedgenerator.hpp>
#include <ql/qldefines.hpp>
#include <ql/utilities/tracing.hpp>

namespace ore {
namespace analytics {

struct SingletonRemover {
    virtual ~SingletonRemover() {
#ifdef QL_ENABLE_SESSIONS
        ore::analytics::ObservationMode::remove();
        QuantLib::Settings::remove();
        QuantLib::IndexManager::remove();
        QuantLib::CommoditySettings::remove();
        QuantLib::ExchangeRateManager::remove();
        QuantLib::SeedGenerator::remove();
        QuantLib::detail::Tracing::remove();
        QuantLib::Money::Settings::remove();
        QuantLib::IborCoupon::Settings::remove();
        // observables hold a reference to ObservableSettings, so we can remove the instance ourselves
        // QuantLib::ObservableSettings::remove();
#endif
    }
};

} // namespace analytics
} // namespace ore
