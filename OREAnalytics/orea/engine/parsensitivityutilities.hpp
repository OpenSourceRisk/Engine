/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

/*! \file orea/engine/parsensitivityanalysis.hpp
    \brief Perfrom sensitivity analysis for a given portfolio
    \ingroup simulation
*/

#pragma once

#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/scenario/scenario.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
namespace ore {
namespace analytics {

//! Computes the implied quote
Real impliedQuote(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& i);

//! true if key type and name are equal, do not care about the index though
bool riskFactorKeysAreSimilar(const ore::analytics::RiskFactorKey& x, const ore::analytics::RiskFactorKey& y);

double impliedVolatility(const QuantLib::CapFloor& cap, double targetValue,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& d, double guess,
                         QuantLib::VolatilityType type, double displacement);

double impliedVolatility(const QuantLib::YoYInflationCapFloor& cap, double targetValue,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& d, double guess,
                         QuantLib::VolatilityType type, double displacement,
                         const QuantLib::Handle<QuantLib::YoYInflationIndex>& index = {});

double impliedVolatility(const RiskFactorKey& key, const ParSensitivityInstrumentBuilder::Instruments& instruments);



} // namespace analytics
} // namespace ore
