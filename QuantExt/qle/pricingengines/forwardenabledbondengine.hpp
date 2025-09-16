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

#pragma once

#include <qle/instruments/cashflowresults.hpp>

#include <ql/instrument.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/pricingengine.hpp>
#include <ql/qldefines.hpp>
#include <ql/time/date.hpp>
#include <ql/cashflows/duration.hpp>

namespace QuantExt {

struct ForwardEnabledBondEngine {
    virtual ~ForwardEnabledBondEngine() {}
    // forwardNpv and settlement, do not call directly but via forwardPrice() function below
    std::pair<QuantLib::Real, QuantLib::Real> virtual forwardPrice(
        const QuantLib::Date& forwardNpvDate, const QuantLib::Date& settlementDate,
        const bool conditionalOnSurvival = true, std::vector<CashFlowResults>* const cfResults = nullptr,
        QuantLib::Leg* const expectedCashflows = nullptr) const = 0;
};

std::pair<QuantLib::Real, QuantLib::Real>
forwardPrice(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, const QuantLib::Date& forwardDate,
             const QuantLib::Date& settlementDate, const bool conditionalOnSurvival = true,
             std::vector<CashFlowResults>* const cfResults = nullptr, QuantLib::Leg* const expectedCashflows = nullptr);

QuantLib::Real yield(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, QuantLib::Real price,
                     const QuantLib::DayCounter& dayCounter, QuantLib::Compounding compounding,
                     QuantLib::Frequency frequency, QuantLib::Date forwardDate = QuantLib::Date(),
                     QuantLib::Date settlementDate = QuantLib::Date(), const bool conditionalOnSurvival = true,
                     QuantLib::Real accuracy = 1.0e-10, QuantLib::Size maxIterations = 100, QuantLib::Rate guess = 0.05,
                     QuantLib::Bond::Price::Type priceType = QuantLib::Bond::Price::Clean);

QuantLib::Real duration(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, QuantLib::Rate yield,
                        const QuantLib::DayCounter& dayCounter, QuantLib::Compounding compounding,
                        QuantLib::Frequency frequency, QuantLib::Duration::Type type = QuantLib::Duration::Modified,
                        QuantLib::Date forwardDate = QuantLib::Date(), QuantLib::Date settlementDate = QuantLib::Date(),
                        const bool conditionalOnSurvivial = true);

} // namespace QuantExt
