/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/tradeutils.hpp
    \brief trade utility functions
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/instrumentwrapper.hpp>

#include <qle/instruments/multiccycompositeinstrument.hpp>

#include <ql/instruments/compositeinstrument.hpp>

namespace ore {
namespace data {

std::set<QuantLib::ext::shared_ptr<InstrumentWrapper>>
unpackCompositeInstrumentWrappers(const std::set<QuantLib::ext::shared_ptr<InstrumentWrapper>>& wrappers);

std::set<std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Real>> unpackCompositeInstruments(
    const std::set<std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Real>>& qlInstruments);

} // namespace data
} // namespace ore
