/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file durationadjustedcmslegbuilder.hpp
    \brief leg builder for duration adjusted cms coupon legs
    \ingroup portfolio
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/cashflow.hpp>

namespace ore {
namespace data {

class DurationAdjustedCmsLegBuilder : public ore::data::LegBuilder {
public:
    DurationAdjustedCmsLegBuilder() : LegBuilder(LegType::DurationAdjustedCMS) {}

    QuantLib::Leg buildLeg(const ore::data::LegData& data,
                           const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory,
                           ore::data::RequiredFixings& requiredFixings, const std::string& configuration,
                           const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                           const bool attachPricer = true,
                           std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngine =
                               nullptr) const override;
};

} // namespace data
} // namespace ore
