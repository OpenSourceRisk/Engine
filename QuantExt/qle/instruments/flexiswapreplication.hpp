/*
 Copyright (C) 2026 AcadiaSoft, Inc.

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

/*! \file qle/instruments/flexiswapreplication.hpp
    \brief Reference: F. Jamshidian, Replication of Flexi-swaps, January 2005
*/

#pragma once

#include <qle/instruments/multilegoption.hpp>

namespace QuantExt {

/*! Note: the passed legs should only be coupon legs, no notional legs. The replicating basket consists of
    constant notional swaptions. Inclusion of notional exchanges on exercise and at maturity are controlled
    by the flags generateNotionalExchangeOnExercise and generateFinalNotionalExchange.
    The passed legs will be unregistered with their coupon pricers. Therefore it is important that the
    legs passed into this method are not used in other contexts, where registration with the coupon pricer
    is important. */
std::vector<QuantLib::ext::shared_ptr<MultiLegOption>>
generateFlexiSwapReplication(const QuantLib::Date& referenceDate, const std::vector<QuantLib::Leg>& legs,
                             const std::vector<bool>& payer, const std::vector<QuantLib::Currency>& currency,
                             const std::vector<std::vector<QuantLib::Real>>& lowerNotionalBounds,
                             const bool generateNotionalExchangeOnExercise, const bool generateFinalNotionalExchange);

} // namespace QuantExt
