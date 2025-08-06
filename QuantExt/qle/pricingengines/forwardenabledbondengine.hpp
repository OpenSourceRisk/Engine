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

#include <ql/pricingengine.hpp>
#include <ql/qldefines.hpp>
#include <ql/time/date.hpp>
#include <ql/instrument.hpp>

namespace QuantExt {

struct ForwardEnabledBondEngine {
    virtual ~ForwardEnabledBondEngine() {}
    // npv are always w.r.t. settlementDate, excluding flows between forward and settlement date
    QuantLib::Real virtual forwardPrice(const QuantLib::Date& forwardDate,
                                                  const QuantLib::Date& settlementDate, const bool clean = true,
                                                  const bool conditionalOnSurvival = true) const = 0;
};

QuantLib::Real forwardPrice(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument,
                            const QuantLib::Date& forwardDate, const QuantLib::Date& settlementDate,
                            const bool clean = true, const bool conditionalOnSurvival = true);

} // namespace QuantExt
