/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/formulabasedlegbuilder.hpp
    \brief Formula based leg builder
    \ingroup portfolio
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/legbuilders.hpp>

#include <ql/cashflow.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

class FormulaBasedLegBuilder : public ore::data::LegBuilder {
public:
    explicit FormulaBasedLegBuilder() : LegBuilder("FormulaBased") {}
    QuantLib::Leg buildLeg(const ore::data::LegData& data,
                           const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory,
                           ore::data::RequiredFixings& requiredFixings, const std::string& configuration,
                           const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                           const bool useXbsCurves = false) const override;
};

} // namespace data
} // namespace ore
